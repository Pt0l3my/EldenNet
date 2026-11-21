// The local probes and the model list (ARCH §11.4): GETs on one transport, each read whole up to a bound, all ended
// together at one deadline or by the stop token.
#include "en/llm/probe.hpp"

#include <algorithm>
#include <functional>
#include <map>
#include <utility>

#include <nlohmann/json.hpp>

#include "en/llm/request.hpp"
#include "en/proto/json.hpp"

namespace en::llm {

using nlohmann::json;

namespace {

// The flavour checks' answers are small (llama.cpp's props carry its chat templates, some tens of KB); they are parsed
// whole, so they are held to en-proto's JSON caps (ARCH §10.2), well below a model list's bound.
constexpr std::size_t kMaxCheckBody = en::proto::kMaxJsonBytes;

std::string ms_text(Millis ms) { return std::to_string(ms.count()) + " ms"; }

std::string bytes_text(std::size_t n) {
  if (n >= (1u << 20) && n % (1u << 20) == 0) return std::to_string(n >> 20) + " MB";
  if (n >= (1u << 10) && n % (1u << 10) == 0) return std::to_string(n >> 10) + " KB";
  return std::to_string(n) + " bytes";
}

Millis slice_of(Millis requested) { return requested.count() > 0 ? requested : Millis(50); }

// Control characters (C0, DEL and C1, which UTF-8 writes as C2 80 to C2 9F) could act on a terminal or a screen that
// shows the id, so an id holding one is left out.
bool printable(std::string_view id) {
  for (std::size_t i = 0; i < id.size(); ++i) {
    const auto c = static_cast<unsigned char>(id[i]);
    if (c < 0x20 || c == 0x7F) return false;
    if (c == 0xC2 && i + 1 < id.size()) {
      const auto next = static_cast<unsigned char>(id[i + 1]);
      if (next >= 0x80 && next <= 0x9F) return false;
    }
  }
  return true;
}

// Reads a model list as it is parsed and keeps only the ids within the limits, so that memory stays at what is kept
// whatever a server sends (a DOM of many small entries would be many times the body's size).
class ModelListReader {
 public:
  explicit ModelListReader(const ModelListLimits& limits) : limits_(limits) {}

  bool null() { return value(Kind::Null, nullptr); }
  bool boolean(bool) { return value(Kind::Other, nullptr); }
  bool number_integer(json::number_integer_t) { return value(Kind::Other, nullptr); }
  bool number_unsigned(json::number_unsigned_t) { return value(Kind::Other, nullptr); }
  bool number_float(json::number_float_t, const json::string_t&) { return value(Kind::Other, nullptr); }
  bool string(json::string_t& text) { return value(Kind::String, &text); }
  bool binary(json::binary_t&) { return value(Kind::Other, nullptr); }
  bool start_object(std::size_t) { return open(Kind::Object); }
  bool start_array(std::size_t) { return open(Kind::Array); }

  bool key(json::string_t& name) {
    if (depth_ == 1) next_is_data_ = name == "data";
    if (in_entry_ && depth_ == 3) next_is_id_ = name == "id";
    return true;
  }

  bool end_object() {
    --depth_;
    if (in_entry_ && depth_ == 2) end_entry();
    return true;
  }

  bool end_array() {
    --depth_;
    if (in_data_ && depth_ == 1) in_data_ = false;
    return true;
  }

  bool parse_error(std::size_t, const std::string&, const json::exception&) { return false; }

  // The list, once the whole body has parsed.
  std::optional<ModelList> take() {
    if (!data_seen_ || (entries_ > 0 && !any_id_)) return std::nullopt;
    return std::move(list_);
  }

 private:
  enum class Kind { Null, String, Object, Array, Other };

  bool open(Kind kind) {
    if (!value(kind, nullptr)) return false;
    ++depth_;
    return true;
  }

