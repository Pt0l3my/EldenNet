// The local probes and the model list (ARCH §11.4), on a scripted transport and a manual clock: each URL plays a
// script of timed events, a URL with none is refused as a closed port is, and the deadlines run in no time.
#include <doctest.h>

#include <algorithm>
#include <chrono>
#include <functional>
#include <map>
#include <optional>
#include <stop_token>
#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "en/llm/probe.hpp"
#include "en/llm/request.hpp"
#include "rig.hpp"

using namespace en::llm;
using nlohmann::json;
using namespace std::chrono_literals;

namespace {

struct Step {
  Millis at{0};  // after the request was sent
  TransportEventKind kind = TransportEventKind::Done;
  int status = 0;
  std::string data;
  std::string error;
};
using Script = std::vector<Step>;

// A whole answer from `at`: the head, the body, and the end. Given a `piece` size, the body comes in pieces a
// millisecond apart, so that a caller can give up part way.
Script answer(int status, const std::string& body, Millis at = 10ms, std::size_t piece = 0) {
  Script s{{at, TransportEventKind::Head, status, "", ""}};
  if (piece == 0) piece = std::max<std::size_t>(body.size(), 1);
  Millis t = at;
  for (std::size_t i = 0; i < body.size(); i += piece) {
    s.push_back({t, TransportEventKind::Data, status, body.substr(i, piece), ""});
    if (piece < body.size()) t += 1ms;
  }
  s.push_back({t, TransportEventKind::Done, status, "", ""});
  return s;
}

// A head and then nothing, until the caller gives up.
Script hangs(Millis head_at = 10ms) { return {{head_at, TransportEventKind::Head, 200, "", ""}}; }

Script refused(Millis at = 1ms) { return {{at, TransportEventKind::Failed, 0, "", "Couldn't connect to server"}}; }

struct Seen {
  std::string method;
  std::string url;
  std::vector<HttpHeader> headers;
  std::vector<std::pair<std::string, std::string>> secrets;  // revealed here for the checks only
  std::string body;
  Millis connect_timeout{0};
  TimePoint started;
  std::optional<TimePoint> ended;
  bool cancelled = false;
};

class Scripted final : public TransportFactory {
 public:
  explicit Scripted(std::shared_ptr<ManualClock> c) : clock(std::move(c)) {}

  // Every request for `url` plays the script.
  void on(const std::string& url, Script script) { scripts[url] = std::move(script); }
  std::unique_ptr<Transport> open() override;

  const Seen* first(const std::string& url) const {
    for (const Seen& s : seen) {
      if (s.url == url) return &s;
    }
    return nullptr;
  }

  std::shared_ptr<ManualClock> clock;
  std::map<std::string, Script> scripts;
  std::vector<Seen> seen;
  std::function<void()> on_wait;  // runs at the start of every wait: the tests' way into the caller's loop
  bool gives_transport = true;
  int opened = 0;
  int next_exchange = 1;
};

class ScriptedTransport final : public Transport {
 public:
  explicit ScriptedTransport(Scripted& server) : s_(server) {}

  ~ScriptedTransport() override {
    for (const Running& r : running_) end(r, true);
  }

  int start(const HttpRequest& request) override {
    Seen seen;
    seen.method = request.method;
    seen.url = request.url;
    seen.headers = request.headers;
    for (const auto& [name, value] : request.secret_headers) seen.secrets.emplace_back(name, value.reveal());
    seen.body = request.body;
    seen.connect_timeout = request.connect_timeout;
    seen.started = s_.clock->now();
    const auto script = s_.scripts.find(request.url);
    running_.push_back(Running{s_.next_exchange++, script != s_.scripts.end() ? script->second : refused(), 0,
                               seen.started, s_.seen.size()});
    s_.seen.push_back(std::move(seen));
    return running_.back().id;
  }

  void cancel(int exchange) override {
    const auto it = std::find_if(running_.begin(), running_.end(), [&](const Running& r) { return r.id == exchange; });
    if (it == running_.end()) return;
    end(*it, true);
    running_.erase(it);
  }

  // Moves the clock to the next due event, as the shared fake does, so waiting takes no real time.
  void wait(Millis max_wait, std::vector<TransportEvent>& out) override {
    if (s_.on_wait) s_.on_wait();
    if (collect(out)) return;
    TimePoint target = s_.clock->now() + max_wait;
    for (const Running& r : running_) {
      if (r.next < r.script.size()) target = std::min(target, r.start + r.script[r.next].at);
    }
    s_.clock->advance_to(target);
    collect(out);
  }

