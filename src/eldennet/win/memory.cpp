// The memory pages in the DLL (dll.hpp; S12.2.a, S12.2.b; PAGES §4.6.1, §4.6.4, §4.6.5): their facts (en-overlay's
// memory.hpp) from the store's page queries, sent to T9 from the pages' own sequence numbers (en-proto's kPageSeq up)
// and answered in C8's results, and from the core (the open journey, who is perceived now, who is speaking); and the
// space's requests: a forget goes to the core, which alone writes the store (CoreInput::Forget), and a transcript to
// T9.
//
// Read again when the file opens, after each forget the core sent, when a conversation ends, and every 5 s while the
// file is open; only while it is open (never during the rollback choice). Chat and the memory job's words reach the
// facts for the pages that show them alone: nothing here logs them (PV4); a log line is a count.
#include <algorithm>
#include <map>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "dll.hpp"
#include "en/core/facts.hpp"
#include "en/core/memory.hpp"
#include "en/proto/ffi_guard.hpp"

namespace eldennet {

using en::shell::Line;
using en::shell::LogLevel;
namespace db = en::proto::db;
namespace ov = en::overlay;

struct MemoryLink {
  ov::MemoryFacts facts;
  std::uint64_t next_seq = en::proto::db::kPageSeq;
  std::map<std::uint64_t, db::PageQuery> pending;  // the queries in flight, by sequence number
  bool open = false;                               // the file was open at the last tick
  std::uint64_t forgets = 0;                       // the core's forgets seen
  std::uint64_t refreshed = 0;                     // when the pages' data were last asked for (mono ms)
  bool talking = false;
  std::map<std::uint16_t, ov::JourneyFacts> journeys;  // being gathered
  std::uint64_t read = 0;                          // answers taken, for the log
};

namespace {

constinit MemoryLink* g_memory = nullptr;  // T6's alone
constexpr std::uint64_t kRefreshMs = 5000;

void ask(Shell& s, MemoryLink& m, db::PageQuery q) {
  const std::uint64_t seq = m.next_seq++;
  m.pending[seq] = q;
  std::vector<en::proto::DbRequest> one;
  one.push_back(en::proto::DbRequest{seq, q});
  store_send(s, one);
}

void refresh(Shell& s, MemoryLink& m, std::uint64_t now) {
  m.refreshed = now;
  ask(s, m, db::PageQuery{db::PageQuery::What::Journeys});
  ask(s, m, db::PageQuery{db::PageQuery::What::Backups});
}

std::string name_of(const en::core::Core& core, const std::string& persona) {
  if (const auto i = core.directory().find(persona)) return core.directory().persona(*i).display_name_fallback;
  return "Someone you know";
}

std::optional<std::uint64_t> number(const std::vector<en::proto::FactRow>& facts, std::string_view key) {
  for (const en::proto::FactRow& f : facts) {
    if (f.key != key) continue;
    std::uint64_t v = 0;
    for (const char c : f.value) {
      if (c < '0' || c > '9') return std::nullopt;
      v = v * 10 + static_cast<std::uint64_t>(c - '0');
    }
    return v;
  }
  return std::nullopt;
}

ov::MemberFacts member_of(const en::core::Core& core, const db::MemoryCard& c) {
  namespace fact = en::core::fact;
  ov::MemberFacts m;
  m.persona = c.persona;
  m.name = name_of(core, c.persona);
  m.met = static_cast<std::uint32_t>(number(c.facts, fact::kMetCount).value_or(0));
  m.seen = static_cast<std::uint32_t>(number(c.facts, fact::kSeenCount).value_or(0));
  m.struck = static_cast<std::uint32_t>(number(c.facts, fact::kTimesAttacked).value_or(0));
  m.fought_beside = static_cast<std::uint32_t>(number(c.facts, fact::kFoughtBeside).value_or(0));
  m.saw_fall = static_cast<std::uint32_t>(number(c.facts, fact::kSeenPlayerDie).value_or(0));
  m.first_met_pt = number(c.facts, fact::kFirstMetPt);
  m.last_met_pt = number(c.facts, fact::kLastMetPt);
  if (c.journal) {
    if (const std::optional<en::core::Journal> j = en::core::journal_of(*c.journal)) {
      m.journal = true;
      m.summary = j->summary;
      m.disposition = j->disposition;
      m.written_pt = c.journal->as_of_pt;
      if (c.journal->told_name) m.told_name = *c.journal->told_name;
      const std::optional<std::size_t> kept = en::core::protected_keepsake(*j);
      for (std::size_t i = 0; i < j->keepsakes.size(); ++i) {
        const en::core::Keepsake& k = j->keepsakes[i];
        ov::KeepsakeFacts out;
        out.text = k.text;
        out.weight = k.weight;
        out.told = k.basis == en::core::Keepsake::Basis::Said;
        out.about = k.about == "tarnished" ? "You"
                    : k.about == "self"    ? "Themselves"
                    : k.about == "world"   ? "The world"
                                           : name_of(core, k.about);
        out.kept = kept && *kept == i;
        out.pt = static_cast<std::uint64_t>(std::max<std::int64_t>(0, k.since_pt));
        m.keepsakes.push_back(std::move(out));
      }
    }
  }
  m.queued = c.queued;
  m.running = c.running;
  if (c.last) m.last_job = en::proto::to_string(*c.last);
  return m;
}

}  // namespace

void memory_tick(Shell& s, const en::core::Core& core, std::uint64_t now) noexcept {
  en::proto::ffi_guard(en::proto::Component::DllMain, [&] {
    if (g_memory == nullptr) g_memory = new MemoryLink();  // lives for the process (R14)
    MemoryLink& m = *g_memory;
    const auto& store = core.store();
    const bool open = store.mode() == en::core::StorePath::Mode::Open;
    const bool talking = core.conversation().state() != en::proto::ConvState::Closed;
    if (!open) {
      if (m.open || m.facts.loaded) {
        m.facts = ov::MemoryFacts{};
        m.facts.revision = m.read + 1;
        m.journeys.clear();
        m.pending.clear();
      }
      m.open = false;
      m.talking = talking;
      return;
    }
    const bool ended = m.talking && !talking;
    if (!m.open || core.forgets() != m.forgets || ended || now - m.refreshed >= kRefreshMs) {
      m.forgets = core.forgets();
      refresh(s, m, now);
    }
    m.open = true;
    m.talking = talking;
    // From the core, each wake: the open journey, whether a forget may go, who is perceived and who is speaking.
    ov::MemoryFacts& f = m.facts;
    const std::uint16_t current = store.journey();
    const bool can = core.can_forget(current);
    const auto present = core.present();
    const std::optional<std::string> partner = core.conversation().partner();
    bool changed = !f.loaded || f.current != current || f.can_forget != can;
    f.loaded = true;
    f.current = current;
    f.can_forget = can;
    for (ov::JourneyFacts& j : f.journeys) {
      j.current = j.journey == current;
      for (ov::MemberFacts& mm : j.members) {
        const auto p =
            std::find_if(present.begin(), present.end(), [&](const auto& x) { return x.first == mm.persona; });
        const bool here = j.current && p != present.end() && !p->second;
        const bool fell = mm.fell || (j.current && p != present.end() && p->second);
        const bool speaking = j.current && partner && *partner == mm.persona;
        if (here != mm.here || fell != mm.fell || speaking != mm.speaking) changed = true;
        mm.here = here;
        mm.fell = fell;
        mm.speaking = speaking;
      }
    }
    if (changed) ++f.revision;
  });
}

bool memory_result(Shell& s, const en::core::Core& core, en::proto::DbResult& r) noexcept {
  if (r.seq < db::kPageSeq) return false;
  en::proto::ffi_guard(en::proto::Component::DllMain, [&] {
    MemoryLink* m = g_memory;
    if (m == nullptr) return;
    const auto it = m->pending.find(r.seq);
    if (it == m->pending.end()) return;  // asked before the file closed
    const db::PageQuery q = it->second;
    m->pending.erase(it);
    const auto* rows = std::get_if<db::PageRows>(&r.result);
    if (rows == nullptr) {
      if (const auto* f = std::get_if<db::Failed>(&r.result)) {
        log(Line("memory_pages").word("failed", db::to_string(f->fault)), LogLevel::Warn);
      }
      return;
    }
    ++m->read;
    ov::MemoryFacts& f = m->facts;
    using W = db::PageQuery::What;
    switch (q.what) {
      case W::Journeys:
        for (const db::JourneyRow& j : rows->journeys) {
          ask(s, *m, db::PageQuery{W::Conversations, j.journey});
          ask(s, *m, db::PageQuery{W::Memories, j.journey});
          m->journeys[j.journey].journey = j.journey;
        }
        std::erase_if(m->journeys, [&](const auto& kv) {
          return std::none_of(rows->journeys.begin(), rows->journeys.end(),
                              [&](const db::JourneyRow& j) { return j.journey == kv.first; });
        });
        break;
      case W::Conversations: {
        ov::JourneyFacts& j = m->journeys[q.journey];
        j.journey = q.journey;
        j.conversations.clear();
        for (const en::proto::ConversationRow& c : rows->conversations) {
          ov::ConversationFacts out;
          out.id = static_cast<std::uint64_t>(c.id);
          out.persona = c.persona;
          out.name = name_of(core, c.persona);
          out.started_pt = static_cast<std::uint64_t>(c.started_pt);
          if (c.ended_pt) out.ended_pt = static_cast<std::uint64_t>(*c.ended_pt);
          out.end_reason = c.end_reason;
          out.lines = c.committed_lines;
          j.conversations.push_back(std::move(out));
        }
        break;
      }
      case W::Memories: {
        ov::JourneyFacts& j = m->journeys[q.journey];
        j.journey = q.journey;
        j.members.clear();
        for (const db::MemoryCard& c : rows->cards) j.members.push_back(member_of(core, c));
        std::sort(j.members.begin(), j.members.end(),
                  [](const ov::MemberFacts& a, const ov::MemberFacts& b) { return a.name < b.name; });
        break;
      }
      case W::Transcript: {
        std::vector<ov::LineFacts>& lines = f.transcripts[q.conv];
        lines.clear();
        const std::string speaker = rows->conversations.empty() ? std::string("Someone you know")
                                                                 : name_of(core, rows->conversations.front().persona);
        for (const en::proto::StoredEvent& e : rows->lines) {
          if (!e.text) continue;
          const bool tarnished = e.kind == "PlayerSaid";
          lines.push_back(ov::LineFacts{tarnished, tarnished ? std::string() : speaker, *e.text,
                                        static_cast<std::uint64_t>(e.t_play_ms)});
        }
        break;
      }
      case W::Backups:
        f.backups = rows->backups;
        f.backup_walls = rows->backup_walls;
        break;
    }
    // The journeys gathered so far, in order, the members' perceived state kept (memory_tick sets it each wake).
    std::vector<ov::JourneyFacts> js;
    for (auto& [n, j] : m->journeys) {
      for (ov::MemberFacts& mm : j.members) {
        for (const ov::JourneyFacts& old : f.journeys) {
          if (old.journey != n) continue;
          for (const ov::MemberFacts& o : old.members) {
            if (o.persona == mm.persona) {
              mm.here = o.here;
              mm.fell = o.fell;
              mm.speaking = o.speaking;
            }
          }
        }
      }
      js.push_back(j);
    }
    f.journeys = std::move(js);
    ++f.revision;
  });
  return true;
}

void memory_requests(Shell& s, en::overlay::Space& space) noexcept {
  en::proto::ffi_guard(en::proto::Component::DllMain, [&] {
    for (const ov::MemoryRequest& r : space.take_memory_requests()) {
      switch (r.kind) {
        case ov::MemoryRequest::Kind::ForgetNpc:
        case ov::MemoryRequest::Kind::ForgetJourney:
          log(Line("memory_forget").word("what", r.kind == ov::MemoryRequest::Kind::ForgetNpc ? "npc" : "journey")
                  .num("journey", r.journey));
          s.forgets_asked.push_back(en::core::CoreInput::Forget{r.journey, r.persona});
          break;
        case ov::MemoryRequest::Kind::Transcript:
          if (g_memory != nullptr && g_memory->open) {
            ask(s, *g_memory,
                db::PageQuery{db::PageQuery::What::Transcript, r.journey, static_cast<en::proto::RowId>(r.conv)});
          }
          break;
      }
    }
  });
}

const en::overlay::MemoryFacts* memory_facts() noexcept { return g_memory != nullptr ? &g_memory->facts : nullptr; }

}  // namespace eldennet
