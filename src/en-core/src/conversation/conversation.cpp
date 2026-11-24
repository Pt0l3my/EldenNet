// The conversation's state machine (conversation.hpp; NPC §5, §6, §8.4, §8.7, §10.5; DESIGN §8, §11.1, §11.2).
#include "en/core/conversation.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <type_traits>
#include <unordered_map>
#include <variant>

#include "en/core/speech_lint.hpp"  // DeflectionRing
#include "en/core/toasts.hpp"

namespace en::core {

namespace {

using T = ConversationTimes;
constexpr Mono kStruckMs = 8000;  // the "you, now" line's fight: struck in the last 8 s (NPC §7.3)

// ---- Words a player reads (our own; DESIGN §2.7's where it has them). Drafts until the owner approves them. ----
constexpr std::string_view kNothingToSay = "They have nothing to say to you.";
constexpr std::string_view kDangerNear = "Not now: danger is near.";
constexpr std::string_view kMomentPassed = "The moment has passed.";
constexpr std::string_view kTimedOut = "No answer: provider timed out";
constexpr std::string_view kKeyRejected = "API key rejected: open settings (F10)";
constexpr std::string_view kRateLimited = "Provider rate limit, try again shortly";
constexpr std::string_view kProviderFailed = "No answer: the provider failed";
constexpr std::string_view kNoPrompt = "No answer: the conversation's prompt could not be made";
constexpr std::string_view kNoModel = "No answer: no model is ready";
constexpr std::string_view kInputBusy = "Chat cannot take the keyboard just now.";
// A refused box names the holder by the id GraceUI gives (its own menu: "graceui"), and a not-now one its reason
// (WHY§442); anything else is told in the generic words above.
constexpr std::string_view kInputMenu = "Chat cannot take the keyboard: GraceUI's menu has the controls.";
constexpr std::string_view kInputModBefore = "Chat cannot take the keyboard: another mod (";
constexpr std::string_view kInputModAfter = ") has the controls.";
constexpr std::string_view kInputMod = "Chat cannot take the keyboard: another mod has the controls.";
constexpr std::string_view kInputDanger = "Chat cannot take the keyboard: danger is near.";
constexpr std::string_view kInputLoading = "Chat cannot take the keyboard while the game loads.";
constexpr std::string_view kInputGameMenu = "Chat cannot take the keyboard while a game menu is open.";
constexpr std::string_view kDegraded = "Conversation paused: part of the game EldenNet reads stopped working.";
constexpr std::string_view kFault = "Conversation closed after a fault in EldenNet.";
constexpr std::string_view kEllipsis = "\xE2\x80\xA6";  // the "…" page and card

bool is_space(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }

// A mod's id as GraceUI checks it at connect (3 to 32 of a-z, 0-9 and '-'): only such an id is ever put in a toast.
bool is_mod_id(std::string_view id) {
  if (id.size() < 3 || id.size() > 32) return false;
  return std::all_of(id.begin(), id.end(),
                     [](char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-'; });
}

// The toast for a box GraceUI refused its input (`detail` the holder's mod id) or answered not now (`detail` the
// overlay's word for why: danger, loading, game_menu, not_ready).
std::string input_refusal(proto::CloseReason why, std::string_view detail) {
  if (why == proto::CloseReason::Refused) {
    if (detail == "graceui") return std::string(kInputMenu);
    if (detail == "eldennet") return std::string(kInputBusy);  // EldenNet's own: nothing to name
    if (!is_mod_id(detail)) return std::string(kInputMod);
    std::string text(kInputModBefore);
    text.append(detail).append(kInputModAfter);
    return text;
  }
  if (detail == "danger") return std::string(kInputDanger);
  if (detail == "loading") return std::string(kInputLoading);
  if (detail == "game_menu") return std::string(kInputGameMenu);
  return std::string(kInputBusy);
}

// The Tarnished's line as typed, made safe to commit and send: control characters out (C0, DEL, C1), runs of
// whitespace as one space, none at either end, at most kLineMaxChars characters and kLineMaxBytes bytes (NPC §8.5).
std::string clean_line(std::string_view typed) {
  std::string out;
  out.reserve(std::min(typed.size(), proto::kLineMaxBytes));
  std::size_t chars = 0;
  bool space = false;
  for (std::size_t i = 0; i < typed.size();) {
    const auto c = static_cast<unsigned char>(typed[i]);
    std::size_t len = c < 0x80 ? 1 : (c >> 5) == 0x6 ? 2 : (c >> 4) == 0xE ? 3 : (c >> 3) == 0x1E ? 4 : 0;
    if (len == 0 || i + len > typed.size()) {  // a broken byte: dropped
      ++i;
      continue;
    }
    bool valid = true;
    for (std::size_t k = 1; k < len; ++k) valid = valid && (static_cast<unsigned char>(typed[i + k]) & 0xC0) == 0x80;
    if (!valid) {
      ++i;
      continue;
    }
    const bool control = (len == 1 && (c < 0x20 || c == 0x7F)) ||
                         (len == 2 && c == 0xC2 && static_cast<unsigned char>(typed[i + 1]) < 0xA0);
    const bool blank = len == 1 && is_space(static_cast<char>(c));
    if (blank || control) {
      space = !out.empty();
      i += len;
      continue;
    }
    const std::size_t extra = space ? 1 : 0;
    if (chars + extra + 1 > proto::kLineMaxChars || out.size() + extra + len > proto::kLineMaxBytes) break;
    if (space) {
      out += ' ';
      ++chars;
      space = false;
    }
    out.append(typed.substr(i, len));
    ++chars;
    i += len;
  }
  return out;
}

// Block 9's time of day, in our own words, from the game clock (the hours are ours, unverified against the game).
std::string time_word(const proto::Clock& c) {
  if (c.h >= 5 && c.h < 8) return "at dawn";
  if (c.h >= 8 && c.h < 17) return "by day";
  if (c.h >= 17 && c.h < 20) return "at dusk";
  return "at night";
}

bool group_ok(const proto::WorldSnapshot& s, proto::Group g) {
  return s.health.of(g).state == proto::GroupState::Ok;
}

struct Candidate {
  std::size_t chr = 0;  // index in the snapshot's chars
  Binding binding;
};

struct Tail {
  proto::PageView view;  // a page of a reply that is no longer the current one
  std::optional<Mono> shown_at;
  Mono stay_ms = 0;      // how long it stays from its first visible frame
  Mono created = 0;
};

struct ReplyRun {
  proto::ReplyId id = 0;
  proto::RequestId request = 0;
  bool live = false;
  Mono submitted = 0;  // the line's Submit: the 20 s total, shared with the regeneration
  bool regenerated = false;
  std::optional<HardList> reminder;
  std::unique_ptr<SentenceStream> stream;
  std::unique_ptr<ReplyLint> lint;
  std::unique_ptr<ReplyPager> pager;
  bool sentences_done = false;  // the pager was told no sentence follows
  bool kept_any = false;        // a sentence went into the pager
  std::vector<Page> pages;      // sealed, by index
  std::optional<std::uint8_t> on_screen;
  Mono published_at = 0;
  std::optional<Mono> shown_at;  // the on-screen page's PageShown
  std::uint8_t acked = 0;        // pages committed
};

enum class Hold : std::uint8_t { None, Native, Hidden };

struct Active {
  proto::ConvId id = 0;
  std::size_t persona = 0;
  std::string persona_id;
  std::string name;
  proto::ChrHandle handle;
  bool box_open = false;
  std::string box_draft;
  std::optional<std::string> queued;
  std::optional<ReplyRun> reply;
  std::optional<Tail> tail;
  std::optional<Mono> failure_until;
  proto::ReplyId failure_reply = 0;  // the reply the "…" page stands for: each failure a page of its own
  std::vector<Turn> turns;                  // committed, oldest first
  std::vector<proto::HistoryLine> strip;    // the history strip: committed lines
  LintTally tally;
  proto::ReplyId next_reply = 1;
  std::uint16_t player_lines = 0, committed = 0;
  Mono idle_from = 0;  // the last player line, or the opening; moved on by the time held
  std::optional<Mono> danger_since, far_since, missing_since;
  Hold hold = Hold::None;
  Mono held_since = 0;
  std::optional<Mono> native_ended_at;
  bool ending = false;  // after the current page
  EndReason end_reason = EndReason::Closed;
  bool esc_closed = false;  // Esc closed the box while a reply paces: end when it is done (NPC §5.7)
  std::optional<proto::ChrHandle> retarget;  // open on this one once ended
};

struct Grace {
  proto::ConvId conv = 0;
  proto::ReplyId reply = 0;
  std::string persona;
  std::uint8_t index = 0;
  std::string text;
  Mono until = 0;
};

struct KeptDraft {
  std::string persona;
  std::string text;
  Mono until = 0;
};

}  // namespace

struct Conversation::Impl {
  explicit Impl(const SpeechParts& p) : parts(p) {}

  const SpeechParts& parts;
  proto::RequestId next_request = kConversationRequests;
  proto::ConvId next_conv = 1;
  std::uint64_t epoch = 0;
  std::optional<Mono> quiet_until;
  Toasts toasts;
  std::vector<KeptDraft> drafts;
  std::map<std::string, DeflectionRing> rings;  // each persona's deflections, least recently used first (§8.4)
  std::optional<std::pair<std::string, Mono>> shut_for;  // the persona a Close{Shut} after an end belongs to

  // What the snapshots showed.
  Surroundings around;
  std::optional<Mono> native_free_since;
  std::unordered_map<std::uint64_t, Mono> approached_at;
  std::unordered_map<std::uint64_t, Mono> attacked_at;
  std::optional<std::uint32_t> last_hp;
  bool hp_dropped = false;
  std::vector<Candidate> candidates;  // valid for the newest snapshot
  std::optional<proto::ChrHandle> best, stable;
  Mono best_since = 0;

  std::optional<Active> conv;
  std::vector<Grace> grace;
  std::optional<std::uint64_t> cast_hash;  // the content the candidates and the conversation were read from

  // The step's.
  const ConversationInput* in = nullptr;
  ConversationOutput* out = nullptr;
  Mono now = 0;

  // ---- The world ----

  [[nodiscard]] const proto::WorldSnapshot* snap() const { return in->world.snapshot; }
  [[nodiscard]] const Directory* cast() const { return in->world.cast; }

  bool hostile(const proto::Chr& c) const {
    const auto& h = in->world.hostile_teams;
    return std::find(h.begin(), h.end(), c.team) != h.end();
  }

  void observe(const proto::WorldSnapshot& s) {
    Surroundings w;
    const proto::ChrHandle player = s.player ? s.player->handle : proto::ChrHandle{};
    if (s.player) {
      w.player_dead = s.player->dead;
      w.riding = s.player->riding;
      w.sheltered = s.player->roundtable;
      hp_dropped = last_hp && s.player->hp.cur < *last_hp;
      last_hp = s.player->hp.cur;
    } else {
      hp_dropped = false;
      last_hp.reset();
    }
    w.boss = s.boss.active;
    w.caps_combat = group_ok(s, proto::Group::Combat);
    w.camera = group_ok(s, proto::Group::Camera);
    w.menu = s.ui.hud != proto::HudState::Default;
    w.banner = s.ui.banner.has_value();
    w.native = s.ui.hud == proto::HudState::PopupMenu || s.ui.talk_cam || s.ui.popup_talk_id.has_value();
    w.hidden = s.ui.hud == proto::HudState::HideAll || s.ui.hud == proto::HudState::Unknown || s.ui.banner.has_value();
    w.caps_conversation = true;
    for (const proto::Group g : {proto::Group::Session, proto::Group::World, proto::Group::Player, proto::Group::Flags,
                                 proto::Group::Ui, proto::Group::Place, proto::Group::Params}) {
      w.caps_conversation = w.caps_conversation && group_ok(s, g);
    }
    for (const proto::Group g : {proto::Group::Flags, proto::Group::Ui, proto::Group::Params}) {
      w.degraded = w.degraded || s.health.of(g).state == proto::GroupState::Tripped;
    }
    for (const proto::Chr& c : s.chars) {
      if (c.dead || !hostile(c)) continue;
      w.hostile_m = std::min(w.hostile_m, c.dist_m);
      const bool engaged = (c.last_hit_by && *c.last_hit_by == player) ||
                           (s.player && s.player->last_hit_by && *s.player->last_hit_by == c.handle);
      if (engaged) w.engaged_hostile_m = std::min(w.engaged_hostile_m, c.dist_m);
    }
    around = w;
    // The observer, failing closed (§5.8): free of native talk since when.
    if (w.native) {
      native_free_since.reset();
    } else if (!native_free_since) {
      native_free_since = now;
    }
    // The personas present, when each came within reach, and when the Tarnished last struck each.
    candidates.clear();
    std::unordered_map<std::uint64_t, Mono> approached;
    if (cast() != nullptr) {
      for (std::size_t i = 0; i < s.chars.size(); ++i) {
        const proto::Chr& c = s.chars[i];
        if (s.player && c.handle == player) continue;
        const auto b = cast()->index().resolve(c);
        if (!b) continue;
        candidates.push_back(Candidate{i, *b});
        if (c.dist_m <= T::kReachM) {
          const auto it = approached_at.find(c.handle.raw);
          approached.emplace(c.handle.raw, it != approached_at.end() ? it->second : now);
        }
        if (s.player && c.last_hit_by && *c.last_hit_by == player) attacked_at[c.handle.raw] = now;
      }
    }
    std::erase_if(attacked_at, [&](const auto& kv) { return now - kv.second >= T::kAttackedMs; });
    approached_at = std::move(approached);
    // The hint's target: a new best leads for 500 ms before it replaces another (§5.1).
    const auto b = choose(s);
    if (b != best) {
      best = b;
      best_since = now;
    }
    if (!best || !stable || now - best_since >= T::kHintLeadMs) stable = best;
  }

  void refresh_flags() {
    around.active = in->world.speech && in->world.in_world && !in->world.dormant;
    around.combat = in->world.combat;
  }

  [[nodiscard]] const proto::Chr* chr_of(proto::ChrHandle h) const {
    if (snap() == nullptr) return nullptr;
    for (const proto::Chr& c : snap()->chars) {
      if (c.handle == h) return &c;
    }
    return nullptr;
  }

  [[nodiscard]] const Candidate* candidate_of(proto::ChrHandle h) const {
    if (snap() == nullptr) return nullptr;
    for (const Candidate& c : candidates) {
      if (c.chr < snap()->chars.size() && snap()->chars[c.chr].handle == h) return &c;
    }
    return nullptr;
  }

  // NPC §5.2, rules 3 to 5 (0 to 2 wait for own-initiative lines and S3's observer; 6, the call-out, for v0.6).
  [[nodiscard]] std::optional<proto::ChrHandle> choose(const proto::WorldSnapshot& s) const {
    const auto eligible = [&](const Candidate& c) { return c.binding.form != "summon" && !s.chars[c.chr].dead; };
    if (s.player && s.player->locked_on) {
      for (const Candidate& c : candidates) {
        if (eligible(c) && s.chars[c.chr].handle == *s.player->locked_on) return s.chars[c.chr].handle;
      }
    }
    if (around.camera) {
      std::optional<proto::ChrHandle> pick;
      float score = 0;
      for (const Candidate& c : candidates) {
        const proto::Chr& ch = s.chars[c.chr];
        if (!eligible(c) || !std::isfinite(ch.bearing_deg)) continue;
        const float angle = std::fabs(ch.bearing_deg);
        if (angle > T::kConeDeg || ch.dist_m > T::kReachM) continue;
        const float sc = angle / T::kConeDeg + ch.dist_m / T::kReachM;
        if (!pick || sc < score || (sc == score && ch.handle < *pick)) {
          pick = ch.handle;
          score = sc;
        }
      }
      if (pick) return pick;
    }
    const float reach = around.sheltered ? T::kReachM : T::kNearestM;
    std::optional<proto::ChrHandle> pick;
    float dist = 0;
    for (const Candidate& c : candidates) {
      const proto::Chr& ch = s.chars[c.chr];
      if (!eligible(c) || ch.dist_m > reach) continue;
      if (!pick || ch.dist_m < dist || (ch.dist_m == dist && ch.handle < *pick)) {
        pick = ch.handle;
        dist = ch.dist_m;
      }
    }
    return pick;
  }

  [[nodiscard]] std::size_t stage_of(std::size_t persona) const {
    if (const KnowledgeView* k = in->world.knowledge; k != nullptr && persona < k->personas.size()) {
      return k->personas[persona].stage;  // the catalogue's flags, in three values (knowledge.hpp; NPC §3.2)
    }
    if (cast() == nullptr || in->world.predicates == nullptr) return 0;
    return cast()->active_stage(persona, *in->world.predicates, in->world.facts);
  }

  [[nodiscard]] PartnerFacts facts_of(const Candidate& c) const {
    PartnerFacts f;
    f.persona = &cast()->persona(c.binding.persona);
    f.stage = stage_of(c.binding.persona);
    f.chr = &snap()->chars[c.chr];
    f.summon = c.binding.form == "summon";
    f.hostile = hostile(*f.chr);
    for (const proto::BossBar& bar : snap()->boss.bars) f.holds_boss_bar = f.holds_boss_bar || bar.handle == f.chr->handle;
    if (const auto it = attacked_at.find(f.chr->handle.raw); it != attacked_at.end()) f.attacked_at = it->second;
    return f;
  }

  // The fail-closed observer (DESIGN §8.7, NPC §5.8): the HUD at Default, 3 s free of the pop-up and the talk camera,
  // and 3 s since the persona came within reach (time for a greeting without a menu to play).
  [[nodiscard]] bool observer_allows(proto::ChrHandle h) const {
    if (snap() == nullptr || snap()->ui.hud != proto::HudState::Default) return false;
    if (!native_free_since || now - *native_free_since < T::kObserverMs) return false;
    const auto it = approached_at.find(h.raw);
    return it != approached_at.end() && now - it->second >= T::kObserverMs;
  }

  [[nodiscard]] bool would_open(const Candidate& c) const {
    return !conversable(facts_of(c), around, now) && !safe_context(around) && observer_allows(snap()->chars[c.chr].handle);
  }

  // ---- Commits ----

  void commit(Commit c) {
    c.conv = c.conv == 0 && conv ? conv->id : c.conv;
    c.at = now;
    out->commits.push_back(std::move(c));
  }

  void toast(ToastClass cls, proto::ToastLevel level, std::string_view text, Mono hold = Toasts::kHoldMs) {
    toasts.post(cls, level, std::string(text), now, hold);
  }

  // ---- Opening ----

  void on_open_chat() {
    if (snap() == nullptr || cast() == nullptr || !in->world.chat) return;  // features.chat off: the key does nothing
    const std::optional<proto::ChrHandle> target = stable && candidate_of(*stable) ? stable : best;
    if (conv) {
      if (conv->box_open || conv->ending) return;
      if (target && *target != conv->handle) {
        if (const Candidate* c = candidate_of(*target); c != nullptr && would_open(*c)) {
          conv->retarget = *target;  // §5.7: end after the current page, then open the new one
          end(EndReason::Retarget, false);
          return;
        }
      }
      // Reopens on the same target with its draft, once danger has cleared (§5.4).
      const Candidate* c = candidate_of(conv->handle);
      if (c == nullptr) return;
      if (const auto why = conversable(facts_of(*c), around, now)) {
        refuse(*why, *c);
        return;
      }
      if (const auto unsafe = safe_context(around)) {
        refuse_unsafe(*unsafe);
        return;
      }
      conv->box_open = true;
      conv->box_draft = kept_draft(conv->persona_id);
      conv->esc_closed = false;  // opened again while its reply paces: the conversation goes on
      ++epoch;
      return;
    }
    if (!target) return;  // nothing in range: no response
    const Candidate* c = candidate_of(*target);
    if (c == nullptr) return;
    if (const auto why = conversable(facts_of(*c), around, now)) {
      refuse(*why, *c);
      return;
    }
    if (const auto unsafe = safe_context(around)) {
      refuse_unsafe(*unsafe);
      return;
    }
    if (!observer_allows(*target)) return;  // fail closed, silently: the game may be talking
    open(*c);
  }

  void refuse(NotConversable why, const Candidate& c) {
    const Persona& p = cast()->persona(c.binding.persona);
    const std::string& name = p.display_name_fallback;
    switch (why) {
      case NotConversable::Stage:
        if (p.chat_at(stage_of(c.binding.persona)) == ChatMode::Refuse) {
          toast(ToastClass::Status, proto::ToastLevel::Info, "(" + name + " will not speak with you)");
        } else {
          toast(ToastClass::Status, proto::ToastLevel::Info, kNothingToSay, 2000);
        }
        return;
      case NotConversable::Muted:
        toast(ToastClass::Status, proto::ToastLevel::Info, kNothingToSay, 2000);
        return;
      case NotConversable::Asleep:
        toast(ToastClass::Status, proto::ToastLevel::Info,
              p.state_at(stage_of(c.binding.persona)) == Conscious::Senseless ? "(" + name + " does not stir.)"
                                                                              : "(" + name + " is asleep.)");
        return;
      default:
        return;  // dead, hostile, struck, busy, in native talk, unavailable: no generated speech, and no word
    }
  }

  void refuse_unsafe(Unsafe why) {
    if (why == Unsafe::Player || why == Unsafe::Boss || why == Unsafe::Combat || why == Unsafe::Hostiles) {
      toast(ToastClass::Danger, proto::ToastLevel::Info, kDangerNear);
    }
  }

  // The draft kept for `persona`, within its 60 s (NPC §5.4). The store keeps its copy, on its first clock, until the
  // box's next close replaces it (keep_draft) or a line is sent: a box GraceUI refuses at its opening (not now, or
  // refused: it never held input, so its close carries no draft) loses nothing (WHY§586).
  [[nodiscard]] std::string kept_draft(const std::string& persona) const {
    for (const KeptDraft& d : drafts) {
      if (d.persona == persona && d.until > now) return d.text;
    }
    return {};
  }

  void keep_draft(const std::string& persona, const std::string& text) {
    std::erase_if(drafts, [&](const KeptDraft& d) { return d.persona == persona; });
    if (!text.empty()) drafts.push_back(KeptDraft{persona, text, now + T::kDraftMs});
  }

  void open(const Candidate& c) {
    Active a;
    a.id = next_conv++;
    a.persona = c.binding.persona;
    const Persona& p = cast()->persona(a.persona);
    a.persona_id = p.id;
    a.name = p.display_name_fallback;
    a.handle = snap()->chars[c.chr].handle;
    a.box_open = true;
    a.box_draft = kept_draft(p.id);
    a.idle_from = now;
    conv = std::move(a);
    ++epoch;  // a box opening is an input epoch (§5.9)
    Commit s;
    s.kind = Commit::Kind::Started;
    s.persona = conv->persona_id;
    commit(std::move(s));
  }

  // ---- A line ----

  void on_submit(const std::string& typed) {
    if (!conv || !conv->box_open || conv->ending) return;
    std::string line = clean_line(typed);
    if (line.empty()) return;
    keep_draft(conv->persona_id, {});  // the box held input: its own text, not the store's copy, is the draft now
    if (conv->reply) {
      if (conv->reply->acked == 0) {
        conv->queued = std::move(line);  // a later line replaces it, never committed (§6.3)
        return;
      }
      cut_off();
    }
    dispatch(std::move(line));
  }

  void cancel_request(ReplyRun& r) {
    if (!r.live) return;
    out->cancels.push_back(r.request);
    r.live = false;
  }

  // §6.3: the page up stays until it has shown 2 s or its hold ends; the stream stops; unshown pages are dropped; the
  // NpcSaid is finalised with the pages shown and marked cut off.
  void cut_off() {
    ReplyRun& r = *conv->reply;
    cancel_request(r);
    if (r.on_screen) {
      Tail t;
      t.view = page_view(r, *r.on_screen);
      t.shown_at = r.shown_at;
      t.stay_ms = std::min<Mono>(T::kCutOffMs, r.pages[*r.on_screen].hold_ms);
      t.created = now;
      conv->tail = std::move(t);
    }
    if (r.acked > 0) {
      Commit c;
      c.kind = Commit::Kind::CutOff;
      c.reply = r.id;
      c.persona = conv->persona_id;
      commit(std::move(c));
    }
    if (r.lint) conv->tally = r.lint->tally();
    conv->reply.reset();
  }

  [[nodiscard]] proto::Surface surface() const {
    return snap() != nullptr && !snap()->ui.subtitles_on ? proto::Surface::HistoryStrip : proto::Surface::LowerThird;
  }

  [[nodiscard]] proto::PageView page_view(const ReplyRun& r, std::uint8_t index) const {
    proto::PageView v;
    v.id = proto::PageId{conv->id, r.id, index};
    v.kind = proto::PageKind::Speech;
    v.surface = surface();
    v.speaker = conv->name;
    v.text = r.pages[index].text;
    v.count = static_cast<std::uint8_t>(r.pages.size());
    v.last = r.pages[index].last;
    v.hold_ms = r.pages[index].hold_ms;
    return v;
  }

  [[nodiscard]] Scene scene() const {
    Scene s;
    if (snap() != nullptr && snap()->clock) s.time_word = time_word(*snap()->clock);
    // The "you, now" line (NPC §7.3; O35): the partner's wounds, and the fight when the Tarnished struck it in the
    // last 8 s; its statuses wait for S8.
    if (const proto::Chr* c = conv ? chr_of(conv->handle) : nullptr; c != nullptr) {
      const auto hit = attacked_at.find(c->handle.raw);
      const bool fighting = hit != attacked_at.end() && now - hit->second < kStruckMs;
      s.you_now = you_now(c->hp, fighting, in->world.self_awareness);
    }
    return s;
  }

  void open_parts(ReplyRun& r) {
    const Persona& p = cast()->persona(conv->persona);
    // The persona's own names: as the game shows it, and its record's fallback. Step 5 strips them as a leading
    // "Name:", and the lint reads them for third-person self-reference.
    std::vector<std::string> self;
    for (const std::string& n : {conv->name, p.display_name_fallback}) {
      if (!n.empty() && std::find(self.begin(), self.end(), n) == self.end()) self.push_back(n);
    }
    r.stream = parts.sanitizer.open(SanitizeContext{self});
    std::vector<std::string> tics;
    std::vector<std::uint8_t> tic_max;
    for (const Tic& t : p.tics) {
      tics.push_back(t.text);
      tic_max.push_back(t.max_per_conversation);
    }
    // The proper nouns this turn shows (the lint's unknown nouns): the persona's names and its forms of address; the
    // places, the lexicons' names and the KB join with the scene's place and knowledge (S11.3.c, v0.3).
    std::vector<std::string> names = self;
    for (const std::string& a : p.address_forms) names.push_back(a);
    LintContext lc;
    lc.regeneration = r.regenerated;
    lc.archaic = p.reg == Register::ArchaicRegal;
    lc.openers = p.openers;
    lc.tics = tics;
    lc.names = names;
    lc.tally = conv->tally;
    lc.self_names = self;
    lc.tic_max = tic_max;
    r.lint = parts.lint.open(lc);
    r.pager = parts.pager.open(PageContext{p.reply_words_max});
    r.sentences_done = false;
    r.kept_any = false;
    r.pages.clear();
    r.on_screen.reset();
    r.shown_at.reset();
  }

  // Renders and sends a request for the current reply; false when nothing went (a failed render sends nothing).
  bool send(ReplyRun& r) {
    const Persona& p = cast()->persona(conv->persona);
    const nlohmann::json record = p.resolved(stage_of(conv->persona));
    // Block 6 (b) is what came before the newest run of the Tarnished's lines, which block 10 recaps (its last three).
    std::size_t split = conv->turns.size();
    while (split > 0 && conv->turns[split - 1].who == Turn::Who::Tarnished) --split;
    std::vector<std::string> recap;
    for (std::size_t i = split; i < conv->turns.size(); ++i) recap.push_back(conv->turns[i].text);
    if (recap.size() > 3) recap.erase(recap.begin(), recap.end() - 3);
    std::vector<Turn> earlier;
    if (in->world.log != nullptr) {
      for (const LoggedEvent& e : in->world.log->last_lines(conv->persona_id, conv->id, 6)) {
        earlier.push_back(Turn{e.kind == Commit::Kind::PlayerSaid ? Turn::Who::Tarnished : Turn::Who::Persona, e.text});
      }
    }
    DialogueContext ctx;
    ctx.persona = conv->persona_id;
    ctx.record = &record;
    ctx.name = conv->name;
    ctx.earlier = earlier;
    ctx.history = std::span<const Turn>(conv->turns.data(), split);
    ctx.scene = scene();
    ctx.recap = recap;
    ctx.reminder = r.reminder;
    ctx.tally = conv->tally;  // block 11's tic note, carried from reply to reply
    // Block 5b's lines: what the persona knows at its stage, set out positively (MEM §12.6; O36, O86).
    if (const KnowledgeView* k = in->world.knowledge; k != nullptr && conv->persona < k->personas.size()) {
      ctx.tied = k->personas[conv->persona].tied;
      ctx.land = k->personas[conv->persona].land;
    }
    auto rendered = parts.prompt.render(ctx);
    if (!rendered) return false;
    r.request = next_request++;
    r.live = true;
    out->requests.push_back(proto::LlmRequest::chat(r.request, dialogue_request(std::move(*rendered))));
    return true;
  }

  void dispatch(std::string line) {
    Commit c;
    c.kind = Commit::Kind::PlayerSaid;
    c.persona = conv->persona_id;
    c.text = line;
    commit(std::move(c));
    ++conv->player_lines;
    ++conv->committed;
    conv->strip.push_back(proto::HistoryLine{proto::HistoryLine::Who::Tarnished, "", line, false});
    conv->turns.push_back(Turn{Turn::Who::Tarnished, std::move(line)});
    conv->idle_from = now;
    if (!in->world.speech || in->world.dormant) {  // C4: nothing goes out while gated
      failure(ToastClass::Provider, proto::ToastLevel::Error, kNoModel);
      return;
    }
    ReplyRun r;
    r.id = conv->next_reply++;
    r.submitted = now;
    conv->reply = std::move(r);
    open_parts(*conv->reply);
    if (!send(*conv->reply)) failure(ToastClass::Provider, proto::ToastLevel::Error, kNoPrompt);
  }

  // NPC §6.2: the "…" page, never acknowledged or committed, and a toast; back to Open. A queued line goes next.
  void failure(ToastClass cls, proto::ToastLevel level, std::string_view text) {
    conv->failure_reply = conv->reply ? conv->reply->id : conv->next_reply++;
    if (conv->reply) {
      cancel_request(*conv->reply);
      if (conv->reply->lint) conv->tally = conv->reply->lint->tally();
      conv->reply.reset();
    }
    conv->failure_until = now + T::kFailureMs;
    toast(cls, level, text);
    if (conv->queued) {
      std::string q = std::move(*conv->queued);
      conv->queued.reset();
      dispatch(std::move(q));
    }
  }

  void failure_for(proto::ErrorClass e) {
    switch (e) {
      case proto::ErrorClass::Auth:
        failure(ToastClass::Auth, proto::ToastLevel::Error, kKeyRejected);
        return;
      case proto::ErrorClass::RateLimited:
        failure(ToastClass::RateLimit, proto::ToastLevel::Error, kRateLimited);
        return;
      case proto::ErrorClass::FirstTokenTimeout:
      case proto::ErrorClass::TotalTimeout:
        failure(ToastClass::Timeout, proto::ToastLevel::Error, kTimedOut);
        return;
      default:
        failure(ToastClass::Provider, proto::ToastLevel::Error, kProviderFailed);
        return;
    }
  }

  // ---- The model's events ----

  void on_llm(const proto::LlmEvent& e) {
    if (!conv || !conv->reply || !conv->reply->live || e.id != conv->reply->request) return;  // a cancelled one's
    ReplyRun& r = *conv->reply;
    switch (e.kind) {
      case proto::EventKind::Delta: {
        std::vector<Sentence> sentences;
        const Flow flow = r.stream->feed(e.text, sentences);
        const proto::RequestId request = r.request;
        if (!r.sentences_done) take(r, sentences);
        // The lint may have regenerated (a new request) or failed the reply (none, or the queued line's).
        if (conv && conv->reply && conv->reply->request == request && r.live && !r.sentences_done) {
          if (flow == Flow::IntentOnly) end_sentences(r);
          if (flow == Flow::Stop) {
            cancel_request(r);
            end_sentences(r);
          }
        }
        return;
      }
      case proto::EventKind::Reset:
        if (r.pages.empty() && !r.on_screen) {
          open_parts(r);  // nothing of it is up: start the reply again
        } else {
          r.stream->withdraw();
        }
        return;
      case proto::EventKind::Ended: {
        r.live = false;
        const proto::ErrorClass err = e.chat ? e.chat->error : e.error();
        if (err == proto::ErrorClass::None || r.kept_any) {
          std::vector<Sentence> sentences;
          r.stream->finish(sentences);
          const proto::RequestId request = r.request;
          if (!r.sentences_done) take(r, sentences);
          if (conv && conv->reply && conv->reply->request == request && !r.sentences_done && !r.live) end_sentences(r);
          return;
        }
        if (err == proto::ErrorClass::Cancelled) return;
        failure_for(err);
        return;
      }
      default:
        return;
    }
  }

  void end_sentences(ReplyRun& r) {
    if (r.sentences_done) return;
    r.sentences_done = true;
    r.pager->end();
  }

  // Each sentence through the lint and into the pager, in order (NPC §8.2 steps 8 to 11, §8.4).
  void take(ReplyRun& r, std::vector<Sentence>& sentences) {
    for (Sentence& s : sentences) {
      if (r.sentences_done) return;
      const LintVerdict v = r.lint->check(s);
      switch (v.action) {
        case LintAction::Pass: {
          const Take t = r.pager->push(std::move(s));
          if (t != Take::Over) r.kept_any = true;
          if (t != Take::Kept) {  // the reply is full (§8.5): Full, or Over when no Full came first
            cancel_request(r);
            end_sentences(r);
            return;
          }
          break;
        }
        case LintAction::Drop:
          break;
        case LintAction::Regenerate:
          if (!r.regenerated) {
            regenerate(r, v.hit);
            return;
          }
          [[fallthrough]];
        case LintAction::Deflect:
          deflect(r);
          return;
        case LintAction::Stop:
          cancel_request(r);
          end_sentences(r);
          return;
      }
    }
  }

  // §8.4: cancel, and resend once with block 11's reminder; a fresh first-token timer, the same 20 s total.
  void regenerate(ReplyRun& r, std::optional<HardList> hit) {
    cancel_request(r);
    r.regenerated = true;
    r.reminder = hit ? hit : std::optional<HardList>(HardList::Banned);
    open_parts(r);
    if (!send(r)) failure(ToastClass::Provider, proto::ToastLevel::Error, kNoPrompt);
  }

  // §8.4: the persona's own deflection in place of the reply (content, never an invented answer).
  void deflect(ReplyRun& r) {
    cancel_request(r);
    const Persona& p = cast()->persona(conv->persona);
    if (p.deflections.empty()) {
      failure(ToastClass::Provider, proto::ToastLevel::Error, kProviderFailed);
      return;
    }
    // The least recently used line in this session (DeflectionRing; the store keeps its order per journey, v0.3).
    auto ring = rings.find(conv->persona_id);
    if (ring == rings.end()) ring = rings.emplace(conv->persona_id, DeflectionRing(p.deflections.size())).first;
    if (ring->second.order().size() != p.deflections.size()) ring->second = DeflectionRing(p.deflections.size());
    Sentence s;
    s.text = p.deflections[ring->second.next().value_or(0) % p.deflections.size()];
    if (r.pager->push(std::move(s)) != Take::Over) r.kept_any = true;
    end_sentences(r);
  }

  // ---- The surfaces' events ----

  void on_page_shown(const proto::UiEvent::PageShown& e) {
    const Mono t = std::min<Mono>(e.t_ms, now);
    if (conv && e.page.conv == conv->id) {
      if (conv->reply && e.page.reply == conv->reply->id) {
        ReplyRun& r = *conv->reply;
        if (!r.on_screen || *r.on_screen != e.page.index || r.shown_at) return;  // a duplicate, or not the page up
        r.shown_at = t;
        commit_page(r.id, e.page.index, r.pages[e.page.index].text);
        r.acked = static_cast<std::uint8_t>(e.page.index + 1);
        // §6.3: the queued line goes when the reply's last page starts showing, so generation overlaps the reading.
        if (r.pages[e.page.index].last && conv->queued) {
          to_tail(r, r.pages[e.page.index].hold_ms);
          std::string q = std::move(*conv->queued);
          conv->queued.reset();
          dispatch(std::move(q));
        }
        return;
      }
      if (conv->tail && conv->tail->view.id == e.page && !conv->tail->shown_at) {
        conv->tail->shown_at = t;
        commit_page(e.page.reply, e.page.index, conv->tail->view.text);
      }
      return;
    }
    // §8.7 item 4: the last reply's acks, 500 ms after an end.
    for (auto it = grace.begin(); it != grace.end(); ++it) {
      if (it->conv == e.page.conv && it->reply == e.page.reply && it->index == e.page.index && now <= it->until) {
        Commit c;
        c.kind = it->index == 0 ? Commit::Kind::NpcSaid : Commit::Kind::NpcPage;
        c.conv = it->conv;
        c.reply = it->reply;
        c.page = it->index;
        c.persona = it->persona;
        c.text = it->text;
        commit(std::move(c));
        grace.erase(it);
        return;
      }
    }
  }

  void commit_page(proto::ReplyId reply, std::uint8_t index, const std::string& text) {
    Commit c;
    c.kind = index == 0 ? Commit::Kind::NpcSaid : Commit::Kind::NpcPage;
    c.reply = reply;
    c.page = index;
    c.persona = conv->persona_id;
    c.text = text;
    commit(std::move(c));
    if (index == 0) {
      ++conv->committed;
      conv->strip.push_back(proto::HistoryLine{proto::HistoryLine::Who::Npc, conv->name, text, false});
      conv->turns.push_back(Turn{Turn::Who::Persona, text});
    } else if (!conv->strip.empty()) {
      conv->strip.back().text += ' ' + text;
      conv->turns.back().text += ' ' + text;
    }
  }

  // The reply's page up becomes the tail, staying `stay` from its first frame; the reply itself is done.
  void to_tail(ReplyRun& r, Mono stay) {
    Tail t;
    t.view = page_view(r, *r.on_screen);
    t.shown_at = r.shown_at;
    t.stay_ms = stay;
    t.created = now;
    conv->tail = std::move(t);
    cancel_request(r);
    if (r.lint) conv->tally = r.lint->tally();
    conv->reply.reset();
  }

  void on_skip(const proto::UiEvent::Skip& e) {
    if (!conv) return;
    if (conv->reply && conv->reply->on_screen && conv->reply->shown_at &&
        e.page == proto::PageId{conv->id, conv->reply->id, *conv->reply->on_screen}) {
      ReplyRun& r = *conv->reply;
      const Mono end = std::max(*r.shown_at + T::kSkipMinMs, now);
      const Mono hold = end - *r.shown_at;
      r.pages[*r.on_screen].hold_ms = static_cast<std::uint32_t>(std::min<Mono>(hold, r.pages[*r.on_screen].hold_ms));
      return;
    }
    if (conv->tail && conv->tail->shown_at && e.page == conv->tail->view.id) {
      const Mono end = std::max(*conv->tail->shown_at + T::kSkipMinMs, now);
      conv->tail->stay_ms = std::min(conv->tail->stay_ms, end - *conv->tail->shown_at);
    }
  }

  void on_close(const proto::UiEvent::Close& e) {
    if (!conv) {
      if (e.reason == proto::CloseReason::Shut && shut_for && now - shut_for->second <= 5000) {
        keep_draft(shut_for->first, e.draft);
        shut_for.reset();
      }
      return;
    }
    // Esc acts on the box only while it is open (WHY§461): with the box closed the key is the game's, and no press
    // stops a reply that paces.
    if (e.reason == proto::CloseReason::Escape && !conv->box_open) return;
    conv->box_open = false;
    // A box refused at its opening never held input: the store's copy of its draft stays as it was (kept_draft).
    if (e.reason != proto::CloseReason::Refused && e.reason != proto::CloseReason::NotNow) {
      keep_draft(conv->persona_id, e.draft);
    }
    switch (e.reason) {
      case proto::CloseReason::Escape: {
        const bool pending = conv->reply.has_value() || conv->tail.has_value();
        if (pending) {
          conv->esc_closed = true;  // the first: the reply finishes pacing, then the conversation ends
        } else {
          end(EndReason::Closed, false);
        }
        return;
      }
      case proto::CloseReason::Refused:
      case proto::CloseReason::NotNow:
        toast(ToastClass::Input, proto::ToastLevel::Info, input_refusal(e.reason, e.detail));
        if (conv->player_lines == 0) end(EndReason::Closed, true);
        return;
      case proto::CloseReason::Fault:
        toast(ToastClass::Fault, proto::ToastLevel::Error, kFault);
        end(EndReason::Fault, true);
        return;
      case proto::CloseReason::FocusLost:
      case proto::CloseReason::InputLost:
      case proto::CloseReason::Shut:
        return;  // the box is closed and its draft kept; the reply paces on
    }
  }

  // ---- Endings ----

  void end(EndReason why, bool at_once) {
    if (!conv) return;
    if (!at_once) {
      // After the current page: the stream stops and what is not up is dropped; the page up plays out.
      conv->ending = true;
      conv->end_reason = why;
      conv->queued.reset();
      if (conv->reply) {
        ReplyRun& r = *conv->reply;
        if (r.on_screen) {
          to_tail(r, r.pages[*r.on_screen].hold_ms);
        } else {
          cancel_request(r);
          conv->reply.reset();
        }
      }
      if (!conv->tail) finish(why);
      return;
    }
    // At once: what is up but not yet acknowledged may still be acknowledged for 500 ms (§8.7 item 4).
    if (conv->reply) {
      ReplyRun& r = *conv->reply;
      cancel_request(r);
      if (r.on_screen && !r.shown_at) {
        grace.push_back(Grace{conv->id, r.id, conv->persona_id, *r.on_screen, r.pages[*r.on_screen].text,
                              now + T::kGraceMs});
      }
    }
    if (conv->tail && !conv->tail->shown_at) {
      const proto::PageId id = conv->tail->view.id;
      grace.push_back(Grace{id.conv, id.reply, conv->persona_id, id.index, conv->tail->view.text, now + T::kGraceMs});
    }
    finish(why);
  }

  void finish(EndReason why) {
    if (conv->reply) cancel_request(*conv->reply);
    Commit c;
    c.kind = Commit::Kind::Ended;
    c.persona = conv->persona_id;
    c.reason = why;
    c.player_lines = conv->player_lines;
    c.committed = conv->committed;
    commit(std::move(c));
    // The memory job (≥ 2 player lines and ≥ 4 committed, DESIGN §8.6) is v0.3's, with the store.
    ++epoch;
    quiet_until = now + T::kQuietBarksMs;
    out->ended = why;
    shut_for = std::make_pair(conv->persona_id, now);
    const std::optional<proto::ChrHandle> next = conv->retarget;
    conv.reset();
    if (next) {
      if (const Candidate* c2 = candidate_of(*next); c2 != nullptr && would_open(*c2)) open(*c2);
    }
  }

  // ---- Time passing ----

  void advance() {
    std::erase_if(grace, [&](const Grace& g) { return now > g.until; });
    std::erase_if(drafts, [&](const KeptDraft& d) { return now >= d.until; });
    if (!conv) return;
    const ConversationWorld& w = in->world;
    if (w.loading) return end(EndReason::Travel, true);
    if (w.dormant || !w.in_world) return end(EndReason::Dormant, true);
    if (!w.chat) return end(EndReason::Closed, true);  // features.chat turned off (ARCH §11.6: the next step)
    if (w.player_died || around.player_dead) return end(EndReason::Death, true);
    if (around.degraded) {
      toast(ToastClass::Degraded, proto::ToastLevel::Warning, kDegraded);
      return end(EndReason::Degraded, true);
    }
    // The partner (§5.7): unresolvable for 2 s, dead, hostile, at a muted or refusing stage, asleep.
    const Candidate* c = candidate_of(conv->handle);
    const bool same = c != nullptr && c->binding.persona == conv->persona;
    if (!same) {
      if (!conv->missing_since) conv->missing_since = now;
      if (now - *conv->missing_since >= T::kUnresolvedMs) return end(EndReason::NpcState, true);
    } else {
      conv->missing_since.reset();
      const PartnerFacts f = facts_of(*c);
      const Conscious st = f.persona->state_at(f.stage);
      if (f.chr->dead || f.hostile || f.persona->chat_at(f.stage) != ChatMode::Allowed || st == Conscious::Asleep ||
          st == Conscious::Senseless) {
        return end(EndReason::NpcState, true);
      }
      // Native talk with the partner (before S3: any native talk while the partner is within 3 m).
      if (around.native && f.chr->dist_m <= T::kNativePartnerM) return end(EndReason::NativeTalk, true);
      if (f.chr->dist_m > T::kWalkAwayM) {
        if (!conv->far_since) conv->far_since = now;
        if (now - *conv->far_since >= T::kWalkAwayMs) return end(EndReason::WalkedAway, true);
      } else {
        conv->far_since.reset();
      }
    }
    // Danger (§5.4): the box closes at once, input released, the draft kept; pages shown pace on. Any failing clause of
    // safe_context closes the box; only a threat (the Tarnished down or riding, a boss, combat, hostiles near) counts
    // towards the 10 s that end the conversation: a menu or a hidden overlay holds the pages instead (§5.8, 30 s),
    // and a model that is not ready leaves the conversation open, to its idle timer (impl).
    // A game banner never closes an open box (O108): it holds the pages (w.hidden) and stops only the opening.
    Surroundings open_around = around;
    open_around.banner = false;
    const std::optional<Unsafe> unsafe = safe_context(open_around);
    const bool threat = hp_dropped || w.player_died ||
                        (unsafe && *unsafe != Unsafe::Menu && *unsafe != Unsafe::NotActive);
    if ((unsafe || threat) && conv->box_open) {
      conv->box_open = false;  // the overlay answers Close{Shut} with the draft
      toast(ToastClass::Danger, proto::ToastLevel::Info, kMomentPassed);
    }
    const bool threat_lasts = unsafe && *unsafe != Unsafe::Menu && *unsafe != Unsafe::NotActive;
    if (threat) {
      if (!conv->danger_since) conv->danger_since = now;
    } else if (!threat_lasts) {
      conv->danger_since.reset();
    }
    if (conv->danger_since && threat_lasts && now - *conv->danger_since >= T::kDangerMs) {
      return end(EndReason::Danger, true);
    }
    hold();
    if (!conv) return;
    if (conv->hold == Hold::None && now - conv->idle_from >= T::kIdleMs) return end(EndReason::Idle, true);
    if (conv->hold == Hold::None) pump();
  }

  [[nodiscard]] bool pacing() const {
    return conv->reply.has_value() || conv->tail.has_value();
  }

  // §5.8: native talk within 20 m (not the partner's) or the overlay hidden hold what is not yet shown.
  void hold() {
    const bool native = around.native;
    const bool hidden = around.hidden;
    if (conv->hold == Hold::None) {
      if (!pacing() || (!native && !hidden)) return;
      conv->hold = native ? Hold::Native : Hold::Hidden;
      conv->held_since = now;
      conv->native_ended_at.reset();
      return;
    }
    const Mono held = now - conv->held_since;
    bool resume = false;
    if (conv->hold == Hold::Native) {
      if (hidden && !native) {
        conv->hold = Hold::Hidden;  // the reason changed; the held time counts on
      } else if (!native) {
        if (!conv->native_ended_at) conv->native_ended_at = now;
        resume = now - *conv->native_ended_at >= T::kNativeResumeMs;
      } else {
        conv->native_ended_at.reset();
      }
      if (!resume && held >= T::kNativeDropMs) return drop_held();
    } else {
      if (native) {
        conv->hold = Hold::Native;
      } else {
        resume = !hidden;
      }
      if (!resume && held >= T::kHiddenDropMs) return drop_held();
    }
    if (resume) {
      // The timers paused while held: they move on by the time held.
      conv->idle_from += held;
      if (conv->reply) {
        conv->reply->submitted += held;
        if (conv->reply->shown_at) *conv->reply->shown_at += held;
        conv->reply->published_at += held;
      }
      if (conv->tail) {
        if (conv->tail->shown_at) *conv->tail->shown_at += held;
        conv->tail->created += held;
      }
      if (conv->failure_until) *conv->failure_until += held;
      conv->hold = Hold::None;
    }
  }

  // Held too long: what was not shown is dropped, never committed; back to Open.
  void drop_held() {
    conv->hold = Hold::None;
    if (conv->reply) {
      ReplyRun& r = *conv->reply;
      cancel_request(r);
      if (r.lint) conv->tally = r.lint->tally();
      conv->reply.reset();
    }
    conv->tail.reset();
    conv->queued.reset();
    conv->idle_from = now;
    after_reply();
  }

  void after_reply() {
    if (conv->ending && !conv->tail && !conv->reply) return finish(conv->end_reason);
    if (conv->esc_closed && !conv->tail && !conv->reply) return end(EndReason::Closed, true);
    if (conv->queued && !conv->reply) {
      std::string q = std::move(*conv->queued);
      conv->queued.reset();
      dispatch(std::move(q));
    }
  }

  void publish(ReplyRun& r, std::uint8_t index) {
    r.on_screen = index;
    r.shown_at.reset();
    r.published_at = now;
  }

  void pump() {
    if (conv->failure_until && now >= *conv->failure_until) conv->failure_until.reset();
    if (conv->tail) {
      Tail& t = *conv->tail;
      const bool gone = t.shown_at ? now >= *t.shown_at + t.stay_ms : now - t.created >= T::kUnackedTailMs;
      if (gone) {
        conv->tail.reset();
        if (!conv->reply) return after_reply();
      }
    }
    if (!conv->reply) return;
    ReplyRun& r = *conv->reply;
    // 20 s from Submit with nothing shown: the "…" page and a toast (§6.2).
    if (r.acked == 0 && !r.shown_at && now - r.submitted >= T::kTotalMs) {
      return failure(ToastClass::Timeout, proto::ToastLevel::Error, kTimedOut);
    }
    if (r.on_screen) {
      if (settle_last(r)) return;  // the queued line went: the reply is the tail now
      if (!r.shown_at || now < *r.shown_at + r.pages[*r.on_screen].hold_ms) return;
      const auto next = static_cast<std::uint8_t>(*r.on_screen + 1);
      if (next < r.pages.size()) return publish(r, next);
      const Sealing s = r.pager->next(now);
      if (s.kind == Sealing::Kind::Page) {
        r.pages.push_back(s.page);
        return publish(r, next);
      }
      if (s.kind == Sealing::Kind::Wait) return;  // the page stays up until the next is sealed
      return done(r);
    }
    if (conv->tail || conv->failure_until) return;  // page 1 waits for the page up (§6.3)
    if (!r.kept_any && !r.sentences_done) return;
    const Sealing s = r.pager->next(now);
    if (s.kind == Sealing::Kind::Page) {
      r.pages.push_back(s.page);
      return publish(r, 0);
    }
    if (s.kind == Sealing::Kind::Done) {
      if (r.pages.empty()) return failure(ToastClass::Provider, proto::ToastLevel::Error, kProviderFailed);
      done(r);
    }
  }

  // §6.3 (WHY§432): a page sealed while the reply still streamed is not marked last, so once no sentence follows, the
  // pager is asked for the page after the one up: with nothing left, the page up is the reply's last, and a queued line
  // goes as soon as it shows (now, if it has), not when its hold ends. A page that is left is sealed at once; with no
  // sentence to come, sealing it now gives the page it would give at the hold's end. True when the queued line went.
  bool settle_last(ReplyRun& r) {
    if (!r.sentences_done || !r.on_screen || *r.on_screen + 1U != r.pages.size()) return false;
    const std::uint8_t up = *r.on_screen;
    if (!r.pages[up].last) {
      const Sealing s = r.pager->next(now);
      if (s.kind == Sealing::Kind::Page) {
        r.pages.push_back(s.page);
        return false;
      }
      if (s.kind != Sealing::Kind::Done) return false;
      r.pages[up].last = true;
    }
    if (!r.shown_at || !conv->queued) return false;
    to_tail(r, r.pages[up].hold_ms);
    std::string q = std::move(*conv->queued);
    conv->queued.reset();
    dispatch(std::move(q));
    return true;
  }

  // The reply's last page has held: back to Open, and what waits goes next.
  void done(ReplyRun& r) {
    cancel_request(r);
    if (r.lint) conv->tally = r.lint->tally();
    conv->reply.reset();
    after_reply();
  }

  // ---- The model ----

  [[nodiscard]] proto::ConvState state() const {
    if (!conv) return proto::ConvState::Closed;
    if (conv->ending) return proto::ConvState::Ending;
    if (conv->hold != Hold::None) return proto::ConvState::Held;
    if (conv->reply) {
      const ReplyRun& r = *conv->reply;
      if (r.acked == 0) return proto::ConvState::Awaiting;
      return r.live || !r.sentences_done ? proto::ConvState::Streaming : proto::ConvState::Paging;
    }
    if (conv->tail) return proto::ConvState::Paging;
    return proto::ConvState::Open;
  }

  [[nodiscard]] proto::UiModel model() const {
    proto::UiModel m;
    m.toasts = toasts.visible();
    if (!conv) {
      if (stable && snap() != nullptr && cast() != nullptr && in->world.chat) {
        if (const Candidate* c = candidate_of(*stable); c != nullptr && would_open(*c)) {
          m.hint = proto::TalkHint{proto::HintVerb::Speak, cast()->persona(c->binding.persona).display_name_fallback,
                                   *stable};
        }
      }
      return m;
    }
    const proto::ConvState st = state();
    proto::ChatBox& box = m.chat;
    box.conv = conv->id;
    box.state = st;
    box.partner = conv->name;
    box.heard_by = {conv->name};  // one partner in v0.2 (O97)
    box.history = conv->strip;
    if (conv->queued) box.history.push_back(proto::HistoryLine{proto::HistoryLine::Who::Tarnished, "", *conv->queued, true});
    box.draft = conv->box_draft;
    const bool strip = surface() == proto::Surface::HistoryStrip;
    box.view = conv->box_open ? proto::BoxView::Open : (strip && pacing() ? proto::BoxView::Strip : proto::BoxView::Closed);
    if (st == proto::ConvState::Held) return m;  // nothing over the game's own talk, and nothing while hidden
    if (conv->failure_until) {
      proto::PageView v;
      v.id = proto::PageId{conv->id, conv->failure_reply, 0};
      v.kind = proto::PageKind::Failure;
      v.surface = surface();
      v.speaker = conv->name;
      v.text = std::string(kEllipsis);
      v.last = true;
      v.hold_ms = static_cast<std::uint32_t>(T::kFailureMs);
      m.page = std::move(v);
      return m;
    }
    if (conv->tail) {
      m.page = conv->tail->view;
      return m;
    }
    if (conv->reply) {
      const ReplyRun& r = *conv->reply;
      if (r.on_screen) {
        m.page = page_view(r, *r.on_screen);
      } else if (now - r.submitted >= T::kCardMs) {
        proto::PageView v;
        v.id = proto::PageId{conv->id, r.id, 0};
        v.kind = proto::PageKind::Card;
        v.surface = surface();
        v.speaker = conv->name;
        v.count = 0;
        m.page = std::move(v);
      }
    }
    return m;
  }

  ConversationOutput step(const ConversationInput& input) {
    ConversationOutput o;
    in = &input;
    out = &o;
    now = input.now;
    // Content loaded anew: what was read from the old content goes (v0.2 loads content once; impl).
    const std::optional<std::uint64_t> hash =
        input.world.cast != nullptr ? std::optional<std::uint64_t>(input.world.cast->pack().hash) : std::nullopt;
    if (hash != cast_hash) {
      cast_hash = hash;
      candidates.clear();
      best.reset();
      stable.reset();
      approached_at.clear();
      if (conv) {
        conv->tail.reset();  // what is up was read from the old content: dropped, never committed
        conv->retarget.reset();
        finish(EndReason::Fault);  // cancels the live request
      }
    }
    if (input.world.snapshot != nullptr && input.world.fresh) observe(*input.world.snapshot);
    refresh_flags();
    if (input.ui != nullptr) {
      std::visit(
          [&](const auto& ev) {
            using E = std::decay_t<decltype(ev)>;
            if constexpr (std::is_same_v<E, proto::UiEvent::OpenChat>) {
              on_open_chat();
            } else if constexpr (std::is_same_v<E, proto::UiEvent::Submit>) {
              on_submit(ev.text);
            } else if constexpr (std::is_same_v<E, proto::UiEvent::PageShown>) {
              on_page_shown(ev);
            } else if constexpr (std::is_same_v<E, proto::UiEvent::Close>) {
              on_close(ev);
            } else if constexpr (std::is_same_v<E, proto::UiEvent::Skip>) {
              on_skip(ev);
            }
          },
          input.ui->event);
    }
    if (input.llm != nullptr) on_llm(*input.llm);
    advance();
    toasts.step(now, around.boss);
    o.ui = model();
    in = nullptr;
    out = nullptr;
    return o;
  }
};

Conversation::Conversation(SpeechParts parts) : parts_(parts), impl_(std::make_unique<Impl>(parts_)) {}
Conversation::~Conversation() = default;

ConversationOutput Conversation::step(const ConversationInput& in) { return impl_->step(in); }

proto::ConvState Conversation::state() const noexcept { return impl_->state(); }
bool Conversation::box_open() const noexcept { return impl_->conv && impl_->conv->box_open; }
std::optional<std::string> Conversation::partner() const {
  if (!impl_->conv) return std::nullopt;
  return impl_->conv->persona_id;
}
std::optional<std::string> Conversation::queued() const {
  if (!impl_->conv) return std::nullopt;
  return impl_->conv->queued;
}
std::optional<Mono> Conversation::barks_quiet_until() const noexcept { return impl_->quiet_until; }
std::uint64_t Conversation::epoch() const noexcept { return impl_->epoch; }
std::optional<proto::RequestId> Conversation::live_request() const noexcept {
  if (!impl_->conv || !impl_->conv->reply || !impl_->conv->reply->live) return std::nullopt;
  return impl_->conv->reply->request;
}

}  // namespace en::core