 private:
  struct Running {
    int id = 0;
    Script script;
    std::size_t next = 0;
    TimePoint start;
    std::size_t seen = 0;
  };

  void end(const Running& r, bool cancelled) {
    Seen& seen = s_.seen[r.seen];
    seen.ended = s_.clock->now();
    seen.cancelled = cancelled;
  }

  // Delivers every step that is due, in time order; returns whether there were any.
  bool collect(std::vector<TransportEvent>& out) {
    const TimePoint now = s_.clock->now();
    bool any = false;
    while (true) {
      Running* earliest = nullptr;
      for (Running& r : running_) {
        if (r.next >= r.script.size() || r.start + r.script[r.next].at > now) continue;
        if (earliest == nullptr || r.start + r.script[r.next].at < earliest->start + earliest->script[earliest->next].at) {
          earliest = &r;
        }
      }
      if (earliest == nullptr) return any;
      const Step& step = earliest->script[earliest->next++];
      TransportEvent event;
      event.exchange = earliest->id;
      event.kind = step.kind;
      event.status = step.status;
      event.data = step.data;
      event.error = step.error;
      out.push_back(std::move(event));
      any = true;
      if (step.kind == TransportEventKind::Done || step.kind == TransportEventKind::Failed) {
        end(*earliest, false);
        const int id = earliest->id;
        running_.erase(std::find_if(running_.begin(), running_.end(), [&](const Running& r) { return r.id == id; }));
      }
    }
  }

  Scripted& s_;
  std::vector<Running> running_;
};

std::unique_ptr<Transport> Scripted::open() {
  ++opened;
  if (!gives_transport) return nullptr;
  return std::make_unique<ScriptedTransport>(*this);
}

std::string at(int port, const std::string& path) { return "http://127.0.0.1:" + std::to_string(port) + path; }
std::string models_url(int port) { return at(port, "/v1/models"); }
std::string version_url(int port) { return at(port, "/api/version"); }
std::string props_url(int port) { return at(port, "/props"); }

std::string list_of(const std::vector<std::string>& ids) {
  json data = json::array();
  for (const std::string& id : ids) data.push_back({{"id", id}, {"object", "model"}, {"owned_by", "invented"}});
  return json{{"object", "list"}, {"data", data}}.dump();
}

// A long list, built quickly: `n` entries of one invented id, about 32 bytes each.
std::string long_list(std::size_t n) {
  std::string body = R"({"object": "list", "data": [)";
  for (std::size_t i = 0; i < n; ++i) body += i == 0 ? R"({"id": "an-invented-model-id"})" : R"(, {"id": "an-invented-model-id"})";
  return body + "]}";
}

// Invented answers in the shape of each server's own endpoint.
const std::string kOllamaVersion = R"({"version": "0.0.1-invented"})";
const std::string kLlamaProps =
    R"({"default_generation_settings": {"n_ctx": 4096, "params": {}}, "total_slots": 1, "model_path": "models/invented.gguf",)"
    R"( "chat_template": "", "build_info": "b1-invented", "is_sleeping": false})";

struct ProbeRig {
  std::shared_ptr<ManualClock> clock = std::make_shared<ManualClock>();
  Scripted server{clock};
  TimePoint t0 = clock->now();

  ProbeOptions options() const {
    ProbeOptions o;
    o.clock = clock;
    return o;
  }
  std::vector<LocalEndpoint> run(std::stop_token stop = {}) { return probe_local(server, options(), std::move(stop)); }
  Millis elapsed() const { return std::chrono::duration_cast<Millis>(clock->now() - t0); }
  Millis since_start(TimePoint t) const { return std::chrono::duration_cast<Millis>(t - t0); }
};

std::vector<int> ports_of(const std::vector<LocalEndpoint>& endpoints) {
  std::vector<int> ports;
  for (const LocalEndpoint& e : endpoints) ports.push_back(e.port);
  return ports;
}

}  // namespace

// ---- the model list ----------------------------------------------------------------------------------------------

TEST_CASE("a model list: the ids in the server's order") {
  const auto list = parse_model_list(list_of({"model-b", "model-a", "model-c"}));
  REQUIRE(list);
  CHECK(list->ids == std::vector<std::string>{"model-b", "model-a", "model-c"});
  CHECK(list->omitted == 0);

  // Fields of every kind around the ids are passed over, and an "id" inside an entry's own fields is not its id.
  const auto nested = parse_model_list(
      R"({"object": "list", "models": [{"id": "elsewhere"}], "data": [{"meta": {"id": "inner", "size": [1, 2.5, null]},)"
      R"( "id": "outer", "created": 1, "ready": true}], "id": "top"})");
  REQUIRE(nested);
  CHECK(nested->ids == std::vector<std::string>{"outer"});
}

