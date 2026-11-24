// The profile editor's glue in the DLL (dll.hpp; S7.2.b; NPC §4.7): the Characters pages' facts from the loaded content
// and the player's own layer files, and the editor's commands made on T6: a check by en-core's own load
// (profile.hpp's check_layer), a save written by T11 (logger.cpp: beside the file and renamed over it, the previous kept
// as <file>.bak) and answered when it is written, a reset and a restore likewise, and a draft asked of en-llm's handle on
// the draft slot (ai.cpp) and answered when it ends. After a write, the content is loaded again once no conversation is
// open, so the next line takes the player's notes (NPC §4.7's "Effect").
//
// - **One validating entry point** (GraceUI's invariant 3): the space has checked each command's shape; here the text is
//   checked as the load checks it, and nothing is written that the load would refuse.
// - **Privacy** (NPC §4.7, PV4): the log names the target, the revision, the size and the outcome's codes, never the
//   player's text, their notes or the model's draft.
#include <algorithm>
#include <cmath>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "dll.hpp"
#include "en/core/content_view.hpp"
#include "en/core/profile.hpp"
#include "en/core/speech_content.hpp"
#include "en/core/speech_lint.hpp"
#include "en/proto/ffi_guard.hpp"

namespace eldennet {

using en::shell::Line;
using en::shell::LogLevel;
namespace ov = en::overlay;
namespace ec = en::core;

namespace {

constexpr std::string_view kPersonaKey = "persona.";

struct PendingWrite {
  ov::ProfileRequest request;
  std::string path;      // the layer's, relative to the overrides folder
  std::string text;      // what was written (Write), for the files kept
  bool remove = false;   // a reset
  bool restore = false;  // a restore's first step: the .bak read
};

struct PendingDraft {
  ov::ProfileRequest request;
  en::proto::RequestId id = 0;
  ec::DraftInput input;
};

// T6's own, made at the first content load and never destroyed (R14).
struct ProfilesState {
  std::vector<ec::ContentFile> files;  // the shipped pack's, then the overrides folder's, as loaded (writes applied)
  ec::Realm realm = ec::Realm::Game;
  std::optional<ec::Lexicon> lexicon;  // the pack's, for a draft's safety check
  ov::CharacterFacts facts;
  bool rows_dirty = true;                // the files or the content changed: the rows are worked out again
  bool reload = false;                   // a write was made: load the content again once no conversation is open
  std::uint64_t next_token = 1;
  std::map<std::uint64_t, PendingWrite> writes;
  std::optional<PendingDraft> draft;
  int drafts = 0;                        // this session's
  en::proto::RequestId next_draft = kDraftRequests;
  // The values each draft proposed this session, by row: a save that takes one marks the layer `assisted` (PL12).
  std::map<std::string, std::vector<nlohmann::json>, std::less<>> proposed;
  bool gate_open = false;
};
constinit ProfilesState* g_profiles = nullptr;

std::string persona_of(std::string_view key) {
  return key.rfind(kPersonaKey, 0) == 0 ? std::string(key.substr(kPersonaKey.size())) : std::string();
}

const ec::ContentFile* file_at(const ProfilesState& st, std::string_view path) {
  const ec::ContentFile* found = nullptr;
  for (const ec::ContentFile& f : st.files) {
    if (f.path == path) found = &f;  // the last of a path wins, as the load takes it
  }
  return found;
}

void put_file(ProfilesState& st, const std::string& path, const std::string* text) {
  std::erase_if(st.files, [&](const ec::ContentFile& f) {
    return f.path == path && f.origin == ec::ContentOrigin::Override;
  });
  if (text != nullptr) st.files.push_back(ec::ContentFile{path, *text, ec::ContentOrigin::Override});
  st.rows_dirty = true;
}

std::vector<ov::ProfileMessage> messages_of(const std::vector<ec::LayerFinding>& findings) {
  std::vector<ov::ProfileMessage> out;
  for (const ec::LayerFinding& f : findings) out.push_back(ov::ProfileMessage{f.field, f.item, f.error, ec::words(f)});
  return out;
}

// The words a register's or a news field's value takes on the pages are en-overlay's: here only the ids.
std::string string_at(const nlohmann::json& j, std::initializer_list<const char*> path) {
  const nlohmann::json* v = &j;
  for (const char* k : path) {
    if (!v->is_object() || !v->contains(k)) return {};
    v = &(*v)[k];
  }
  return v->is_string() ? v->get<std::string>() : std::string();
}

// The rows of Named NPCs: every persona of the loaded content that the realm voices (a test persona only in the tests'
// realm), by its first-meeting name, with its first-meeting fields and the player's own layer file as it stands.
void work_out_rows(ProfilesState& st, const ec::Core& core) {
  std::vector<ov::ProfileRow> rows;
  const ec::ContentPack& pack = core.directory().pack();
  for (const ec::Persona& p : pack.personas) {
    const ec::PersonaRow* index = pack.row(p.id);
    if (index != nullptr && index->test_only && st.realm != ec::Realm::Eval) continue;
    ov::ProfileRow r;
    r.key = std::string(kPersonaKey) + p.id;
    r.name = p.display_name_fallback;
    r.role = string_at(p.record, {"public", "role"});
    r.summary = string_at(p.record, {"public", "summary"});
    if (const auto it = p.record.find("subjects"); it != p.record.end() && it->is_array()) {
      for (const auto& s : *it) {
        if (s.is_string()) r.subjects.push_back(s.get<std::string>());
      }
    }
    r.register_id = string_at(p.record, {"register"});
    r.reply_words_max = p.record.value("reply_words_max", 40);
    r.news = string_at(p.record, {"awareness", "news"});
    if (const ec::ContentFile* f = file_at(st, ec::layer_path(p.id)); f != nullptr) {
      std::vector<ec::LayerFinding> read;
      const auto layer = ec::read_layer(*f, read);
      r.rev = ec::layer_rev(f->text);
      r.newer = std::any_of(read.begin(), read.end(),
                            [](const ec::LayerFinding& x) { return x.problem == ec::LayerProblem::NewerSchema; });
      if (layer) {
        r.fields = layer->fields;
        r.updated = !layer->base_hash.empty() && layer->base_hash != p.hash;
        // The same load the core runs, for its warnings, and for an error the speech parts find (a template mark).
        const ec::LayerCheck c = ec::check_layer(st.files, *f);
        r.refused = !c.ok();
        r.layer = c.ok();
        r.warnings = static_cast<std::uint32_t>(std::count_if(c.findings.begin(), c.findings.end(),
                                                               [](const ec::LayerFinding& x) { return !x.error; }));
      } else {
        r.refused = !r.newer;
      }
    }
    rows.push_back(std::move(r));
  }
  std::stable_sort(rows.begin(), rows.end(), [](const ov::ProfileRow& a, const ov::ProfileRow& b) { return a.name < b.name; });
  if (rows != st.facts.rows) {
    st.facts.rows = std::move(rows);
    ++st.facts.revision;
  }
  st.rows_dirty = false;
}

void answer(ov::Space& space, const ov::ProfileRequest& r, ov::ProfileAnswer a, const char* what) {
  // The log: the command, its row, the revision and the outcome's code, never a text (NPC §4.7).
  log(Line("profile").word("what", what).word("target", r.key).num("rev", r.rev)
          .word("result", a.error.empty() ? "ok" : a.error).num("messages", a.messages.size()));
  space.answer_profile(r, a);
}

ov::ProfileAnswer refusal(std::string code, std::string words = {}) {
  ov::ProfileAnswer a;
  a.error = std::move(code);
  a.detail = std::move(words);
  return a;
}

// The candidate layer a check or a save makes of the view's doc: the fields set (null and an empty note leave a field to
// the shipped profile), at revision `rev`, over the persona's shipped hash. A number that is no whole number is said in
// `bad`.
ec::PersonaLayer layer_of(const ov::ProfileRequest& r, const ec::Persona& p, std::uint32_t rev, bool assisted,
                            std::vector<ov::ProfileMessage>& bad) {
  ec::PersonaLayer l;
  l.persona = p.id;
  l.file = ec::layer_path(p.id);
  l.rev = rev;
  l.base_hash = p.hash;
  l.assisted = assisted;
  for (const auto& [k, v] : r.doc.items()) {
    if (v.is_null()) continue;
    if (k == "instructions" && v.is_string()) {
      const std::string text = v.get<std::string>();
      if (text.find_first_not_of(" \t\r\n") != std::string::npos) l.fields[k] = text;
    } else if (k == "reply_words_max" && v.is_number()) {
      const double d = v.get<double>();
      if (std::floor(d) != d || d < -1e9 || d > 1e9) {
        bad.push_back(ov::ProfileMessage{k, -1, true, "Must be a whole number from 10 to 60."});
      } else {
        l.fields[k] = static_cast<std::int64_t>(d);
      }
    } else {
      l.fields[k] = v;
    }
  }
  return l;
}

bool post(ProfilesState& st, Shell& s, const PendingWrite& w, ProfileWrite::Op op, std::string text) {
  ProfileWrite pw;
  pw.op = op;
  pw.folder = s.paths.data + L"\\overrides";
  pw.rel = w.path;
  pw.text = std::move(text);
  pw.token = st.next_token;
  if (!post_profile_write(pw)) return false;  // C17's eight waiting: busy
  st.writes.emplace(st.next_token++, w);
  return true;
}

void make(Shell& s, ProfilesState& st, const ec::Core& core, ov::Space& space, ov::ProfileRequest r) {
  using K = ov::ProfileRequest::Kind;
  const std::string id = persona_of(r.key);
  const ec::Persona* p = core.directory().pack().persona(id);
  const char* what = r.kind == K::Check ? "check" : r.kind == K::Save ? "save" : r.kind == K::Reset ? "reset"
                     : r.kind == K::Restore ? "restore" : "draft";
  if (p == nullptr || s.paths.data.empty()) {
    answer(space, r, refusal("not_now", "That profile is not loaded just now."), what);
    return;
  }
  const std::string path = ec::layer_path(id);
  const ec::ContentFile* now = file_at(st, path);
  const std::uint32_t current = now != nullptr ? ec::layer_rev(now->text) : 0;
  if (r.kind == K::Check || r.kind == K::Save) {
    if (r.kind == K::Save && r.rev != current) {
      answer(space, r, refusal("conflict"), what);  // the view says "Changed elsewhere since you opened it."
      return;
    }
    // A draft's proposal taken and saved marks the layer assisted (PL12), and it stays so.
    bool assisted = false;
    if (now != nullptr) {
      std::vector<ec::LayerFinding> ignored;
      if (const auto old = ec::read_layer(*now, ignored)) assisted = old->assisted;
    }
    if (const auto it = st.proposed.find(r.key); it != st.proposed.end()) {
      for (const nlohmann::json& v : it->second) {
        for (const auto& [k, value] : r.doc.items()) assisted = assisted || (!value.is_null() && value == v.value(k, nlohmann::json()));
      }
    }
    std::vector<ov::ProfileMessage> bad;
    const ec::PersonaLayer layer = layer_of(r, *p, current + 1, assisted, bad);
    const std::string text = ec::layer_text(layer);
    const ec::LayerCheck c = ec::check_layer(st.files, ec::ContentFile{path, text, ec::ContentOrigin::Override});
    std::vector<ov::ProfileMessage> messages = messages_of(c.findings);
    messages.insert(messages.begin(), bad.begin(), bad.end());
    if (r.kind == K::Check) {
      ov::ProfileAnswer a;
      a.messages = std::move(messages);
      answer(space, r, std::move(a), what);
      return;
    }
    if (!c.ok() || !bad.empty()) {
      ov::ProfileAnswer a = refusal("invalid_value");  // the view says "Not saved: see the messages below."
      a.messages = std::move(messages);
      answer(space, r, std::move(a), what);
      return;
    }
    PendingWrite w{r, path, text, false, false};
    if (!post(st, s, w, ProfileWrite::Op::Write, text)) answer(space, r, refusal("busy", "Busy: try again."), what);
    return;
  }
  if (r.kind == K::Reset) {
    if (now == nullptr) {
      answer(space, r, refusal("not_now", "There are no notes of yours to reset."), what);
      return;
    }
    PendingWrite w{r, path, {}, true, false};
    if (!post(st, s, w, ProfileWrite::Op::Remove, {})) answer(space, r, refusal("busy", "Busy: try again."), what);
    return;
  }
  if (r.kind == K::Restore) {
    PendingWrite w{r, path, {}, false, true};
    if (!post(st, s, w, ProfileWrite::Op::ReadBak, {})) answer(space, r, refusal("busy", "Busy: try again."), what);
    return;
  }
  // A draft (NPC §4.7): one at a time, at most 20 a session, only while the gate allows the network, on the draft slot.
  if (st.draft) {
    answer(space, r, refusal("busy", "A draft is being written: wait for it."), what);
    return;
  }
  if (!st.facts.draft_blocked.empty()) {
    answer(space, r, refusal("not_now", st.facts.draft_blocked), what);
    return;
  }
  ec::DraftInput in;
  in.notes = r.notes;
  in.kind_phrase = "a named person";
  if (now != nullptr) {
    std::vector<ec::LayerFinding> ignored;
    if (const auto old = ec::read_layer(*now, ignored)) {
      for (const char* k : {"instructions", "subjects"}) {
        if (old->fields.contains(k)) in.current[k] = old->fields[k];
      }
    }
  }
  auto messages = ec::draft_messages(core.directory().pack().others, in);
  if (!messages) {
    log(Line("profile").word("what", "draft").word("target", r.key).word("result", "no_prompt"), LogLevel::Warn);
    answer(space, r, refusal("internal", "No draft: its prompt could not be made."), what);
    return;
  }
  en::proto::ChatRequest chat;
  chat.slot = std::string(en::proto::kDraft);
  chat.purpose = en::proto::Purpose::Reply;  // the player asked: not own-initiative (NPC §4.7)
  chat.messages = std::move(*messages);
  chat.max_tokens = ec::kDraftMaxTokens;
  chat.json_schema = ec::draft_schema();
  const en::proto::RequestId rid = st.next_draft++;
  if (!ai_submit(s, en::proto::LlmRequest::chat(rid, std::move(chat)))) {
    answer(space, r, refusal("not_now", "No draft: no model is ready for it."), what);
    return;
  }
  ++st.drafts;
  st.draft = PendingDraft{r, rid, std::move(in)};
  st.rows_dirty = true;  // the documents' draft line follows the count
  log(Line("profile").word("what", "draft").word("target", r.key).word("result", "sent").num("id", rid)
          .num("notes_chars", r.notes.size()).num("drafts", static_cast<std::uint64_t>(st.drafts)));
}

const char* why_words(en::proto::ErrorClass e) {
  using E = en::proto::ErrorClass;
  switch (e) {
    case E::Auth: return "No draft: the provider refused its key.";
    case E::FirstTokenTimeout:
    case E::TotalTimeout: return "No draft: the provider took too long.";
    case E::RateLimited: return "No draft: the provider asks you to wait; try again shortly.";
    case E::GateClosed: return "No draft: EldenNet may not use the network just now.";
    case E::NoHealthyProvider:
    case E::Busy: return "No draft: no model is ready for it.";
    case E::Cancelled: return "The draft was stopped.";
    default: return "No draft: the provider failed.";
  }
}

void draft_ended(ProfilesState& st, ov::Space& space, const en::proto::LlmEvent& e) {
  if (!st.draft || st.draft->id != e.id) return;
  const PendingDraft d = std::move(*st.draft);
  st.draft.reset();
  st.rows_dirty = true;
  const en::proto::ChatResult* chat = e.chat ? &*e.chat : nullptr;
  if (chat == nullptr || !chat->ok()) {
    answer(space, d.request, refusal("not_now", why_words(chat != nullptr ? chat->error : en::proto::ErrorClass::Server)),
           "draft");
    return;
  }
  const ec::Lexicon none;
  const ec::DraftResult got = ec::read_draft(chat->text, d.input, st.lexicon ? *st.lexicon : none);
  ov::ProfileAnswer a;
  a.draft = got.draft;
  // Draft words, for the owner's review. A sentence left out for a name it invented is not marked (NPC §4.7).
  if (got.unreadable) {
    a.notice = "The model's answer could not be read as a draft: try again.";
  } else if (got.discarded) {
    a.notice = "The draft held a word EldenNet never uses, so it was set aside.";
  } else if (got.cut > 0) {
    a.notice = "Part of the draft was cut to fit the fields' lengths.";
  }
  if (!got.draft.empty()) st.proposed[d.request.key].push_back(got.draft);
  log(Line("profile").word("what", "draft").word("target", d.request.key).word("result", "ended")
          .num("fields", got.draft.size()).flag("discarded", got.discarded).flag("unreadable", got.unreadable)
          .num("dropped", got.dropped).num("cut", got.cut));
  space.answer_profile(d.request, a);
}

}  // namespace

void profiles_loaded(Shell& s, const ec::Core& core, std::vector<ec::ContentFile> files, ec::Realm realm) noexcept {
  en::proto::ffi_guard(en::proto::Component::DllMain, [&] {
    (void)s;
    if (g_profiles == nullptr) g_profiles = new ProfilesState();
    ProfilesState& st = *g_profiles;
    st.files = std::move(files);
    st.realm = realm;
    // The pack's lexicon, for a draft's safety check (the pack the core loaded: its others).
    std::string banned;
    std::string safety;
    for (const ec::ContentFile& f : core.directory().pack().others) {
      if (f.path == ec::kBannedPath) banned = f.text;
      if (f.path == ec::kSafetyPath) safety = f.text;
    }
    if (auto lex = ec::parse_lexicon(banned, safety)) st.lexicon = std::move(*lex);
    st.rows_dirty = true;
  });
}

const ov::CharacterFacts* profiles_facts(Shell& s, const ec::Core& core, bool gate_open) noexcept {
  return en::proto::ffi_guard(en::proto::Component::DllMain, static_cast<const ov::CharacterFacts*>(nullptr), [&] {
    ProfilesState* st = g_profiles;
    if (st == nullptr) return static_cast<const ov::CharacterFacts*>(nullptr);
    if (st->rows_dirty) work_out_rows(*st, core);
    // The draft helper's provider, and why it cannot run now (draft words, for the owner's review).
    st->gate_open = gate_open;
    std::string provider = ai_slot_provider(s, en::proto::kDraft);
    std::string blocked;
    if (provider.empty()) {
      blocked = "Drafts need a model: set one up on AI › Models.";
    } else if (!gate_open) {
      blocked = "Drafts wait until EldenNet may use the network.";
    } else if (st->drafts >= ec::kDraftsPerSession) {
      blocked = "This session's 20 drafts are used.";
    }
    if (provider != st->facts.draft_provider || blocked != st->facts.draft_blocked) {
      st->facts.draft_provider = std::move(provider);
      st->facts.draft_blocked = std::move(blocked);
      ++st->facts.revision;
    }
    return static_cast<const ov::CharacterFacts*>(&st->facts);
  });
}

void profiles_request(Shell& s, const ec::Core& core, ov::Space& space, ov::ProfileRequest request) noexcept {
  const bool done = en::proto::ffi_guard(en::proto::Component::DllMain, false, [&] {
    if (g_profiles == nullptr) {
      answer(space, request, refusal("not_now", "Profiles are not loaded yet."), "request");
      return true;
    }
    make(s, *g_profiles, core, space, std::move(request));
    return true;
  });
  if (!done) space.answer_profile(request, refusal("internal"));  // a fault: still answered once
}

void profiles_llm_event(Shell& s, en::proto::LlmEvent event) noexcept {
  en::proto::ffi_guard(en::proto::Component::DllMain, [&] {
    (void)s;
    ov::Space* space = overlay_space();
    if (g_profiles == nullptr || space == nullptr || event.kind != en::proto::EventKind::Ended) return;
    draft_ended(*g_profiles, *space, event);
  });
}

void profiles_tick(Shell& s, ec::Core& core) noexcept {
  en::proto::ffi_guard(en::proto::Component::DllMain, [&] {
    ProfilesState* st = g_profiles;
    ov::Space* space = overlay_space();
    if (st == nullptr) return;
    // T11's writes, each answering its command.
    ProfileWritten done;
    while (take_profile_written(done)) {
      const auto it = st->writes.find(done.token);
      if (it == st->writes.end()) continue;
      PendingWrite w = std::move(it->second);
      st->writes.erase(it);
      const char* what = w.remove ? "reset" : w.restore ? "restore" : "save";
      if (w.restore && done.ok) {
        // The version before the last, checked as the load checks it, then written over the file at the next revision.
        std::vector<ec::LayerFinding> read;
        const ec::ContentFile bak{w.path, done.text, ec::ContentOrigin::Override};
        auto layer = ec::read_layer(bak, read);
        const ec::ContentFile* now = file_at(*st, w.path);
        if (!layer) {
          if (space != nullptr) answer(*space, w.request, refusal("not_now", "The earlier version cannot be read."), what);
          continue;
        }
        layer->rev = (now != nullptr ? ec::layer_rev(now->text) : 0) + 1;
        const std::string text = ec::layer_text(*layer);
        const ec::LayerCheck c = ec::check_layer(st->files, ec::ContentFile{w.path, text, ec::ContentOrigin::Override});
        if (!c.ok()) {
          ov::ProfileAnswer a = refusal("invalid_value", "The earlier version holds a problem, so it was not restored.");
          a.messages = messages_of(c.findings);
          if (space != nullptr) answer(*space, w.request, std::move(a), what);
          continue;
        }
        PendingWrite again{w.request, w.path, text, false, false};
        if (!post(*st, s, again, ProfileWrite::Op::Write, text) && space != nullptr) {
          answer(*space, w.request, refusal("busy", "Busy: try again."), what);
        }
        continue;
      }
      if (!done.ok) {
        const char* words = w.restore ? "There is no earlier version to restore."
                                      : "The file could not be written: check that the overrides folder can be written to.";
        if (space != nullptr) answer(*space, w.request, refusal(w.restore ? "not_now" : "internal", words), what);
        continue;
      }
      put_file(*st, w.path, w.remove ? nullptr : &w.text);
      st->reload = true;
      log(Line("profile_write").word("what", what).word("target", w.request.key).num("rev", ec::layer_rev(w.text))
              .num("bytes", w.text.size()));
      ov::ProfileAnswer a;
      a.chosen = w.request.key;  // the row the list chooses next: the same row, a layer keeps its persona's key (D218)
      if (space != nullptr) answer(*space, w.request, std::move(a), what);
    }
    // A draft whose page every view has left is cancelled (WHY§377 (4)); its command is answered once all the same.
    if (st->draft && space != nullptr &&
        !space->watching(std::string(ov::kProfilesFamily) + "/" + st->draft->request.key)) {
      ai_cancel(s, st->draft->id);
      const PendingDraft d = std::move(*st->draft);
      st->draft.reset();
      st->rows_dirty = true;
      answer(*space, d.request, refusal("not_now", "The draft was stopped: its page was left."), "draft");
    }
    // The content loaded again with the player's files once no conversation is open: the next line takes them.
    if (st->reload && core.conversation().state() == en::proto::ConvState::Closed) {
      st->reload = false;
      const std::vector<ec::ContentError> errors = core.load_content(st->files, st->realm);
      ec::record_load(s.content_view, st->files, errors, core, st->realm, since_ms(),
                      en::proto::ReloadLayer::Overrides);  // Content packs (PAGES §4.6.9)
      log(Line("content_load").word("why", "profile").flag("ok", errors.empty()).num("files", st->files.size())
              .num("errors", errors.size()).num("warnings", core.content_warnings().size()),
          errors.empty() ? LogLevel::Info : LogLevel::Warn);
      st->rows_dirty = true;
    }
  });
}

}  // namespace eldennet