  // Every value comes here first, at the depth of the container that holds it.
  bool value(Kind kind, json::string_t* text) {
    if (depth_ == 0) return kind == Kind::Object;  // the answer is an object, or no list
    if (depth_ == 1) {
      if (!next_is_data_) return true;
      next_is_data_ = false;
      if (data_seen_) return false;  // "data" twice: not a list we can trust
      data_seen_ = true;
      in_data_ = kind == Kind::Array;
      return kind == Kind::Array || kind == Kind::Null;  // null: how Ollama writes an empty list
    }
    if (in_data_ && depth_ == 2) {
      if (kind == Kind::Object) {
        in_entry_ = true;
        has_id_ = false;
        id_.reset();
      } else {
        ++entries_;  // an entry that is no object has no id
        ++list_.omitted;
      }
      return true;
    }
    if (in_entry_ && depth_ == 3 && next_is_id_) {
      next_is_id_ = false;
      has_id_ = kind == Kind::String && !text->empty();
      // A longer id is only noted: it is left out, never cut, so its text is not kept.
      if (has_id_ && text->size() <= limits_.max_id_bytes) {
        id_ = std::move(*text);
      } else {
        id_.reset();
      }
    }
    return true;
  }

  void end_entry() {
    in_entry_ = false;
    next_is_id_ = false;
    ++entries_;
    if (!has_id_) {
      ++list_.omitted;
      return;
    }
    any_id_ = true;
    if (!id_ || list_.ids.size() >= limits_.max_models || !printable(*id_)) {
      ++list_.omitted;
      return;
    }
    list_.ids.push_back(std::move(*id_));
  }

  const ModelListLimits& limits_;
  ModelList list_;
  int depth_ = 0;
  bool next_is_data_ = false;
  bool data_seen_ = false;
  bool in_data_ = false;
  bool in_entry_ = false;
  bool next_is_id_ = false;
  bool has_id_ = false;
  bool any_id_ = false;
  std::optional<std::string> id_;
  std::size_t entries_ = 0;
};

// Several GETs on one transport. Each is read whole, up to its bound; they all end together at one deadline or when a
// stop is requested, and whatever runs then is cancelled.
class Gets {
 public:
  struct Answer {
    int status = 0;          // 0 when no head came
    std::string body;        // a 2xx answer's body, whole; an error's body is never read
    bool failed = false;     // the exchange broke: `failure` has the transport's words, which name no address
    bool too_large = false;  // the body passed its bound, and the exchange was cancelled
    std::string failure;

    bool ok(int wanted_status) const { return status == wanted_status && !failed && !too_large; }
  };
  enum class End { Answered, Deadline, Stopped };
  using OnAnswer = std::function<void(int tag, Answer answer)>;

  Gets(Transport& transport, const Clock& clock, std::stop_token stop, TimePoint deadline, Millis slice)
      : transport_(transport), clock_(clock), stop_(std::move(stop)), deadline_(deadline), slice_(slice) {}

  // Starts a GET, unless the deadline has passed or a stop was requested; `tag` comes back with its answer.
  void start(int tag, const HttpRequest& request, std::size_t max_body) {
    if (stop_.stop_requested() || clock_.now() >= deadline_) return;
    running_[transport_.start(request)] = Running{tag, max_body, {}};
  }

  // Runs until every GET has answered (an answer may start more), the deadline passes or a stop is requested.
  End run(const OnAnswer& on_answer) {
    std::vector<TransportEvent> events;
    while (!running_.empty()) {
      if (stop_.stop_requested()) {
        cancel_all();
        return End::Stopped;
      }
      const TimePoint now = clock_.now();
      if (now >= deadline_) {
        cancel_all();
        return End::Deadline;
      }
      events.clear();
      transport_.wait(std::min(slice_, std::chrono::ceil<Millis>(deadline_ - now)), events);
      for (const TransportEvent& event : events) handle(event, on_answer);
    }
    return End::Answered;
  }

 private:
  struct Running {
    int tag = 0;
    std::size_t max_body = 0;
    Answer answer;
  };
  using Iter = std::map<int, Running>::iterator;

  void handle(const TransportEvent& event, const OnAnswer& on_answer) {
    const Iter it = running_.find(event.exchange);
    if (it == running_.end()) return;  // ended already
    Running& r = it->second;
    switch (event.kind) {
      case TransportEventKind::Head:
        r.answer.status = event.status;
        break;
      case TransportEventKind::Data:
        if (r.answer.status < 200 || r.answer.status >= 300) break;
        if (event.data.size() > r.max_body - r.answer.body.size()) {
          r.answer.too_large = true;
          transport_.cancel(event.exchange);
          finish(it, on_answer);
          break;
        }
        r.answer.body += event.data;
        break;
      case TransportEventKind::Done:
        finish(it, on_answer);
        break;
      case TransportEventKind::Failed:
        r.answer.failed = true;
        r.answer.failure = event.error;
        finish(it, on_answer);
        break;
    }
  }