TEST_CASE("an empty model list is a list, and so is Ollama's data: null") {
  const auto empty = parse_model_list(R"({"object": "list", "data": []})");
  REQUIRE(empty);
  CHECK(empty->ids.empty());
  const auto null = parse_model_list(R"({"object": "list", "data": null})");
  REQUIRE(null);
  CHECK(null->ids.empty());
}

TEST_CASE("what is not a model list") {
  CHECK_FALSE(parse_model_list(""));
  CHECK_FALSE(parse_model_list("<html>a page</html>"));
  CHECK_FALSE(parse_model_list(R"([{"id": "model-a"}])"));               // not an object
  CHECK_FALSE(parse_model_list(R"({"object": "list"})"));                 // no data
  CHECK_FALSE(parse_model_list(R"({"data": "model-a"})"));                // data no list
  CHECK_FALSE(parse_model_list(R"({"data": {"id": "model-a"}})"));
  CHECK_FALSE(parse_model_list(R"({"data": [{"name": "model-a"}, 3]})"));  // no entry has an id
  CHECK_FALSE(parse_model_list(R"({"data": [{"id": ""}, {"id": 7}]})"));
  CHECK_FALSE(parse_model_list(R"({"data": [{"id": "a"}]} trailing)"));
  CHECK_FALSE(parse_model_list(R"({"data": [{"id": "a"}], "data": [{"id": "b"}]})"));  // data twice
  CHECK_FALSE(parse_model_list(R"({"data": [{"id": "a"})"));                         // cut short

  // Entries without an id beside ones with an id are left out and counted.
  const auto mixed = parse_model_list(R"({"data": [{"id": "model-a"}, {"name": "x"}, "model-b", [1], {"id": "model-c"}]})");
  REQUIRE(mixed);
  CHECK(mixed->ids == std::vector<std::string>{"model-a", "model-c"});
  CHECK(mixed->omitted == 3);
}

