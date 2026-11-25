// Tools › Workshop's keeping in the DLL (dll.hpp; PAGES §4.6.8, SP8; en_shell's workshop.hpp), T6's alone: on and off
// with the space's own `debug.panel` (the value it publishes, which the page follows at once: GraceUI D258's
// shown_with, the owner's O130 (a)); fed each chat request ai_submit sends and each of the model client's events ai_pump takes,
// each of Augur's events of a step, and each new snapshot's characters; and, each wake while a Workshop topic is
// watched, the world view, the known sets and the kinds built from the core, at most once a second. Going off empties
// it at once. Nothing of it is written anywhere, logged or recorded: one log line says Workshop came on or went off.
//
// Every text it keeps is scrubbed first (ai_redact: each configured key's value and anything shaped like a key; then
// the playthrough's name, matched whole: PV2).
#include "dll.hpp"
#include "en/proto/ffi_guard.hpp"
#include "en/shell/workshop.hpp"

namespace eldennet {

using en::shell::Line;

namespace {

struct WorkshopState {
  en::shell::Workshop keeper;
  bool on = false;
  std::string name;            // the playthrough's, for the mask; empty before one
  std::uint64_t snapshot = 0;  // the newest snapshot taken (its seq)
  std::uint64_t built_ms = 0;  // when the world view, the known sets and the kinds were last built
  bool built = false;
};
constinit WorkshopState* g_workshop = nullptr;

WorkshopState& workshop_state(Shell& s) {
  if (g_workshop == nullptr) {
    g_workshop = new WorkshopState();
    g_workshop->keeper.set_scrub([&s](std::string_view text) {
      return en::shell::mask_name(ai_redact(s, text), g_workshop != nullptr ? g_workshop->name : std::string());
    });
  }
  return *g_workshop;
}

// The space's own value, as it publishes it; off before the space exists.
bool space_on() noexcept {
  const en::overlay::Space* space = overlay_space();
  return space != nullptr && space->workshop_on();
}

std::uint64_t since_of(std::uint64_t mono) noexcept {
  const std::uint64_t start = mono_ms() - since_ms();
  return mono >= start ? mono - start : 0;
}

}  // namespace

void workshop_sent(Shell& s, const en::proto::LlmRequest& request, en::proto::CallPurpose purpose) noexcept {
  en::proto::ffi_guard(en::proto::Component::DllMain, [&] {
    if (!space_on()) return;
    workshop_state(s).keeper.sent(request, purpose, since_ms());
  });
}

void workshop_llm(Shell& s, const en::proto::LlmEvent& event) noexcept {
  en::proto::ffi_guard(en::proto::Component::DllMain, [&] {
    if (!space_on()) return;
    workshop_state(s).keeper.event(event, since_ms());
  });
}

void workshop_events(Shell& s, const std::vector<en::augur::Event>& events) noexcept {
  en::proto::ffi_guard(en::proto::Component::DllMain, [&] {
    en::overlay::Space* space = overlay_space();
    if (events.empty() || space == nullptr || !space->workshop_on()) return;
    WorkshopState& st = workshop_state(s);
    for (const en::augur::Event& e : events) space->workshop_event(st.keeper.event_row(e, since_of(e.t_mono_ms)));
  });
}

const en::proto::WorkshopView* workshop_tick(Shell& s, const en::core::Core& core,
                                             const en::proto::WorldSnapshot* latest, en::overlay::Space& space,
                                             std::uint64_t now) noexcept {
  return en::proto::ffi_guard(en::proto::Component::DllMain, static_cast<const en::proto::WorkshopView*>(nullptr), [&] {
    WorkshopState& st = workshop_state(s);
    const bool on = space.workshop_on();
    if (on != st.on) {
      st.on = on;
      st.keeper.clear();  // on: from nothing; off: everything kept, gone
      st.snapshot = 0;
      st.built = false;
      st.name.clear();
      log(Line("workshop").flag("on", on));
    }
    if (!on) return static_cast<const en::proto::WorkshopView*>(nullptr);
    st.name = core.playthrough() ? std::string(core.playthrough()->name.view()) : std::string();
    if (latest != nullptr && latest->seq != st.snapshot) {
      st.snapshot = latest->seq;
      st.keeper.see(*latest, core.directory().index(), since_of(latest->mono_ms));
    }
    const en::augur::BusStats& bus = core.bus().stats();
    st.keeper.drops(bus.dropped_t, bus.dropped_k);
    en::proto::WorkshopView& v = st.keeper.view();
    v.recording = s.record;  // the recorder's state, as core_main keeps it (shell.cpp)
    // What the core holds now, built while it is watched, at most once a second.
    const bool watched = space.watching(en::overlay::kWorkshopWorld) || space.watching(en::overlay::kWorkshopKnown) ||
                         space.watching(en::overlay::kWorkshopKinds);
    if (watched && (!st.built || now - st.built_ms >= 1000)) {
      st.built = true;
      st.built_ms = now;
      v.world = en::shell::Workshop::world_of(core);
      v.known = en::shell::Workshop::known_of(core);
      v.kinds = en::shell::Workshop::kinds_of(core);
    }
    return static_cast<const en::proto::WorkshopView*>(&v);
  });
}

}  // namespace eldennet
