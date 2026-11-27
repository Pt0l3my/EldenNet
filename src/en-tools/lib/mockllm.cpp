// mockllm: the scripted model and the recorded one (mockllm.hpp; DESIGN §14, ARCH §12.3).
#include "mockllm.hpp"

#include <algorithm>
#include <array>
#include <sstream>

#include <nlohmann/json.hpp>
#include <toml.hpp>

#include "en/proto/json.hpp"
#include "en/proto/toml_check.hpp"

namespace en::tools {

namespace {

std::optional<proto::ErrorClass> error_named(std::string_view name) {
  for (int i = 0; i <= static_cast<int>(proto::ErrorClass::Dropped); ++i) {
    const auto e = static_cast<proto::ErrorClass>(i);
    if (name == proto::to_string(e)) return e;
  }
  return std::nullopt;
}

std::string at_line(const toml::node& n, const std::string& what) {
  return "line " + std::to_string(n.source().begin.line) + ": " + what;
}

// Cuts `text` into pieces of about `n` bytes, never inside a UTF-8 character.
std::vector<std::string> chunks(std::string_view text, std::size_t n) {
  std::vector<std::string> out;
  n = std::max<std::size_t>(n, 1);
  std::size_t i = 0;
  while (i < text.size()) {
    std::size_t end = std::min(text.size(), i + n);
    while (end < text.size() && (static_cast<unsigned char>(text[end]) & 0xC0) == 0x80) ++end;
    out.emplace_back(text.substr(i, end - i));
    i = end;
  }
  return out;
}

void insert(std::vector<MockEvent>& pending, MockEvent e) {
  // Stable by time: after every event already scheduled at the same time.
  const auto it = std::upper_bound(pending.begin(), pending.end(), e.at,
                                   [](core::Mono t, const MockEvent& x) { return t < x.at; });
  pending.insert(it, std::move(e));
}

proto::LlmEvent event(proto::EventKind kind, proto::RequestId id) {
  proto::LlmEvent e;
  e.kind = kind;
  e.id = id;
  return e;
}

}  // namespace

proto::Expected<MockScript, std::string> parse_mock_script(std::string_view text) {
  if (text.size() > 256 * 1024) return proto::Unexpected{std::string("the script is larger than 256 KB")};
  if (const auto unsafe = proto::unsafe_for_toml(text, proto::TomlText::Player)) {
    return proto::Unexpected{"line " + std::to_string(unsafe->line) + ": refused before it is parsed: " + unsafe->problem};
  }
  toml::parse_result parsed = toml::parse(text);
  if (!parsed) {
    return proto::Unexpected{"line " + std::to_string(parsed.error().source().begin.line) +
                             ": not valid TOML: " + std::string(parsed.error().description())};
  }
  const toml::table& root = parsed.table();
  MockScript script;
  for (auto&& [k, v] : root) {
    if (k.str() != "schema" && k.str() != "reply" && k.str() != "repeat_last") {
      return proto::Unexpected{at_line(v, "an unknown key")};
    }
  }
  if (root["schema"].value_or(std::int64_t{0}) != 1) return proto::Unexpected{std::string("schema must be 1")};
  script.repeat_last = root["repeat_last"].value_or(true);
  const toml::array* replies = root["reply"].as_array();
  if (replies == nullptr || !replies->is_array_of_tables() || replies->empty()) {
    return proto::Unexpected{std::string("the script needs at least one [[reply]]")};
  }
  constexpr std::array<std::string_view, 9> kKeys = {"text",  "think", "trailer", "reset", "first_token_ms",
                                                     "chunk_ms", "chunk_chars", "end_ms", "error"};
  for (std::size_t i = 0; i < replies->size(); ++i) {
    const toml::table& t = *replies->get(i)->as_table();
    const std::string where = "reply[" + std::to_string(i) + "]";
    for (auto&& [k, v] : t) {
      if (std::find(kKeys.begin(), kKeys.end(), k.str()) == kKeys.end()) {
        return proto::Unexpected{at_line(v, where + ": an unknown key")};
      }
    }
    MockReply r;
    r.text = t["text"].value_or(std::string());
    if (const toml::array* parts = t["text"].as_array()) {  // a long text in pieces, joined by one space
      for (const toml::node& n : *parts) {
        const auto s = n.value<std::string>();
        if (!s || s->empty()) return proto::Unexpected{at_line(n, where + ".text: its pieces must be strings")};
        r.text += (r.text.empty() ? "" : " ") + *s;
      }
    }
    r.think = t["think"].value_or(std::string());
    r.trailer = t["trailer"].value_or(std::string());
    r.reset = t["reset"].value_or(false);
    const auto ms = [&](std::string_view key, core::Mono fallback) -> std::optional<core::Mono> {
      const toml::node* n = t.get(key);
      if (n == nullptr) return fallback;
      const auto* i64 = n->as_integer();
      if (i64 == nullptr || i64->get() < 0 || i64->get() > 600000) return std::nullopt;
      return static_cast<core::Mono>(i64->get());
    };
    const auto first = ms("first_token_ms", r.first_token_ms);
    const auto chunk = ms("chunk_ms", r.chunk_ms);
    const auto size = ms("chunk_chars", r.chunk_chars);
    const auto end = ms("end_ms", r.end_ms);
    if (!first || !chunk || !size || !end || *size == 0) {
      return proto::Unexpected{at_line(t, where + ": a time or size must be a whole number from 0 to 600000")};
    }
    r.first_token_ms = *first;
    r.chunk_ms = *chunk;
    r.chunk_chars = static_cast<std::size_t>(*size);
    r.end_ms = *end;
    if (const toml::node* e = t.get("error")) {
      const auto* s = e->as_string();
      const auto cls = s != nullptr ? error_named(s->get()) : std::nullopt;
      if (!cls) return proto::Unexpected{at_line(*e, where + ".error: must be one of en-llm's error classes")};
      r.error = *cls;
    }
    if (r.text.empty() && !r.error) return proto::Unexpected{at_line(t, where + ": needs a text or an error")};
    script.replies.push_back(std::move(r));
  }
  return script;
}

void MockModel::submit(const proto::LlmRequest& request, core::Mono now) {
  const proto::RequestId id = request.id;
  if (request.kind() == proto::RequestKind::Decision) {
    const auto& call = std::get<proto::DecisionCall>(request.body);
    proto::LlmEvent e = event(proto::EventKind::Ended, id);
    proto::DecisionPick pick;
    pick.key = call.rule;
    pick.task = call.task;
    e.pick = pick;
    insert(pending_, MockEvent{now, std::move(e)});
    return;
  }
  ++served_;
  const MockReply* reply = nullptr;
  if (next_ < script_.replies.size()) {
    reply = &script_.replies[next_++];
  } else if (script_.repeat_last && !script_.replies.empty()) {
    reply = &script_.replies.back();
  }
  proto::LlmEvent started = event(proto::EventKind::Started, id);
  started.provider = "mock";
  started.model = "scripted";
  insert(pending_, MockEvent{now, std::move(started)});
  proto::ChatResult result;
  result.provider = "mock";
  result.model = "scripted";
  if (reply == nullptr) {
    result.error = proto::ErrorClass::NoHealthyProvider;
    proto::LlmEvent e = event(proto::EventKind::Ended, id);
    e.chat = result;
    insert(pending_, MockEvent{now, std::move(e)});
    return;
  }
  core::Mono t = now + reply->first_token_ms;
  bool any = false;
  const auto stream = [&](std::string_view text) {
    for (std::string& piece : chunks(text, reply->chunk_chars)) {
      if (any) t += reply->chunk_ms;
      any = true;
      proto::LlmEvent d = event(proto::EventKind::Delta, id);
      d.text = std::move(piece);
      insert(pending_, MockEvent{t, std::move(d)});
    }
  };
  if (!reply->think.empty()) {
    stream("<think>" + reply->think + "</think>");
    if (reply->reset) {
      t += reply->chunk_ms;
      insert(pending_, MockEvent{t, event(proto::EventKind::Reset, id)});
    }
  }
  stream(reply->text);
  if (!reply->trailer.empty()) stream("\n" + reply->trailer);
  result.text = reply->text;
  result.streamed = any;
  result.finish_reason = "stop";
  if (reply->error) {
    result.error = *reply->error;
    result.finish_reason.clear();
  }
  proto::LlmEvent ended = event(proto::EventKind::Ended, id);
  ended.chat = result;
  insert(pending_, MockEvent{(any ? t : now + reply->first_token_ms) + reply->end_ms, std::move(ended)});
}

void MockModel::cancel(proto::RequestId id, core::Mono now) {
  const auto live = std::find_if(pending_.begin(), pending_.end(), [&](const MockEvent& e) { return e.event.id == id; });
  if (live == pending_.end()) return;  // ended already
  std::erase_if(pending_, [&](const MockEvent& e) { return e.event.id == id && e.at > now; });  // what is due stays
  ++cancelled_;
  proto::LlmEvent e = event(proto::EventKind::Ended, id);
  proto::ChatResult r;
  r.error = proto::ErrorClass::Cancelled;
  e.chat = r;
  insert(pending_, MockEvent{now, std::move(e)});
}

std::vector<proto::LlmEvent> MockModel::due(core::Mono now) {
  std::vector<proto::LlmEvent> out;
  std::size_t n = 0;
  while (n < pending_.size() && pending_[n].at <= now) ++n;
  for (std::size_t i = 0; i < n; ++i) out.push_back(std::move(pending_[i].event));
  pending_.erase(pending_.begin(), pending_.begin() + static_cast<std::ptrdiff_t>(n));
  return out;
}

std::optional<core::Mono> MockModel::next_at() const {
  if (pending_.empty()) return std::nullopt;
  return pending_.front().at;
}

std::vector<std::string> mock_lines(const MockScript& script, std::size_t requests) {
  std::vector<std::string> out;
  // Each request on its own clock from 0, as `--llm recorded` reads them.
  MockModel fresh(script);
  for (std::size_t n = 1; n <= requests; ++n) {
    proto::ChatRequest chat;
    fresh.submit(proto::LlmRequest::chat(n, chat), 0);
    std::vector<MockEvent> events;
    while (const auto at = fresh.next_at()) {
      for (proto::LlmEvent& e : fresh.due(*at)) events.push_back(MockEvent{*at, std::move(e)});
    }
    for (const MockEvent& e : events) {
      nlohmann::json j;
      j["request"] = n;
      j["after_ms"] = e.at;
      j["event"] = e.event;
      out.push_back(j.dump());
    }
  }
  return out;
}

proto::Expected<std::vector<RecordedEvent>, std::string> parse_recorded(std::string_view text) {
  std::vector<RecordedEvent> out;
  std::istringstream lines{std::string(text)};
  std::string line;
  std::size_t n = 0;
  while (std::getline(lines, line)) {
    ++n;
    if (line.empty()) continue;
    const proto::ParsedJson p = proto::parse_json(line);
    if (!p.ok() || !p.value.is_object()) return proto::Unexpected{"line " + std::to_string(n) + ": not a JSON object"};
    try {
      RecordedEvent r;
      for (auto it = p.value.begin(); it != p.value.end(); ++it) {
        if (it.key() != "request" && it.key() != "after_ms" && it.key() != "event") {
          return proto::Unexpected{"line " + std::to_string(n) + ": an unknown key"};
        }
      }
      r.request = p.value.at("request").get<std::size_t>();
      r.after_ms = p.value.at("after_ms").get<core::Mono>();
      r.event = p.value.at("event").get<proto::LlmEvent>();
      if (r.request == 0) return proto::Unexpected{"line " + std::to_string(n) + ": request counts from 1"};
      out.push_back(std::move(r));
    } catch (const std::exception& e) {
      return proto::Unexpected{"line " + std::to_string(n) + ": " + std::string(e.what())};
    }
  }
  std::stable_sort(out.begin(), out.end(), [](const RecordedEvent& a, const RecordedEvent& b) {
    return a.request != b.request ? a.request < b.request : a.after_ms < b.after_ms;
  });
  return out;
}

void RecordedModel::submit(const proto::LlmRequest& request, core::Mono now) {
  const std::size_t n = ++served_;
  bool any = false;
  for (const RecordedEvent& r : events_) {
    if (r.request != n) continue;
    proto::LlmEvent e = r.event;
    e.id = request.id;
    insert(pending_, MockEvent{now + r.after_ms, std::move(e)});
    any = true;
  }
  if (!any) {
    proto::LlmEvent e = event(proto::EventKind::Ended, request.id);
    proto::ChatResult r;
    r.error = proto::ErrorClass::NoHealthyProvider;  // the recording holds no answer for it
    e.chat = r;
    insert(pending_, MockEvent{now, std::move(e)});
  }
}

void RecordedModel::cancel(proto::RequestId id, core::Mono now) {
  const auto live = std::find_if(pending_.begin(), pending_.end(), [&](const MockEvent& e) { return e.event.id == id; });
  if (live == pending_.end()) return;
  std::erase_if(pending_, [&](const MockEvent& e) { return e.event.id == id && e.at > now; });
  proto::LlmEvent e = event(proto::EventKind::Ended, id);
  proto::ChatResult r;
  r.error = proto::ErrorClass::Cancelled;
  e.chat = r;
  insert(pending_, MockEvent{now, std::move(e)});
}

std::vector<proto::LlmEvent> RecordedModel::due(core::Mono now) {
  std::vector<proto::LlmEvent> out;
  std::size_t n = 0;
  while (n < pending_.size() && pending_[n].at <= now) ++n;
  for (std::size_t i = 0; i < n; ++i) out.push_back(std::move(pending_[i].event));
  pending_.erase(pending_.begin(), pending_.begin() + static_cast<std::ptrdiff_t>(n));
  return out;
}

std::optional<core::Mono> RecordedModel::next_at() const {
  if (pending_.empty()) return std::nullopt;
  return pending_.front().at;
}

}  // namespace en::tools