TEST_CASE("bounds on a model list: count, id length, control characters and the body") {
  ModelListLimits limits;
  limits.max_models = 3;
  limits.max_id_bytes = 12;
  limits.max_body_bytes = 4096;

  const auto counted = parse_model_list(list_of({"m1", "m2", "m3", "m4", "m5"}), limits);
  REQUIRE(counted);
  CHECK(counted->ids == std::vector<std::string>{"m1", "m2", "m3"});
  CHECK(counted->omitted == 2);

  // A long id is left out, never cut: a cut id would name no model.
  const auto long_id = parse_model_list(list_of({"twelve-bytes", "thirteen-byte", "ok"}), limits);
  REQUIRE(long_id);
  CHECK(long_id->ids == std::vector<std::string>{"twelve-bytes", "ok"});
  CHECK(long_id->omitted == 1);

  // C0, DEL and C1 controls could act on a terminal; other UTF-8 is kept as it is.
  const auto controls = parse_model_list(
      R"({"data": [{"id": "esc\u001b[2J"}, {"id": "del\u007f"}, {"id": "csi\u009b"}, {"id": "café"}]})", limits);
  REQUIRE(controls);
  CHECK(controls->ids == std::vector<std::string>{"caf\xC3\xA9"});
  CHECK(controls->omitted == 3);

  // A list with only overlong ids is still a list: the server is there, and its ids could not be kept.
  const auto all_long = parse_model_list(list_of({"far-too-long-an-id"}), limits);
  REQUIRE(all_long);
  CHECK(all_long->ids.empty());
  CHECK(all_long->omitted == 1);

  std::vector<std::string> many(400, "padding-id");
  CHECK_FALSE(parse_model_list(list_of(many), limits));  // over max_body_bytes: not read at all
}

// ---- the probe -----------------------------------------------------------------------------------------------------

TEST_CASE("the probe order is ARCH §11.4's, each port with its usual owner") {
  const auto& ports = probe_ports();
  REQUIRE(ports.size() == 5);
  CHECK(ports[0].port == 1234);
  CHECK(ports[0].usual_owner == Flavor::LmStudio);
  CHECK(ports[1].port == 11434);
  CHECK(ports[1].usual_owner == Flavor::Ollama);
  CHECK(ports[2].port == 9931);
  CHECK(ports[2].usual_owner == Flavor::LlamaCpp);
  CHECK(ports[3].port == 8080);
  CHECK(ports[3].usual_owner == Flavor::LlamaCpp);
  CHECK(ports[4].port == 5001);
  CHECK(ports[4].usual_owner == Flavor::KoboldCpp);
}

TEST_CASE("probe: results come in the probe order whatever order the answers came in") {
  ProbeRig rig;
  rig.server.on(models_url(5001), answer(200, list_of({"kobold-model"}), 5ms));
  rig.server.on(models_url(8080), answer(200, list_of({"legacy-model"}), 10ms));
  rig.server.on(models_url(9931), answer(200, list_of({"llama-model"}), 15ms));
  rig.server.on(models_url(11434), answer(200, list_of({"ollama-model", "ollama-other"}), 20ms));
  rig.server.on(models_url(1234), answer(200, list_of({"studio-model"}), 25ms));

  const auto endpoints = rig.run();
  CHECK(ports_of(endpoints) == std::vector<int>{1234, 11434, 9931, 8080, 5001});
  REQUIRE(endpoints.size() == 5);
  CHECK(endpoints[0].base_url == "http://127.0.0.1:1234/v1");
  CHECK(endpoints[1].models == std::vector<std::string>{"ollama-model", "ollama-other"});
  CHECK(endpoints[4].models == std::vector<std::string>{"kobold-model"});
  // Nothing confirmed a flavour (no port answered its checks), so each is openai, its port's owner only a hint.
  const std::vector<Flavor> hints = {Flavor::LmStudio, Flavor::Ollama, Flavor::LlamaCpp, Flavor::LlamaCpp,
                                     Flavor::KoboldCpp};
  for (std::size_t i = 0; i < endpoints.size(); ++i) {
    CHECK(endpoints[i].flavor == Flavor::OpenAI);
    CHECK_FALSE(endpoints[i].confirmed);
    CHECK(endpoints[i].hint == hints[i]);
  }
  CHECK(rig.elapsed() < 100ms);  // everything answered: the probe ends then, not at its deadline
}

TEST_CASE("probe: all at once on one transport, only 127.0.0.1 and three paths, no key, a 500 ms connect timeout") {
  ProbeRig rig;
  for (const ProbePort& p : probe_ports()) rig.server.on(models_url(p.port), answer(200, list_of({"m"}), 20ms));
  rig.run();

  CHECK(rig.server.opened == 1);
  REQUIRE(rig.server.seen.size() == 15);  // five lists, then two checks for each port that counts
  for (std::size_t i = 0; i < 5; ++i) {
    CHECK(rig.server.seen[i].url == models_url(probe_ports()[i].port));
    CHECK(rig.since_start(rig.server.seen[i].started) == 0ms);  // the lists all start at once
  }
  for (const Seen& s : rig.server.seen) {
    bool allowed = false;
    for (const ProbePort& p : probe_ports()) {
      allowed = allowed || s.url == models_url(p.port) || s.url == version_url(p.port) || s.url == props_url(p.port);
    }
    CHECK_MESSAGE(allowed, s.url);
    CHECK(s.method == "GET");
    CHECK(s.secrets.empty());
    CHECK(s.body.empty());
    CHECK(s.connect_timeout == 500ms);
    REQUIRE(s.headers.size() == 1);
    CHECK(s.headers[0].name == "Accept");
  }
}

TEST_CASE("probe: 404, non-JSON, JSON without data or a list without ids is no endpoint; an empty list counts") {
  ProbeRig rig;
  rig.server.on(models_url(1234), answer(404, R"({"error": "not found"})"));
  rig.server.on(models_url(11434), answer(200, "<html>a web page</html>"));
  rig.server.on(models_url(9931), answer(200, R"({"object": "list"})"));
  rig.server.on(models_url(8080), answer(200, R"({"object": "list", "data": []})"));
  rig.server.on(models_url(5001), answer(200, R"({"data": [{"name": "no id"}]})"));

  const auto endpoints = rig.run();
  // The empty list counts: a server that is up with nothing loaded is the commonest local mistake, and the wizard
  // can only say so if it sees the server.
  REQUIRE(ports_of(endpoints) == std::vector<int>{8080});
  CHECK(endpoints[0].models.empty());
  // Only the port that counts is asked for its flavour.
  CHECK(rig.server.first(version_url(8080)) != nullptr);
  for (int port : {1234, 11434, 9931, 5001}) {
    CHECK(rig.server.first(version_url(port)) == nullptr);
    CHECK(rig.server.first(props_url(port)) == nullptr);
  }
}

TEST_CASE("probe: Ollama's empty list (data: null) counts as an endpoint with no models") {
  ProbeRig rig;
  rig.server.on(models_url(11434), answer(200, R"({"object": "list", "data": null})"));
  rig.server.on(version_url(11434), answer(200, kOllamaVersion));
  const auto endpoints = rig.run();
  REQUIRE(ports_of(endpoints) == std::vector<int>{11434});
  CHECK(endpoints[0].models.empty());
  CHECK(endpoints[0].flavor == Flavor::Ollama);
  CHECK(endpoints[0].confirmed);
}

TEST_CASE("probe: Ollama and llama.cpp are confirmed only by their own endpoints") {
  ProbeRig rig;
  // Ollama on its port, confirmed by /api/version.
  rig.server.on(models_url(11434), answer(200, list_of({"ollama-model"})));
  rig.server.on(version_url(11434), answer(200, kOllamaVersion));
  // llama.cpp on its port, confirmed by /props.
  rig.server.on(models_url(9931), answer(200, list_of({"llama-model"})));
  rig.server.on(props_url(9931), answer(200, kLlamaProps));
  // Ollama on LM Studio's port: the port proves nothing either way, the answer does.
  rig.server.on(models_url(1234), answer(200, list_of({"moved-model"})));
  rig.server.on(version_url(1234), answer(200, kOllamaVersion));
  // On the legacy port, props without build_info: not enough to be taken for llama.cpp.
  rig.server.on(models_url(8080), answer(200, list_of({"legacy-model"})));
  rig.server.on(props_url(8080), answer(200, R"({"default_generation_settings": {}, "total_slots": 1})"));
  // A version that is not a string confirms nothing.
  rig.server.on(models_url(5001), answer(200, list_of({"kobold-model"})));
  rig.server.on(version_url(5001), answer(200, R"({"version": 3})"));

  const auto endpoints = rig.run();
  REQUIRE(ports_of(endpoints) == std::vector<int>{1234, 11434, 9931, 8080, 5001});
  CHECK(endpoints[0].flavor == Flavor::Ollama);
  CHECK(endpoints[0].confirmed);
  CHECK(endpoints[0].hint == Flavor::LmStudio);
  CHECK(endpoints[1].flavor == Flavor::Ollama);
  CHECK(endpoints[1].confirmed);
  CHECK(endpoints[2].flavor == Flavor::LlamaCpp);
  CHECK(endpoints[2].confirmed);
  CHECK(endpoints[3].flavor == Flavor::OpenAI);
  CHECK_FALSE(endpoints[3].confirmed);
  CHECK(endpoints[3].hint == Flavor::LlamaCpp);
  CHECK(endpoints[4].flavor == Flavor::OpenAI);
  CHECK_FALSE(endpoints[4].confirmed);
}

TEST_CASE("probe: LM Studio and KoboldCpp are never confirmed, whatever their other paths answer") {
  ProbeRig rig;
  // A server that answers 200 to any path with an error object, and one that imitates part of llama.cpp's props.
  const std::string any_path = R"({"error": "Unexpected endpoint or method."})";
  rig.server.on(models_url(1234), answer(200, list_of({"studio-model"})));
  rig.server.on(version_url(1234), answer(200, any_path));
  rig.server.on(props_url(1234), answer(200, any_path));
  rig.server.on(models_url(5001), answer(200, list_of({"kobold-model"})));
  rig.server.on(version_url(5001), answer(404, "not found"));
  rig.server.on(props_url(5001), answer(200, R"({"chat_template": "", "total_slots": 1, "default_generation_settings": {"n_ctx": 2048}})"));

  const auto endpoints = rig.run();
  REQUIRE(ports_of(endpoints) == std::vector<int>{1234, 5001});
  CHECK(endpoints[0].flavor == Flavor::OpenAI);
  CHECK_FALSE(endpoints[0].confirmed);
  CHECK(endpoints[0].hint == Flavor::LmStudio);
  CHECK(endpoints[1].flavor == Flavor::OpenAI);
  CHECK_FALSE(endpoints[1].confirmed);
  CHECK(endpoints[1].hint == Flavor::KoboldCpp);
}

TEST_CASE("probe: a server that answers as both Ollama and llama.cpp is taken for neither") {
  ProbeRig rig;
  rig.server.on(models_url(11434), answer(200, list_of({"model"})));
  rig.server.on(version_url(11434), answer(200, kOllamaVersion));
  rig.server.on(props_url(11434), answer(200, kLlamaProps));
  const auto endpoints = rig.run();
  REQUIRE(endpoints.size() == 1);
  CHECK(endpoints[0].flavor == Flavor::OpenAI);
  CHECK_FALSE(endpoints[0].confirmed);
  CHECK(endpoints[0].hint == Flavor::Ollama);
}

TEST_CASE("probe: a slow port is cancelled at 1.5 s without delaying the others") {
  ProbeRig rig;
  rig.server.on(models_url(1234), hangs(10ms));
  rig.server.on(models_url(11434), answer(200, list_of({"ollama-model"}), 20ms));
  rig.server.on(version_url(11434), answer(200, kOllamaVersion, 5ms));

  const auto endpoints = rig.run();
  REQUIRE(ports_of(endpoints) == std::vector<int>{11434});
  CHECK(endpoints[0].confirmed);
  CHECK(rig.elapsed() == 1500ms);

  const Seen* slow = rig.server.first(models_url(1234));
  REQUIRE(slow);
  CHECK(slow->cancelled);
  REQUIRE(slow->ended);
  CHECK(rig.since_start(*slow->ended) == 1500ms);
  // The quick port's checks went out as soon as its list came, and ended long before the slow one was given up.
  const Seen* check = rig.server.first(version_url(11434));
  REQUIRE(check);
  CHECK(rig.since_start(check->started) == 20ms);
  REQUIRE(check->ended);
  CHECK(rig.since_start(*check->ended) == 25ms);
  CHECK_FALSE(check->cancelled);
}

TEST_CASE("probe: a port whose checks come too late counts, unconfirmed") {
  ProbeRig rig;
  rig.server.on(models_url(11434), answer(200, list_of({"ollama-model"}), 1400ms));
  rig.server.on(version_url(11434), answer(200, kOllamaVersion, 200ms));  // would come at 1.6 s
  const auto endpoints = rig.run();
  REQUIRE(ports_of(endpoints) == std::vector<int>{11434});
  CHECK(endpoints[0].flavor == Flavor::OpenAI);
  CHECK_FALSE(endpoints[0].confirmed);
  CHECK(rig.elapsed() == 1500ms);
  CHECK(rig.server.first(version_url(11434))->cancelled);
}

TEST_CASE("probe: a stop cancels everything at once and returns the ports that had answered") {
  SUBCASE("requested before it starts: nothing is sent") {
    ProbeRig rig;
    std::stop_source source;
    source.request_stop();
    CHECK(rig.run(source.get_token()).empty());
    CHECK(rig.server.seen.empty());
    CHECK(rig.server.opened == 0);
  }
  SUBCASE("requested while it runs") {
    ProbeRig rig;
    rig.server.on(models_url(1234), hangs(10ms));
    rig.server.on(models_url(11434), answer(200, list_of({"ollama-model"}), 10ms));
    rig.server.on(version_url(11434), answer(200, kOllamaVersion, 500ms));
    std::stop_source source;
    rig.server.on_wait = [&] {
      if (rig.elapsed() >= 100ms) source.request_stop();
    };
    const auto endpoints = rig.run(source.get_token());
    REQUIRE(ports_of(endpoints) == std::vector<int>{11434});
    CHECK_FALSE(endpoints[0].confirmed);  // its check was cut off: never assumed
    CHECK(rig.elapsed() < 200ms);
    for (const Seen& s : rig.server.seen) CHECK(s.ended);
    CHECK(rig.server.first(models_url(1234))->cancelled);
    CHECK(rig.server.first(version_url(11434))->cancelled);
  }
}

TEST_CASE("probe: the model list is bounded, and an answer past its bound is no endpoint") {
  ProbeRig rig;
  std::vector<std::string> many;
  for (int i = 0; i < 1500; ++i) many.push_back("model-" + std::to_string(i));
  many.push_back(std::string(300, 'x'));  // past the length bound
  rig.server.on(models_url(11434), answer(200, list_of(many)));
  // About 1.3 MB of list in 64 KB pieces, past the probe's 1 MB bound.
  rig.server.on(models_url(9931), answer(200, long_list(40000), 10ms, 64u << 10));

  const auto endpoints = rig.run();
  REQUIRE(ports_of(endpoints) == std::vector<int>{11434});
  CHECK(endpoints[0].models.size() == 1000);
  CHECK(endpoints[0].models.front() == "model-0");
  CHECK(endpoints[0].models.back() == "model-999");
  CHECK(endpoints[0].models_omitted == 501);
  const Seen* huge_list = rig.server.first(models_url(9931));
  REQUIRE(huge_list);
  CHECK(huge_list->cancelled);  // given up as soon as it passed the bound, not read to the end
}

TEST_CASE("probe: a list in pieces is read whole") {
  ProbeRig rig;
  rig.server.on(models_url(1234), answer(200, list_of({"model-a", "model-b"}), 10ms, 7));
  const auto endpoints = rig.run();
  REQUIRE(endpoints.size() == 1);
  CHECK(endpoints[0].models == std::vector<std::string>{"model-a", "model-b"});
}

TEST_CASE("probe: no transport, no probe") {
  ProbeRig rig;
  rig.server.gives_transport = false;
  CHECK(rig.run().empty());
  CHECK(rig.server.seen.empty());
}

// ---- the model list of a provider -----------------------------------------------------------------------------------

namespace {

constexpr const char* kRouter = R"(
schema = 1

[[provider]]
id = "router"
base_url = "https://router.example.invalid/api/v1/"
key_ref = "secrets:cloud"
headers = { "X-Title" = "Invented", "X-Extra" = "secrets:other" }
)";

constexpr const char* kKeyless = R"(
schema = 1

[[provider]]
id = "router"
base_url = "https://router.example.invalid/api/v1"
key_ref = "secrets:missing"
)";

