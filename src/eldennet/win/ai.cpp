// The AI pages' engine side (dll.hpp; PAGES §4.6.2; ARCH §11.1, §11.4, §11.5, §11.7): providers.toml, models.toml and
// secrets.toml read on T1; on T6, the configuration en-llm loads from them, the AI pages' facts, the changes and long
// commands of EldenNet's space, and en-llm's handle (T7–T8), made by T6 when a self-test or a model list first needs
// it and never destroyed (R14). T11 writes the files (logger.cpp), secrets.toml with an access list for this user
// alone (ARCH §11.5).
//
// No key leaves en-llm's Secret but into the request's own header (en-llm) and into secrets.toml (T11): never a log
// line, a topic, an answer or a recording. The log lines here are our own words and numbers, with the configuration's
// provider ids and preset names; en-llm's own lines are not taken, since a server's message could reach them.
#include <algorithm>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>

#include "dll.hpp"
#include "en/llm/edit.hpp"
#include "en/llm/selftest.hpp"
#include "en/llm/service.hpp"
#include "en/llm/transport.hpp"
#include "en/proto/base_url.hpp"
#include "en/proto/ffi_guard.hpp"

namespace eldennet {

using en::shell::Line;
using en::shell::LogLevel;
namespace llm = en::llm;
namespace ov = en::overlay;

// What T1 read, handed to T6 once (Shell::ai_files).
struct AiFilesRead {
  std::optional<std::string> providers;
  std::optional<std::string> models;
  std::optional<std::string> settings;
  std::optional<llm::Secret> secrets;
  std::shared_ptr<llm::TransportFactory> transport;  // WinHTTP, loaded by T1 by its full path (ARCH §11.1)
};

namespace {

// The jobs this increment's Models page shows, in ARCH §11.4's order (Decisions from 0.4, Profile drafts from 0.6).
constexpr std::array<std::string_view, 5> kShownSlots = {llm::kDialogue, llm::kBarkLive, llm::kBarkPool, llm::kMemory,
                                                         llm::kSelftest};

// T6's own, made at the first wake that finds T1's files, never destroyed (R14).
struct AiState {
  AiFilesRead* files = nullptr;
  std::string providers;  // providers.toml as T6 holds it: as read, with the pages' edits made
  bool providers_present = false;
  llm::Secret secrets;    // secrets.toml likewise
  std::optional<llm::Config> config;
  bool refused = false;
  ov::AiRefusal refusal = ov::AiRefusal::Other;  // the last refusal's kind and line, for the pages' words
  std::uint32_t refusal_line = 0;
  std::unique_ptr<llm::Service> service;
  bool gate = false;      // as the handle was last told
  bool gate_now = false;  // the core's, at the last wake
  std::optional<llm::SelfTest> selftest;
  std::uint64_t selftest_at = 0;
  bool selftest_logged = false;
  std::optional<llm::SelfTest> listing;
  bool listing_logged = false;
  std::vector<ov::AiRun> earlier;
  llm::RequestId next_id = 1;
  std::map<std::string, std::int64_t, std::less<>> answered;  // provider → when it last answered (since T1's start)
  // Providers whose last call was refused for their key (a model list's, or the handle's health), or for want of one:
  // kept until the provider answers again or its key or key's place changes, since a reload clears the health's.
  std::set<std::string, std::less<>> rejected;
  ov::AiFacts facts;
  bool values_set = false;  // the space's ai.preset from the files
  std::uint64_t revision = 1;  // the configuration's, each time one is taken (the space's model rows follow it)
  // What each provider listed at its newest answered list (the model list or a self-test): the pickers' choices.
  std::map<std::string, std::vector<std::string>, std::less<>> listed;
  // What only the files and the chosen preset decide, worked out again when either changes (toml++ reads the files):
  // not on every wake.
  bool dirty = true;
  std::string chosen;
};
constinit AiState* g_ai = nullptr;

ov::AiRefusal refusal_of(llm::ConfigRefusal r) noexcept {
  switch (r) {
    case llm::ConfigRefusal::Other: return ov::AiRefusal::Other;
    case llm::ConfigRefusal::Scheme: return ov::AiRefusal::Scheme;
    case llm::ConfigRefusal::Credentials: return ov::AiRefusal::Credentials;
    case llm::ConfigRefusal::QueryOrFragment: return ov::AiRefusal::QueryOrFragment;
    case llm::ConfigRefusal::PlainPublic: return ov::AiRefusal::PlainPublic;
    case llm::ConfigRefusal::PlainCloud: return ov::AiRefusal::PlainCloud;
    case llm::ConfigRefusal::PlainKey: return ov::AiRefusal::PlainKey;
  }
  return ov::AiRefusal::Other;
}

const char* refusal_word(llm::ConfigRefusal r) noexcept {
  switch (r) {
    case llm::ConfigRefusal::Other: return "other";
    case llm::ConfigRefusal::Scheme: return "scheme";
    case llm::ConfigRefusal::Credentials: return "credentials";
    case llm::ConfigRefusal::QueryOrFragment: return "query";
    case llm::ConfigRefusal::PlainPublic: return "plain_public";
    case llm::ConfigRefusal::PlainCloud: return "plain_cloud";
    case llm::ConfigRefusal::PlainKey: return "plain_key";
  }
  return "other";
}

// A configuration error, for the log and the pages: the file, the line and the refusal's kind alone (the message can
// hold an address).
void log_refusal(AiState& st, const llm::ConfigError& e) {
  const std::string what = e.what();
  const std::string file = what.substr(0, what.find(':'));
  std::uint64_t line = e.line();
  if (line == 0) {
    if (const std::size_t at = what.rfind("(line "); at != std::string::npos) {
      line = std::strtoull(what.c_str() + at + 6, nullptr, 10);
    } else if (const std::size_t at2 = what.find(": line "); at2 != std::string::npos) {
      line = std::strtoull(what.c_str() + at2 + 7, nullptr, 10);
    }
  }
  const bool known = file == "providers.toml" || file == "models.toml" || file == "secrets.toml" ||
                     file == "settings.toml";
  st.refusal = refusal_of(e.refusal());
  st.refusal_line = line <= 0xFFFFFFFFu ? static_cast<std::uint32_t>(line) : 0;
  log(Line("ai_config").word("result", "refused").word("file", known ? file : "unknown").num("line", line)
          .word("why", refusal_word(e.refusal())),
      LogLevel::Warn);
}

std::optional<llm::Config> load(AiState& st, std::string_view providers, const llm::Secret& secrets) {
  llm::ConfigTexts texts;
  texts.providers = std::string(providers);
  texts.models = st.files->models;
  texts.settings = st.files->settings;
  texts.secrets = secrets.reveal();
  try {
    std::optional<llm::Config> cfg = llm::parse_config(texts, [](const std::string&) -> std::optional<std::string> {
      return std::nullopt;  // env: refs stay unresolved in game: keys come from secrets.toml (ARCH §11.5)
    });
    std::fill(texts.secrets->begin(), texts.secrets->end(), '\0');
    return cfg;
  } catch (const llm::ConfigError& e) {
    std::fill(texts.secrets->begin(), texts.secrets->end(), '\0');
    log_refusal(st, e);
    return std::nullopt;
  } catch (const std::exception&) {
    std::fill(texts.secrets->begin(), texts.secrets->end(), '\0');
    st.refusal = ov::AiRefusal::Other;
    st.refusal_line = 0;
    log(Line("ai_config").word("result", "refused").word("file", "unknown").num("line", 0), LogLevel::Warn);
    return std::nullopt;
  }
}

ov::AiFailure failure_of(llm::ErrorClass e, int status) noexcept {
  using E = llm::ErrorClass;
  using F = ov::AiFailure;
  switch (e) {
    case E::None: return F::None;
    case E::FirstTokenTimeout:
    case E::TotalTimeout: return F::TimedOut;
    case E::RateLimited: return F::RateLimited;
    case E::Auth: return F::KeyRefused;
    case E::BadRequest: return F::BadRequest;
    case E::Server: return status == 0 ? F::Unreachable : F::BadReply;
    case E::Empty:
    case E::Truncated: return F::BadReply;
    case E::Refused: return F::Refused;
    case E::Cancelled:
    case E::Preempted: return F::Cancelled;
    case E::Busy:
    case E::BudgetSpent: return F::Busy;
    case E::NoHealthyProvider:
    case E::Dropped: return F::NoProvider;
    case E::GateClosed: return F::GateClosed;
  }
  return F::None;
}

ov::AiStep step_of(llm::CheckState s) noexcept {
  switch (s) {
    case llm::CheckState::Waiting: return ov::AiStep::Waiting;
    case llm::CheckState::Running: return ov::AiStep::Running;
    case llm::CheckState::Passed: return ov::AiStep::Passed;
    case llm::CheckState::Failed: return ov::AiStep::Failed;
    case llm::CheckState::Skipped: return ov::AiStep::Skipped;
  }
  return ov::AiStep::Waiting;
}

ov::AiVerdict verdict_of(llm::Verdict v) noexcept {
  switch (v) {
    case llm::Verdict::None: return ov::AiVerdict::None;
    case llm::Verdict::Quick: return ov::AiVerdict::Quick;
    case llm::Verdict::Acceptable: return ov::AiVerdict::Acceptable;
    case llm::Verdict::Slow: return ov::AiVerdict::Slow;
  }
  return ov::AiVerdict::None;
}

ov::AiFlavor flavor_of(llm::Flavor f) noexcept {
  switch (f) {
    case llm::Flavor::OpenAI: return ov::AiFlavor::OpenAI;
    case llm::Flavor::Ollama: return ov::AiFlavor::Ollama;
    case llm::Flavor::LlamaCpp: return ov::AiFlavor::LlamaCpp;
    case llm::Flavor::LmStudio: return ov::AiFlavor::LmStudio;
    case llm::Flavor::KoboldCpp: return ov::AiFlavor::KoboldCpp;
  }
  return ov::AiFlavor::OpenAI;
}

ov::AiJson json_of(llm::JsonMode m) noexcept {
  switch (m) {
    case llm::JsonMode::None: return ov::AiJson::None;
    case llm::JsonMode::JsonObject: return ov::AiJson::Object;
    case llm::JsonMode::JsonSchema: return ov::AiJson::Schema;
  }
  return ov::AiJson::None;
}

// en-proto's slot names, as string views of constants (the facts keep views).
std::string_view slot_name(std::string_view s) noexcept {
  for (const std::string_view k : llm::known_slots()) {
    if (k == s) return k;
  }
  return {};
}

std::vector<ov::AiList> lists_of(const llm::SelfTestReport& r) {
  std::vector<ov::AiList> out;
  for (const llm::ListCheck& c : r.lists) {
    ov::AiList l;
    l.provider = c.provider;
    l.state = step_of(c.state);
    l.why = failure_of(c.error, c.status);
    l.listed = c.listed;
    for (const std::string& m : c.missing) l.missing.push_back(slot_name(m));
    out.push_back(std::move(l));
  }
  return out;
}

// The chosen preset's providers that providers.toml lacks.
void fill_preset(AiState& st, const std::string& chosen) {
  st.facts.preset_missing.clear();
  st.facts.preset_needs_probe = false;
  if (chosen == "custom" || !st.files->models) return;
  try {
    const std::vector<llm::ProviderEntry> missing = llm::preset_providers(
        *st.files->models, chosen, st.config ? st.config->providers : std::vector<llm::ProviderConfig>{});
    for (const llm::ProviderEntry& e : missing) st.facts.preset_missing.push_back(e.id);
  } catch (const llm::ConfigError&) {
    st.facts.preset_needs_probe = std::find(st.facts.presets.begin(), st.facts.presets.end(), chosen) !=
                                  st.facts.presets.end();
  }
}

void fill_facts(AiState& st, const ov::Space& space) {
  ov::AiFacts& f = st.facts;
  f.files = st.refused ? ov::AiFiles::Refused : st.config ? ov::AiFiles::Ready : ov::AiFiles::Missing;
  f.refusal = st.refusal;
  f.refusal_line = st.refusal_line;
  f.preset = st.config ? st.config->preset : std::string();
  const std::string chosen = space.values().value("ai.preset", std::string("custom"));
  if (st.dirty || chosen != st.chosen) {
    f.presets = st.files->models ? llm::preset_ids(*st.files->models) : std::vector<std::string>{};
    f.keys_set = llm::keys_set(st.secrets.reveal());
    fill_preset(st, chosen);
    st.chosen = chosen;
    st.dirty = false;
  }
  f.providers.clear();
  f.slots.clear();
  f.unused.clear();
  const llm::HealthReport health =
      st.service ? st.service->client().health() : llm::HealthReport{};
  auto provider_health = [&](const std::string& id) -> const llm::HealthReport::Provider* {
    for (const auto& p : health.providers) {
      if (p.id == id) return &p;
    }
    return nullptr;
  };
  auto pair_cooling = [&](const std::string& provider, const std::string& model) {
    for (const auto& p : health.pairs) {
      if (p.provider == provider && p.model == model && p.cooling_left.count() > 0) return true;
    }
    return false;
  };
  if (st.config) {
    const llm::Config& cfg = *st.config;
    for (const llm::ProviderConfig& p : cfg.providers) {
      ov::AiProvider a;
      a.id = p.id;
      a.cloud = p.kind == llm::ProviderKind::Cloud;
      a.flavor = flavor_of(p.flavor);
      a.base_url = p.base_url;
      a.concurrency = p.max_concurrency;
      a.json = json_of(p.json_mode);
      const auto* h = provider_health(p.id);
      a.disabled = h != nullptr && h->disabled;
      a.cooling_s = h != nullptr ? static_cast<std::uint32_t>((h->cooling_left.count() + 999) / 1000) : 0;
      for (const auto& pair : health.pairs) {
        if (pair.provider == p.id && pair.cooling_left.count() > 0) ++a.pairs_cooling;
      }
      if (p.key_ref.rfind("secrets:", 0) == 0) {
        a.key_place = ov::AiKeyPlace::Secrets;
        a.key_name = p.key_ref.substr(8);
      } else if (p.key_ref.rfind("env:", 0) == 0) {
        a.key_place = ov::AiKeyPlace::Env;
      }
      a.plain = en::proto::plain_remote(p.base_url);
      if (a.disabled) st.rejected.insert(p.id);
      const bool refused = st.rejected.count(p.id) != 0;
      if (a.plain) {
        a.key = ov::AiKey::NotNeeded;  // http:// on another computer: never a key (parse_config refuses a key_ref there)
      } else if (p.key_ref.empty()) {
        a.key = refused ? ov::AiKey::Rejected : ov::AiKey::NotNeeded;  // no key's place: none, until one is asked for
      } else if (refused) {
        a.key = ov::AiKey::Rejected;
      } else {
        a.key = p.key ? ov::AiKey::Set : ov::AiKey::Missing;
      }
      if (const auto it = st.answered.find(p.id); it != st.answered.end()) a.answered_ms = it->second;
      for (const std::string_view slot : kShownSlots) {
        const llm::SlotConfig& sc = cfg.slot(slot);
        if (std::any_of(sc.models.begin(), sc.models.end(), [&](const llm::ModelRef& m) { return m.provider == p.id; })) {
          a.slots.push_back(slot);
        }
      }
      f.providers.push_back(std::move(a));
    }
    for (const std::string_view slot : kShownSlots) {
      const llm::SlotConfig& sc = cfg.slot(slot);
      ov::AiSlot s;
      s.slot = slot;
      if (const auto it = cfg.borrowed.find(slot); it != cfg.borrowed.end()) s.same_as = slot_name(it->second);
      if (!sc.models.empty()) {
        s.provider = sc.models[0].provider;
        s.model = sc.models[0].model;
      }
      s.picked = cfg.picked.count(slot) != 0;
      if (sc.models.size() > 1) {
        s.fallback_provider = sc.models[1].provider;
        s.fallback = sc.models[1].model;
      }
      s.healthy = std::any_of(sc.models.begin(), sc.models.end(), [&](const llm::ModelRef& m) {
        const auto* h = provider_health(m.provider);
        return (h == nullptr || (!h->disabled && h->cooling_left.count() == 0)) && !pair_cooling(m.provider, m.model);
      });
      f.slots.push_back(std::move(s));
    }
    f.unused = llm::unused_providers(cfg);
  }
  f.revision = st.revision;
  f.models.clear();
  for (const auto& [provider, ids] : st.listed) f.models.push_back(ov::AiModels{provider, ids});
  // The runs.
  f.listing = st.listing && !st.listing->done();
  f.listed = st.listing.has_value();
  f.lists = st.listing ? lists_of(st.listing->report()) : std::vector<ov::AiList>{};
  ov::AiSelfTest& t = f.selftest;
  t = ov::AiSelfTest{};
  if (st.selftest) {
    const llm::SelfTestReport& r = st.selftest->report();
    t.ran = true;
    t.running = !st.selftest->done();
    t.at_ms = st.selftest_at;
    t.lists = lists_of(r);
    t.latency = step_of(r.latency);
    t.latency_why = failure_of(r.latency_error, 0);
    t.json = step_of(r.json);
    t.json_why = failure_of(r.json_error, 0);
    t.provider = r.provider;
    t.model = r.model;
    t.first_token_ms = r.latency == llm::CheckState::Passed ? r.first_token.count() : -1;
    t.tokens_per_second = r.tokens_per_second;
    t.first_page_ms = r.first_page.count();
    t.verdict = verdict_of(r.verdict);
    t.earlier = st.earlier;
  }
}

// The configuration taken anew after an edit: the handle reloads it (en-llm clears a rejected key's disabling).
void commit(AiState& st, std::string providers, llm::Secret secrets, llm::Config config) {
  st.providers = std::move(providers);
  st.providers_present = true;
  st.secrets = std::move(secrets);
  st.config = std::move(config);
  st.refused = false;
  st.dirty = true;
  ++st.revision;
  if (st.service) st.service->reload(*st.config);
}

bool ensure_service(Shell& s, AiState& st) {
  if (st.service) return true;
  if (!st.config || !st.files->transport) return false;
  llm::ServiceOptions options;  // no log: en-llm's lines are its own words, and a server's message could reach them
  try {
    st.service = std::make_unique<llm::Service>(*st.config, st.files->transport, options);
  } catch (const std::exception&) {
    log(Line("ai_service").flag("ok", false), LogLevel::Warn);
    return false;
  }
  HANDLE wake = s.wake;
  st.service->set_notify([wake] { SetEvent(wake); });
  st.gate = st.gate_now;  // the core's gate, as at the last wake (C15)
  st.service->set_gate_open(st.gate);
  log(Line("ai_service").flag("ok", true).num("workers", st.service->stats().workers));
  return true;
}

void log_selftest(const llm::SelfTestReport& r) {
  int passed = 0;
  for (const llm::ListCheck& c : r.lists) passed += c.state == llm::CheckState::Passed ? 1 : 0;
  Line l("selftest");
  l.word("result", r.passed() ? "passed" : "failed").word("verdict", llm::to_string(r.verdict));
  l.num("lists", r.lists.size()).num("lists_passed", static_cast<std::uint64_t>(passed));
  l.word("latency", llm::to_string(r.latency)).word("json", llm::to_string(r.json));
  if (r.latency == llm::CheckState::Passed) {
    l.num("first_token_ms", static_cast<std::uint64_t>(r.first_token.count()))
        .num("tps", static_cast<std::uint64_t>(r.tokens_per_second + 0.5))
        .num("first_page_ms", static_cast<std::uint64_t>(r.first_page.count()));
  }
  log(l);
}

void note_answers(AiState& st, const llm::SelfTestReport& r) {
  const std::int64_t now = static_cast<std::int64_t>(since_ms());
  for (const llm::ListCheck& c : r.lists) {
    if (c.state == llm::CheckState::Running || c.state == llm::CheckState::Waiting) continue;
    if (c.error == llm::ErrorClass::None) {
      st.answered[c.provider] = now;
      st.listed[c.provider] = c.models;  // the pickers' choices: never logged
      st.rejected.erase(c.provider);
    }
    if (c.error == llm::ErrorClass::Auth) st.rejected.insert(c.provider);
  }
  if (r.latency == llm::CheckState::Passed) {
    st.answered[r.provider] = now;
    st.rejected.erase(r.provider);
  }
}

// providers.toml edited as `edit` says, checked, then held and written; false with a log line when refused.
bool change_providers(AiState& st, const llm::ProvidersEdit& edit, const char* what) {
  const llm::EditedText edited = llm::edit_providers(st.providers_present ? st.providers : std::string(), edit);
  if (!edited.ok) {
    log(Line("ai_change").word("what", what).flag("ok", false).word("why", edited.why), LogLevel::Warn);
    return false;
  }
  std::optional<llm::Config> cfg = load(st, edited.text, st.secrets);
  if (!cfg) {
    log(Line("ai_change").word("what", what).flag("ok", false).word("why", "result_refused"), LogLevel::Warn);
    return false;
  }
  commit(st, edited.text, llm::Secret(st.secrets.reveal()), std::move(*cfg));
  post_ai_write(AiWrite::providers(edit));
  log(Line("ai_change").word("what", what).flag("ok", true).num("added", edit.add.size())
          .num("removed", edit.remove.size()));
  return true;
}

bool edit_key(AiState& st, std::string_view name, const llm::Secret* key) {
  llm::EditedSecrets edited = llm::edit_secrets(st.secrets.reveal(), name, key);
  if (!edited.ok) {
    log(Line("ai_change").word("what", "key").word("name", name).flag("ok", false).word("why", edited.why),
        LogLevel::Warn);
    return false;
  }
  if (st.providers_present) {
    std::optional<llm::Config> cfg = load(st, st.providers, edited.text);
    if (!cfg) return false;
    commit(st, st.providers, std::move(edited.text), std::move(*cfg));
  } else {
    st.secrets = std::move(edited.text);
    st.dirty = true;
  }
  post_ai_write(AiWrite::secret(std::string(name), key != nullptr ? std::optional<llm::Secret>(*key) : std::nullopt));
  log(Line("ai_change").word("what", "key").word("name", name).flag("set", key != nullptr).flag("ok", true));
  // A new key, or none: the providers that name it are not refused until a call says so again.
  if (st.config) {
    for (const llm::ProviderConfig& p : st.config->providers) {
      if (p.key_ref == "secrets:" + std::string(name)) st.rejected.erase(p.id);
    }
  }
  return true;
}

// The chosen preset's providers added (and the preset written): the pages' Add provider and a new preset.
bool apply_preset(AiState& st, const std::string& preset, const char* what) {
  llm::ProvidersEdit edit;
  edit.preset = preset;
  if (preset != "custom" && st.files->models) {
    try {
      edit.add = llm::preset_providers(*st.files->models, preset,
                                       st.config ? st.config->providers : std::vector<llm::ProviderConfig>{});
    } catch (const llm::ConfigError&) {
      log(Line("ai_change").word("what", what).flag("ok", false).word("why", "preset"), LogLevel::Warn);
      return false;
    }
  }
  return change_providers(st, edit, what);
}

AiState* state(Shell& s) {
  if (g_ai != nullptr) return g_ai;
  AiFilesRead* files = s.ai_files.load(std::memory_order_acquire);
  if (files == nullptr) return nullptr;
  auto* st = new AiState();  // lives for the process (R14)
  st->files = files;
  st->providers_present = files->providers.has_value();
  if (files->providers) st->providers = *files->providers;
  if (files->secrets) st->secrets = *files->secrets;
  if (st->providers_present) {
    st->config = load(*st, st->providers, st->secrets);
    st->refused = !st->config;
  }
  log(Line("ai_config").word("result", st->refused ? "refused" : st->config ? "ready" : "missing")
          .num("providers", st->config ? st->config->providers.size() : 0)
          .word("preset", st->config ? st->config->preset : std::string("none"))
          .flag("models", files->models.has_value()));
  g_ai = st;
  return st;
}

}  // namespace

AiWrite AiWrite::providers(llm::ProvidersEdit edit) {
  AiWrite w;
  w.file = File::Providers;
  w.edit = std::move(edit);
  return w;
}

AiWrite AiWrite::secret(std::string name, std::optional<llm::Secret> key) {
  AiWrite w;
  w.file = File::Secrets;
  w.name = std::move(name);
  w.key = std::move(key);
  return w;
}

void read_ai_files(Shell& s, const std::optional<std::string>& settings_text) noexcept {
  en::proto::ffi_guard(en::proto::Component::DllMain, [&] {
    if (s.paths.data.empty()) return;
    auto* files = new AiFilesRead();  // lives for the process (R14)
    const std::wstring config = s.paths.data + L"\\config";
    constexpr std::size_t kMax = 64u << 10;  // DESIGN §11.4: a larger file is refused by its size
    files->providers = read_text(config + L"\\providers.toml", kMax);
    if (std::optional<std::string> secrets = read_text(config + L"\\secrets.toml", kMax)) {
      files->secrets = llm::Secret(std::move(*secrets));
    }
    files->models = read_text(s.paths.dll_dir + L"\\content\\eldenring\\models.toml", kMax);
    files->settings = settings_text;
    files->transport = llm::make_default_transport_factory();
    set_ai_folder(config);
    log(Line("ai_files").flag("providers", files->providers.has_value()).flag("models", files->models.has_value())
            .flag("secrets", files->secrets.has_value()).flag("transport", files->transport != nullptr));
    s.ai_files.store(files, std::memory_order_release);
  });
}

const en::overlay::AiFacts* ai_tick(Shell& s, bool gate_open, en::overlay::Space& space) noexcept {
  const en::overlay::AiFacts* out = nullptr;
  en::proto::ffi_guard(en::proto::Component::DllMain, [&] {
    AiState* st = state(s);
    if (st == nullptr) return;
    st->gate_now = gate_open;
    if (!st->values_set) {
      st->values_set = true;
      // The preset's row as the file has it (a choice the page offers, else Custom). Each job's model row follows the
      // facts (the space's sync); each key's status is its provider's row (the providers topic).
      std::string preset = st->config ? st->config->preset : std::string("cloud-budget");
      if (preset != "cloud-budget" && preset != "cloud-nanogpt" && preset != "custom") preset = "custom";
      space.set_value("ai.preset", preset);
    }
    if (st->service) {
      if (gate_open != st->gate) {
        st->gate = gate_open;
        st->service->set_gate_open(gate_open);
      }
      std::vector<llm::LlmEvent> events;
      st->service->drain(events);
      for (const llm::LlmEvent& e : events) {
        if (st->selftest && st->selftest->take(e)) continue;
        if (st->listing && st->listing->take(e)) continue;
      }
      if (st->selftest && !st->selftest->done()) st->selftest->step(*st->service);
      if (st->listing && !st->listing->done()) st->listing->step(*st->service);
      if (st->selftest && st->selftest->done() && !st->selftest_logged) {
        st->selftest_logged = true;
        log_selftest(st->selftest->report());
        note_answers(*st, st->selftest->report());
      }
      if (st->listing && st->listing->done() && !st->listing_logged) {
        st->listing_logged = true;
        const llm::SelfTestReport& r = st->listing->report();
        int passed = 0;
        for (const llm::ListCheck& c : r.lists) passed += c.state == llm::CheckState::Passed ? 1 : 0;
        log(Line("models_list").num("providers", r.lists.size()).num("passed", static_cast<std::uint64_t>(passed)));
        note_answers(*st, r);
      }
    }
    fill_facts(*st, space);
    out = &st->facts;
  });
  return out;
}

void ai_change(Shell& s, en::overlay::Space& space, en::overlay::Space::Change& c) noexcept {
  en::proto::ffi_guard(en::proto::Component::DllMain, [&] {
    AiState* st = state(s);
    if (st == nullptr) return;
    if (c.key == "ai.preset") {
      const std::string preset = c.value.is_string() ? c.value.get<std::string>() : std::string("custom");
      const std::string before = st->config ? st->config->preset : std::string();
      if (!apply_preset(*st, preset, "preset")) {
        space.set_value("ai.preset", before.empty() ? std::string("cloud-budget") : before);  // as the file has it
      }
      return;
    }
    // A job's pick: "slots.<slot>.model", on the provider the space names with it (the job's, or one chosen for it, and
    // a model it listed: the space checked both); an empty one is a reset, the preset's provider and model again. Its
    // fallback stays where it is. Neither the model's id nor the provider's is logged.
    constexpr std::string_view kSuffix = ".model";
    if (c.key.rfind("slots.", 0) != 0 || c.key.size() <= 6 + kSuffix.size() ||
        c.key.compare(c.key.size() - kSuffix.size(), kSuffix.size(), kSuffix) != 0) {
      return;
    }
    const std::string slot = c.key.substr(6, c.key.size() - 6 - kSuffix.size());
    const std::string model = c.value.is_string() ? c.value.get<std::string>() : std::string();
    const auto restore = [&] {  // the job's rows as the configuration has them
      std::string now;
      std::string provider;
      if (st->config) {
        const llm::SlotConfig& sc = st->config->slot(slot);
        if (!sc.models.empty()) {
          now = sc.models.front().model;
          provider = sc.models.front().provider;
        }
      }
      space.set_value(c.key, now);
      space.set_value("slots." + slot + ".provider", provider);
    };
    if (std::find(kShownSlots.begin(), kShownSlots.end(), slot) == kShownSlots.end() || !st->config) {
      restore();
      return;
    }
    llm::ProvidersEdit edit;
    llm::SlotEdit pick;
    pick.slot = slot;
    const llm::SlotConfig& sc = st->config->slot(slot);
    const std::string before = sc.models.empty() ? std::string() : sc.models.front().provider;
    if (!model.empty()) {
      pick.provider = !c.provider.empty() ? c.provider : before;
      if (pick.provider.empty() || st->config->provider(pick.provider) == nullptr) {
        restore();
        return;
      }
      pick.model = model;
      if (sc.models.size() > 1) pick.fallback_provider = sc.models[1].provider;
    }
    const char* what = model.empty() ? "model_reset" : pick.provider != before ? "provider" : "model";
    edit.slots.push_back(std::move(pick));
    if (!change_providers(*st, edit, what)) restore();
  });
}

void ai_request(Shell& s, en::overlay::Space& space, en::overlay::AiRequest& r) noexcept {
  en::proto::ffi_guard(en::proto::Component::DllMain, [&] {
    // A key's value, taken out of the request at once and the request's copy overwritten.
    std::optional<llm::Secret> key;
    if (r.kind == ov::AiRequest::Kind::SetKey) {
      key = llm::clean_key(r.key);
      std::fill(r.key.begin(), r.key.end(), '\0');
    }
    AiState* st = state(s);
    if (st == nullptr) return;
    const llm::ProviderConfig* provider = st->config ? st->config->provider(r.id) : nullptr;
    switch (r.kind) {
      case ov::AiRequest::Kind::SelfTest:
        if (!st->config || !ensure_service(s, *st)) return;
        if (st->selftest) {
          const llm::SelfTestReport& prev = st->selftest->report();
          st->earlier.insert(st->earlier.begin(), ov::AiRun{st->selftest_at, prev.model, verdict_of(prev.verdict),
                                                            prev.passed()});
          if (st->earlier.size() > 10) st->earlier.resize(10);
        }
        st->selftest.emplace(*st->config, false, st->next_id);
        st->next_id += 2;
        st->selftest_at = since_ms();
        st->selftest_logged = false;
        st->selftest->step(*st->service);
        log(Line("selftest").word("result", "started").num("lists", st->selftest->report().lists.size()));
        return;
      case ov::AiRequest::Kind::ListModels:
        if (!st->config || !ensure_service(s, *st)) return;
        st->listing.emplace(*st->config, true, st->next_id);
        st->next_id += 2;
        st->listing_logged = false;
        st->listing->step(*st->service);
        return;
      case ov::AiRequest::Kind::AddPreset:
        (void)apply_preset(*st, space.values().value("ai.preset", std::string("custom")), "add");
        return;
      case ov::AiRequest::Kind::AddOwn: {
        // The space checked the name and the address (en-proto's check_base_url); parse_config checks the result.
        llm::ProvidersEdit edit;
        edit.add.push_back(llm::own_provider(r.id, r.base_url, r.local));
        (void)change_providers(*st, edit, "add_own");
        return;
      }
      case ov::AiRequest::Kind::Remove: {
        if (provider == nullptr) return;
        const std::vector<std::string> unused = llm::unused_providers(*st->config);
        if (std::find(unused.begin(), unused.end(), r.id) == unused.end()) {
          log(Line("ai_change").word("what", "remove").flag("ok", false).word("why", "in_use"), LogLevel::Warn);
          return;
        }
        llm::ProvidersEdit edit;
        edit.remove.push_back(r.id);
        // Its key goes too, where no provider that stays names it.
        std::vector<llm::ProviderConfig> staying;
        std::vector<llm::ProviderConfig> going;
        for (const llm::ProviderConfig& p : st->config->providers) (p.id == r.id ? going : staying).push_back(p);
        const std::vector<std::string> kept = llm::secret_names(staying);
        if (!change_providers(*st, edit, "remove")) return;
        st->listed.erase(r.id);
        st->rejected.erase(r.id);
        for (const std::string& name : llm::secret_names(going)) {
          if (std::find(kept.begin(), kept.end(), name) == kept.end()) (void)edit_key(*st, name, nullptr);
        }
        return;
      }
      case ov::AiRequest::Kind::SetKey: {
        if (provider == nullptr || !key) return;
        // Never a key to an address it would reach in clear text, nor over an environment variable's (the space
        // refused both; checked again here).
        if (en::proto::plain_remote(provider->base_url) || provider->key_ref.rfind("env:", 0) == 0) {
          log(Line("ai_change").word("what", "key").flag("ok", false).word("why", "not_here"), LogLevel::Warn);
          return;
        }
        // Nor to a provider with no key's place that no call has asked one for (its row reads Not needed, D197).
        if (provider->key_ref.empty() && st->rejected.count(r.id) == 0) {
          log(Line("ai_change").word("what", "key").flag("ok", false).word("why", "not_needed"), LogLevel::Warn);
          return;
        }
        std::string name;
        if (provider->key_ref.rfind("secrets:", 0) == 0) {
          name = provider->key_ref.substr(8);
        } else {
          // A provider whose entry had no key's place (a server of the player's own on this PC): it gets its own.
          llm::ProvidersEdit edit;
          edit.key_refs.push_back(llm::KeyRefEdit{r.id, "secrets:" + r.id});
          if (!change_providers(*st, edit, "key_place")) return;
          st->rejected.erase(r.id);
          name = r.id;
        }
        (void)edit_key(*st, name, &*key);
        return;
      }
      case ov::AiRequest::Kind::ClearKey:
        if (provider == nullptr || provider->key_ref.rfind("secrets:", 0) != 0) return;
        (void)edit_key(*st, provider->key_ref.substr(8), nullptr);
        return;
    }
  });
}

}  // namespace eldennet
