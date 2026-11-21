// The AI pages' self-test and model list (selftest.hpp).
#include "en/llm/selftest.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>

#include <nlohmann/json.hpp>

namespace en::llm {

namespace {

using nlohmann::json;

bool failed_to_answer(const ListCheck& c) { return c.state == CheckState::Failed && c.error != ErrorClass::None; }

}  // namespace

const char* to_string(CheckState s) noexcept {
  switch (s) {
    case CheckState::Waiting: return "waiting";
    case CheckState::Running: return "running";
    case CheckState::Passed: return "passed";
    case CheckState::Failed: return "failed";
    case CheckState::Skipped: return "skipped";
  }
  return "waiting";
}

const char* to_string(Verdict v) noexcept {
  switch (v) {
    case Verdict::None: return "none";
    case Verdict::Quick: return "quick";
    case Verdict::Acceptable: return "acceptable";
    case Verdict::Slow: return "slow";
  }
  return "none";
}

Verdict verdict_of(Millis first_page) noexcept {
  if (first_page <= kQuickPage) return Verdict::Quick;
  if (first_page <= kAcceptablePage) return Verdict::Acceptable;
  return Verdict::Slow;
}

bool SelfTestReport::done() const noexcept {
  const bool lists = std::all_of(this->lists.begin(), this->lists.end(), [](const ListCheck& c) {
    return c.state != CheckState::Waiting && c.state != CheckState::Running;
  });
  if (lists_only) return lists;
  auto over = [](CheckState s) { return s != CheckState::Waiting && s != CheckState::Running; };
  return lists && over(latency) && over(json);
}

bool SelfTestReport::passed() const noexcept {
  const bool lists = std::all_of(this->lists.begin(), this->lists.end(),
                                 [](const ListCheck& c) { return c.state == CheckState::Passed; });
  return lists && (lists_only || (latency == CheckState::Passed && json == CheckState::Passed));
}

SelfTest::SelfTest(const Config& config, bool lists_only, RequestId first_id) : first_id_(first_id) {
  report_.lists_only = lists_only;
  // Each provider a slot in use names, in the slots' order; a slot that borrows another's models adds nothing of its
  // own (its models are that slot's).
  for (const std::string_view name : known_slots()) {
    const auto it = config.slots.find(name);
    if (it == config.slots.end()) continue;
    const bool borrowed = config.borrowed.count(name) != 0;
    for (const ModelRef& m : it->second.models) {
      auto at = std::find_if(report_.lists.begin(), report_.lists.end(),
                             [&](const ListCheck& c) { return c.provider == m.provider; });
      if (at == report_.lists.end()) {
        ListCheck c;
        c.provider = m.provider;
        report_.lists.push_back(std::move(c));
        wants_.emplace_back();
        at = report_.lists.end() - 1;
      }
      if (borrowed) continue;
      std::vector<Want>& want = wants_[static_cast<std::size_t>(at - report_.lists.begin())];
      if (std::none_of(want.begin(), want.end(), [&](const Want& w) { return w.slot == name && w.model == m.model; })) {
        want.push_back(Want{std::string(name), m.model});
      }
    }
  }
  // The model list (Providers' "List models") asks every provider, those no job uses after: whether a provider of the
  // player's own answers, and what it lists for the pages' model pickers.
  if (lists_only) {
    for (const ProviderConfig& p : config.providers) {
      if (std::any_of(report_.lists.begin(), report_.lists.end(), [&](const ListCheck& c) { return c.provider == p.id; })) {
        continue;
      }
      ListCheck c;
      c.provider = p.id;
      report_.lists.push_back(std::move(c));
      wants_.emplace_back();
    }
  }
  list_ids_.resize(report_.lists.size());
  if (lists_only) return;
  const SlotConfig& selftest = config.slot(kSelftest);
  if (!selftest.models.empty()) {
    report_.provider = selftest.models.front().provider;
    report_.model = selftest.models.front().model;
  }
}

ChatRequest SelfTest::latency_request() {
  ChatRequest r;
  r.slot = std::string(kSelftest);
  r.purpose = Purpose::Reply;  // the player asked for it
  r.messages = {Message::system("You answer a test of a connection. Answer in one short sentence of plain words."),
                Message::user("Describe a lantern beside a quiet road at dusk.")};
  return r;
}

ChatRequest SelfTest::json_request() {
  ChatRequest r;
  r.slot = std::string(kMemory);
  r.purpose = Purpose::Reply;
  r.messages = {Message::system("You answer a test of a connection with one JSON object and nothing else."),
                Message::user("Give an object with two fields: \"place\", one word naming a quiet place, and "
                              "\"count\", a whole number from 1 to 9.")};
  r.json_schema = json{{"type", "object"},
                       {"properties", {{"place", {{"type", "string"}}}, {"count", {{"type", "integer"}}}}},
                       {"required", {"place", "count"}},
                       {"additionalProperties", false}};
  return r;
}

bool SelfTest::json_answer_ok(std::string_view text) {
  // A model may wrap the object in a code fence; nothing else is taken off.
  auto trim = [](std::string_view& t) {
    while (!t.empty() && std::isspace(static_cast<unsigned char>(t.front())) != 0) t.remove_prefix(1);
    while (!t.empty() && std::isspace(static_cast<unsigned char>(t.back())) != 0) t.remove_suffix(1);
  };
  trim(text);
  if (text.rfind("```", 0) == 0) {
    const std::size_t line = text.find('\n');
    if (line == std::string_view::npos || text.size() < 6 || text.substr(text.size() - 3) != "```") return false;
    text = text.substr(line + 1, text.size() - line - 4);
    trim(text);
  }
  if (text.size() > (16u << 10)) return false;
  const json j = json::parse(text, nullptr, false);
  if (j.is_discarded() || !j.is_object() || j.size() != 2) return false;
  const auto place = j.find("place");
  const auto count = j.find("count");
  return place != j.end() && place->is_string() && !place->get_ref<const std::string&>().empty() &&
         count != j.end() && count->is_number_integer();
}

void SelfTest::step(Service& service) {
  // 4a: every list at once, each on a worker of its own as they come free.
  for (; next_list_ < report_.lists.size(); ++next_list_) {
    ListCheck& c = report_.lists[next_list_];
    list_ids_[next_list_] = service.list_models(c.provider);
    if (list_ids_[next_list_]) {
      c.state = CheckState::Running;
    } else {
      c.state = CheckState::Failed;
      c.error = ErrorClass::NoHealthyProvider;
    }
  }
  if (report_.lists_only) return;
  const bool lists_over = std::all_of(report_.lists.begin(), report_.lists.end(), [](const ListCheck& c) {
    return c.state != CheckState::Waiting && c.state != CheckState::Running;
  });
  if (!lists_over) return;
  auto provider_failed = [&](const std::string& provider) {
    return std::any_of(report_.lists.begin(), report_.lists.end(),
                       [&](const ListCheck& c) { return c.provider == provider && failed_to_answer(c); });
  };
  // 4b, then 4d, one at a time, each only when its slot's provider answered its list.
  if (!latency_sent_) {
    latency_sent_ = true;
    if (report_.model.empty() || provider_failed(report_.provider)) {
      report_.latency = CheckState::Skipped;
    } else if (service.submit(LlmRequest::chat(first_id_, latency_request())) == SubmitResult::Accepted) {
      report_.latency = CheckState::Running;
      return;
    } else {
      report_.latency = CheckState::Failed;
      report_.latency_error = ErrorClass::Busy;
    }
  }
  if (report_.latency == CheckState::Running) return;
  if (!json_sent_) {
    json_sent_ = true;
    const SlotConfig& memory = service.client().config()->slot(kMemory);
    if (memory.models.empty() || provider_failed(memory.models.front().provider)) {
      report_.json = CheckState::Skipped;
    } else if (service.submit(LlmRequest::chat(first_id_ + 1, json_request())) == SubmitResult::Accepted) {
      report_.json = CheckState::Running;
    } else {
      report_.json = CheckState::Failed;
      report_.json_error = ErrorClass::Busy;
    }
  }
}

bool SelfTest::take(const LlmEvent& event) {
  if (event.kind == EventKind::ModelsEnded) {
    for (std::size_t i = 0; i < report_.lists.size(); ++i) {
      if (!list_ids_[i] || *list_ids_[i] != event.id) continue;
      ListCheck& c = report_.lists[i];
      if (c.state != CheckState::Running) return true;
      const ModelsOutcome& m = event.models;
      c.error = m.error;
      c.status = m.status;
      c.listed = static_cast<int>(m.models.size());
      c.models = m.models;
      if (m.error != ErrorClass::None) {
        c.state = CheckState::Failed;
        return true;
      }
      for (const Want& w : wants_[i]) {
        const bool listed = std::find(m.models.begin(), m.models.end(), w.model) != m.models.end();
        if (!listed && std::find(c.missing.begin(), c.missing.end(), w.slot) == c.missing.end()) {
          c.missing.push_back(w.slot);
        }
      }
      c.state = c.missing.empty() ? CheckState::Passed : CheckState::Failed;
      return true;
    }
    return false;
  }
  if (event.kind == EventKind::Ended && event.chat) {
    if (event.id == first_id_ && latency_sent_ && report_.latency == CheckState::Running) {
      finish_latency(*event.chat);
      return true;
    }
    if (event.id == first_id_ + 1 && json_sent_ && report_.json == CheckState::Running) {
      finish_json(*event.chat);
      return true;
    }
    return event.id == first_id_ || event.id == first_id_ + 1;
  }
  // The run's own Started, Delta and Reset events: its text is never kept.
  return (event.id == first_id_ && latency_sent_) || (event.id == first_id_ + 1 && json_sent_);
}

void SelfTest::finish_latency(const ChatResult& r) {
  report_.latency_error = r.error;
  if (!r.provider.empty()) report_.provider = r.provider;
  if (!r.model.empty()) report_.model = r.model;
  if (!r.ok() || r.first_token.count() < 0) {
    report_.latency = CheckState::Failed;
    if (r.ok()) report_.latency_error = ErrorClass::Empty;
    return;
  }
  report_.latency = CheckState::Passed;
  report_.first_token = r.first_token;
  report_.tokens = r.usage.completion_tokens > 0 ? r.usage.completion_tokens
                                                 : std::max(1, static_cast<int>((r.text.size() + 3) / 4));
  // The rate over the time the reply streamed, at least 50 ms, so that a reply sent in one piece does not read as
  // infinitely fast.
  const double streamed_s = static_cast<double>(std::max<std::int64_t>((r.total - r.first_token).count(), 50)) / 1000.0;
  report_.tokens_per_second = static_cast<double>(report_.tokens) / streamed_s;
  const double page_ms = static_cast<double>(r.first_token.count()) +
                         1000.0 * static_cast<double>(kPageTokens) / report_.tokens_per_second;
  report_.first_page = Millis(static_cast<std::int64_t>(std::llround(page_ms)));
  report_.verdict = verdict_of(report_.first_page);
}

void SelfTest::finish_json(const ChatResult& r) {
  report_.json_error = r.error;
  if (!r.ok()) {
    report_.json = CheckState::Failed;
    return;
  }
  // An answer that is not the object asked for fails with no error class: the provider answered, wrongly.
  report_.json = json_answer_ok(r.text) ? CheckState::Passed : CheckState::Failed;
}

void SelfTest::skip_rest() {
  for (ListCheck& c : report_.lists) {
    if (c.state == CheckState::Waiting || c.state == CheckState::Running) c.state = CheckState::Skipped;
  }
  if (report_.latency == CheckState::Waiting || report_.latency == CheckState::Running) {
    report_.latency = CheckState::Skipped;
  }
  if (report_.json == CheckState::Waiting || report_.json == CheckState::Running) report_.json = CheckState::Skipped;
  latency_sent_ = true;
  json_sent_ = true;
  next_list_ = report_.lists.size();
}

void SelfTest::cancel(Service& service) {
  if (report_.latency == CheckState::Running) service.cancel(first_id_);
  if (report_.json == CheckState::Running) service.cancel(first_id_ + 1);
  skip_rest();
}

}  // namespace en::llm