const std::string kRouterModels = "https://router.example.invalid/api/v1/models";

struct ListRig {
  std::shared_ptr<ManualClock> clock = std::make_shared<ManualClock>();
  Scripted server{clock};
  TimePoint t0 = clock->now();
  Config config;

  explicit ListRig(const char* providers = kRouter) : config(en::llm::test::make_config(providers)) {}

  ModelsResult run(std::stop_token stop = {}) {
    ListModelsOptions options;
    options.clock = clock;
    return list_models(server, config.providers.at(0), options, std::move(stop));
  }
  Millis elapsed() const { return std::chrono::duration_cast<Millis>(clock->now() - t0); }
};

// No key, URL or host in what a caller may show or log.
void check_clean(const ModelsResult& r) {
  for (const char* secret : {"invented-test-key", "invented-other-key", "example.invalid", "://"}) {
    CHECK_MESSAGE(r.message.find(secret) == std::string::npos, r.message);
  }
}

}  // namespace

TEST_CASE("the models request: GET base_url/models, the provider's headers, the key only in its own header") {
  const Config cfg = en::llm::test::make_config(kRouter);
  const HttpRequest r = build_models_request(cfg.providers[0]);
  CHECK(r.method == "GET");
  CHECK(r.url == kRouterModels);
  CHECK(r.body.empty());
  bool accept = false;
  bool title = false;
  for (const HttpHeader& h : r.headers) {
    CHECK(h.value.find("invented-test-key") == std::string::npos);
    CHECK(h.value.find("invented-other-key") == std::string::npos);
    accept = accept || (h.name == "Accept" && h.value == "application/json");
    title = title || (h.name == "X-Title" && h.value == "Invented");
  }
  CHECK(accept);
  CHECK(title);
  REQUIRE(r.secret_headers.size() == 2);
  CHECK(r.secret_headers[0].first == "Authorization");
  CHECK(r.secret_headers[0].second.reveal() == "Bearer invented-test-key");
  CHECK(r.secret_headers[1].first == "X-Extra");
  CHECK(r.secret_headers[1].second.reveal() == "invented-other-key");

  const Config local = en::llm::test::make_config(en::llm::test::kLocal);
  const HttpRequest l = build_models_request(local.providers[0]);
  CHECK(l.url == "http://127.0.0.1:9931/v1/models");
  CHECK(l.secret_headers.empty());  // no key, no header
}