  // Out of the running set first: the answer's handler may start more GETs.
  void finish(Iter it, const OnAnswer& on_answer) {
    Running done = std::move(it->second);
    running_.erase(it);
    on_answer(done.tag, std::move(done.answer));
  }

  void cancel_all() {
    for (const auto& [exchange, running] : running_) transport_.cancel(exchange);
    running_.clear();
  }

  Transport& transport_;
  const Clock& clock_;
  std::stop_token stop_;
  TimePoint deadline_;
  Millis slice_;
  std::map<int, Running> running_;  // by exchange id
};

std::string local_url(int port, std::string_view path) {
  return "http://127.0.0.1:" + std::to_string(port) + std::string(path);
}

HttpRequest local_get(int port, std::string_view path, Millis connect_timeout) {
  HttpRequest request;
  request.method = "GET";
  request.url = local_url(port, path);
  request.headers.push_back(HttpHeader{"Accept", "application/json"});
  request.connect_timeout = connect_timeout;
  return request;
}

json parse_check(const Gets::Answer& answer) {
  return answer.ok(200) ? en::proto::parse_json(answer.body).value : json();
}

// Ollama's GET /api/version: {"version": "<its version>"}.
bool answers_as_ollama(const Gets::Answer& answer) {
  const json doc = parse_check(answer);
  if (!doc.is_object()) return false;
  const auto version = doc.find("version");
  return version != doc.end() && version->is_string() && !version->get_ref<const std::string&>().empty();
}

// llama.cpp's GET /props: three fields its server README documents and its server sets in every answer. A build too
// old to send build_info stays unconfirmed, which is the safe side; a server that only imitates the first two is not
// taken for llama.cpp.
bool answers_as_llamacpp(const Gets::Answer& answer) {
  const json doc = parse_check(answer);
  if (!doc.is_object()) return false;
  const auto settings = doc.find("default_generation_settings");
  const auto slots = doc.find("total_slots");
  const auto build = doc.find("build_info");
  return settings != doc.end() && settings->is_object() && slots != doc.end() && slots->is_number() &&
         build != doc.end() && build->is_string();
}

enum class Check { List, OllamaVersion, LlamaProps };

struct PortFindings {
  std::optional<ModelList> list;
  bool ollama = false;
  bool llamacpp = false;
};

LocalEndpoint endpoint_of(const ProbePort& port, PortFindings& found) {
  LocalEndpoint e;
  e.port = port.port;
  e.base_url = local_url(port.port, "/v1");
  e.hint = port.usual_owner;
  // One flavour's own answer confirms it; a server that answers as both is taken for neither.
  if (found.ollama != found.llamacpp) {
    e.flavor = found.ollama ? Flavor::Ollama : Flavor::LlamaCpp;
    e.confirmed = true;
  }
  e.models = std::move(found.list->ids);
  e.models_omitted = found.list->omitted;
  return e;
}

ModelsResult failed(ErrorClass error, std::string message, int status = 0) {
  ModelsResult r;
  r.error = error;
  r.http_status = status;
  r.message = std::move(message);
  return r;
}

// Our own words for an HTTP error: the server's text is never passed on, since it may echo a key.
std::string http_error_text(int status, ErrorClass error) {
  const std::string code = "HTTP " + std::to_string(status);
  switch (error) {
    case ErrorClass::Auth: return code + ": the provider refused the key, or wants one";
    case ErrorClass::RateLimited: return code + ": rate limited";
    case ErrorClass::BadRequest: return code + ": the provider refused the request";
    default: return code + ": the provider failed";
  }
}

}  // namespace

std::optional<ModelList> parse_model_list(std::string_view body, const ModelListLimits& limits) {
  if (body.size() > limits.max_body_bytes) return std::nullopt;
  // Read as it parses, keeping only the ids, so its bound is the list's own; its depth is held to en-proto's cap
  // before the parser sees it, as every JSON's is (ARCH §10.2).
  if (!en::proto::within_depth(body, en::proto::kMaxJsonDepth)) return std::nullopt;
  ModelListReader reader(limits);
  if (!json::sax_parse(body.begin(), body.end(), &reader)) return std::nullopt;
  return reader.take();
}

std::vector<LocalEndpoint> probe_local(TransportFactory& transports, const ProbeOptions& options, std::stop_token stop) {
  if (stop.stop_requested()) return {};
  const std::unique_ptr<Transport> transport = transports.open();
  if (!transport) return {};
  const std::shared_ptr<Clock> clock = options.clock ? options.clock : system_clock();
  const std::span<const ProbePort> ports = probe_ports();
  const std::size_t max_check = std::min(kMaxCheckBody, options.limits.max_body_bytes);

  std::vector<PortFindings> found(ports.size());
  std::vector<std::pair<std::size_t, Check>> asked;  // by tag: which port, and what was asked
  Gets gets(*transport, *clock, stop, clock->now() + options.total, slice_of(options.poll_slice));
  auto ask = [&](std::size_t port, Check check, std::string_view path, std::size_t max_body) {
    gets.start(static_cast<int>(asked.size()), local_get(ports[port].port, path, options.connect_timeout), max_body);
    asked.emplace_back(port, check);
  };

  for (std::size_t i = 0; i < ports.size(); ++i) ask(i, Check::List, "/v1/models", options.limits.max_body_bytes);
  gets.run([&](int tag, Gets::Answer answer) {
    const auto [i, check] = asked[static_cast<std::size_t>(tag)];
    switch (check) {
      case Check::List:
        if (answer.ok(200)) found[i].list = parse_model_list(answer.body, options.limits);
        if (found[i].list) {
          // Asked of every port that counts: Ollama and llama.cpp may listen anywhere, and the port proves nothing.
          ask(i, Check::OllamaVersion, "/api/version", max_check);
          ask(i, Check::LlamaProps, "/props", max_check);
        }
        break;
      case Check::OllamaVersion:
        found[i].ollama = answers_as_ollama(answer);
        break;
      case Check::LlamaProps:
        found[i].llamacpp = answers_as_llamacpp(answer);
        break;
    }
  });

  std::vector<LocalEndpoint> endpoints;
  for (std::size_t i = 0; i < ports.size(); ++i) {
    if (found[i].list) endpoints.push_back(endpoint_of(ports[i], found[i]));
  }
  return endpoints;
}

ModelsResult list_models(TransportFactory& transports, const ProviderConfig& provider, const ListModelsOptions& options,
                         std::stop_token stop) {
  if (!provider.keys_resolved()) return failed(ErrorClass::Auth, "a key is missing, so nothing was sent");
  if (stop.stop_requested()) return failed(ErrorClass::Cancelled, "cancelled before it was sent");
  const std::unique_ptr<Transport> transport = transports.open();
  if (!transport) return failed(ErrorClass::NoHealthyProvider, "this build has no HTTP transport");
  const std::shared_ptr<Clock> clock = options.clock ? options.clock : system_clock();

  HttpRequest request = build_models_request(provider);
  request.connect_timeout = options.connect_timeout;
  std::optional<Gets::Answer> answer;
  Gets gets(*transport, *clock, stop, clock->now() + options.total, slice_of(options.poll_slice));
  gets.start(0, request, options.limits.max_body_bytes);
  const Gets::End end = gets.run([&](int, Gets::Answer a) { answer = std::move(a); });

  if (!answer) {
    if (end == Gets::End::Stopped) return failed(ErrorClass::Cancelled, "cancelled");
    return failed(ErrorClass::TotalTimeout, "no model list within " + ms_text(options.total));
  }
  if (answer->failed) return failed(ErrorClass::Server, "connection failed: " + answer->failure);
  if (answer->too_large) {
    return failed(ErrorClass::Server, "the model list passed " + bytes_text(options.limits.max_body_bytes), answer->status);
  }
  if (answer->status == 0) return failed(ErrorClass::Server, "the provider sent no status");
  if (const ErrorClass e = classify_http_status(answer->status); e != ErrorClass::None) {
    return failed(e, http_error_text(answer->status, e), answer->status);
  }
  std::optional<ModelList> list = parse_model_list(answer->body, options.limits);
  if (!list) return failed(ErrorClass::Server, "the answer was not a model list", answer->status);

  ModelsResult result;
  result.http_status = answer->status;
  result.models = std::move(list->ids);
  result.omitted = list->omitted;
  return result;
}

}  // namespace en::llm