TEST_CASE("list_models: the ids, with the key sent only in its header") {
  ListRig rig;
  rig.server.on(kRouterModels, answer(200, list_of({"vendor/model-a", "vendor/model-b"})));
  const ModelsResult r = rig.run();
  CHECK(r.ok());
  CHECK(r.http_status == 200);
  CHECK(r.models == std::vector<std::string>{"vendor/model-a", "vendor/model-b"});
  REQUIRE(rig.server.seen.size() == 1);
  const Seen& sent = rig.server.seen[0];
  CHECK(sent.method == "GET");
  CHECK(sent.url == kRouterModels);
  CHECK(sent.connect_timeout == 3000ms);
  CHECK(sent.body.empty());
  CHECK(sent.url.find("invented-test-key") == std::string::npos);
  for (const HttpHeader& h : sent.headers) CHECK(h.value.find("invented-test-key") == std::string::npos);
  REQUIRE_FALSE(sent.secrets.empty());
  CHECK(sent.secrets[0] == std::pair<std::string, std::string>{"Authorization", "Bearer invented-test-key"});
}

TEST_CASE("list_models: HTTP errors take classify_http_status's class, in our own words") {
  const std::vector<std::pair<int, ErrorClass>> cases = {{401, ErrorClass::Auth},
                                                         {403, ErrorClass::Auth},
                                                         {429, ErrorClass::RateLimited},
                                                         {404, ErrorClass::BadRequest},
                                                         {500, ErrorClass::Server},
                                                         {503, ErrorClass::Server}};
  for (const auto& [status, error] : cases) {
    CAPTURE(status);
    ListRig rig;
    // The server's text echoes the key, as some do: none of it reaches the result.
    rig.server.on(kRouterModels, answer(status, R"({"error": "bad key invented-test-key at router.example.invalid"})"));
    const ModelsResult r = rig.run();
    CHECK(r.error == error);
    CHECK(r.http_status == status);
    CHECK(r.models.empty());
    CHECK(r.message.find(std::to_string(status)) != std::string::npos);
    check_clean(r);
  }
}

TEST_CASE("list_models: a broken connection, no answer in time, a stop, and answers that are not lists") {
  SUBCASE("a refused connection is Server, in the transport's words") {
    ListRig rig;  // nothing scripted: refused
    const ModelsResult r = rig.run();
    CHECK(r.error == ErrorClass::Server);
    CHECK(r.message == "connection failed: Couldn't connect to server");
    check_clean(r);
  }
  SUBCASE("no answer within the time is TotalTimeout, and the request is cancelled") {
    ListRig rig;
    rig.server.on(kRouterModels, hangs());
    const ModelsResult r = rig.run();
    CHECK(r.error == ErrorClass::TotalTimeout);
    CHECK(rig.elapsed() == 10000ms);
    CHECK(rig.server.seen[0].cancelled);
    check_clean(r);
  }
  SUBCASE("a stop is Cancelled, at once") {
    ListRig rig;
    rig.server.on(kRouterModels, hangs());
    std::stop_source source;
    rig.server.on_wait = [&] {
      if (rig.elapsed() >= 100ms) source.request_stop();
    };
    const ModelsResult r = rig.run(source.get_token());
    CHECK(r.error == ErrorClass::Cancelled);
    CHECK(rig.elapsed() < 200ms);
    CHECK(rig.server.seen[0].cancelled);
  }
  SUBCASE("a stop before it starts sends nothing") {
    ListRig rig;
    std::stop_source source;
    source.request_stop();
    CHECK(rig.run(source.get_token()).error == ErrorClass::Cancelled);
    CHECK(rig.server.seen.empty());
  }
  SUBCASE("a 200 that is not a model list is Server") {
    ListRig rig;
    rig.server.on(kRouterModels, answer(200, "<html>a sign-in page</html>"));
    const ModelsResult r = rig.run();
    CHECK(r.error == ErrorClass::Server);
    CHECK(r.http_status == 200);
    check_clean(r);
  }
  SUBCASE("an empty list is an answer with no models") {
    ListRig rig;
    rig.server.on(kRouterModels, answer(200, R"({"data": []})"));
    const ModelsResult r = rig.run();
    CHECK(r.ok());
    CHECK(r.models.empty());
  }
}

TEST_CASE("list_models: a provider whose key did not resolve is Auth, and nothing is sent") {
  ListRig rig(kKeyless);
  const ModelsResult r = rig.run();
  CHECK(r.error == ErrorClass::Auth);
  CHECK(rig.server.opened == 0);
  CHECK(rig.server.seen.empty());
  check_clean(r);
}

TEST_CASE("list_models: a local provider without a key sends no key header") {
  ListRig rig(en::llm::test::kLocal);
  rig.server.on("http://127.0.0.1:9931/v1/models", answer(200, list_of({"local-model"})));
  const ModelsResult r = rig.run();
  CHECK(r.ok());
  CHECK(r.models == std::vector<std::string>{"local-model"});
  REQUIRE(rig.server.seen.size() == 1);
  CHECK(rig.server.seen[0].secrets.empty());
}

TEST_CASE("list_models: a list past the body bound is Server, and the request is cancelled") {
  ListRig rig;
  rig.server.on(kRouterModels, answer(200, long_list(300000), 10ms, 1u << 20));  // about 9.6 MB, past 8 MB
  const ModelsResult r = rig.run();
  CHECK(r.error == ErrorClass::Server);
  CHECK(r.message == "the model list passed 8 MB");
  CHECK(rig.server.seen[0].cancelled);
}
