// ErBackend's readers of the flags, the text tables and the params (backend.hpp; ARCH §6.9, DESIGN §4.5): the flag
// tree's descent and its 125-byte blocks, the text tables' binary search and their strings, the param tables' binary
// search and their rows; and their self-tests (ARCH §6.7). Pure reads, no game function called, nothing allocated.
#include <algorithm>
#include <string_view>

#include "backend_slots.hpp"
#include "en/gal/backend.hpp"

namespace en::gal {

using proto::GameThread;
using proto::Group;
using roles::Cls;
using roles::Fld;
using roles::Item;
using namespace slots;

namespace {

constexpr std::uint64_t kPage = 4096;

// UTF-16 to sanitised UTF-8 (R13's spirit): printable characters only, surrogate pairs joined, a lone surrogate dropped.
void to_utf8(std::span<const char16_t> units, proto::FixedString<256>& out) {
  std::array<char, 256> buf{};
  std::size_t n = 0;
  const auto put = [&](std::uint32_t cp) {
    std::array<char, 4> b{};
    std::size_t len = 0;
    if (cp < 0x80) {
      b[len++] = static_cast<char>(cp);
    } else if (cp < 0x800) {
      b[len++] = static_cast<char>(0xC0 | (cp >> 6));
      b[len++] = static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
      b[len++] = static_cast<char>(0xE0 | (cp >> 12));
      b[len++] = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
      b[len++] = static_cast<char>(0x80 | (cp & 0x3F));
    } else {
      b[len++] = static_cast<char>(0xF0 | (cp >> 18));
      b[len++] = static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
      b[len++] = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
      b[len++] = static_cast<char>(0x80 | (cp & 0x3F));
    }
    if (n + len > buf.size()) return false;
    for (std::size_t i = 0; i < len; ++i) buf[n++] = b[i];
    return true;
  };
  for (std::size_t i = 0; i < units.size(); ++i) {
    std::uint32_t cp = units[i];
    if (cp >= 0xD800 && cp <= 0xDBFF && i + 1 < units.size() && units[i + 1] >= 0xDC00 && units[i + 1] <= 0xDFFF) {
      cp = 0x10000 + ((cp - 0xD800) << 10) + (units[i + 1] - 0xDC00);
      ++i;
    } else if (cp >= 0xD800 && cp <= 0xDFFF) {
      continue;
    }
    if (cp < 0x20 || (cp >= 0x7F && cp < 0xA0) || cp == 0xFFFE || cp == 0xFFFF) continue;
    if (!put(cp)) break;
  }
  (void)out.assign_prefix(std::string_view(buf.data(), n));
}

bool printable_name(const proto::FixedString<256>& s) {
  // 1 to 64 characters (ARCH §6.7's text test), counted as UTF-8 lead bytes.
  std::size_t chars = 0;
  for (const char c : s.view()) {
    if ((static_cast<unsigned char>(c) & 0xC0) != 0x80) ++chars;
  }
  return chars >= 1 && chars <= 64;
}

}  // namespace

// ---- Flags ----

std::optional<std::uint64_t> ErBackend::flag_block(const GameThread& gt, const ObjView& flags, std::uint32_t group) {
  const auto head = flags.ptr(b_.field(Fld::FlagMap));
  const auto holder = flags.ptr(b_.field(Fld::HolderBase));
  if (!head || *head == 0 || !holder) return std::nullopt;
  const auto h = object(gt, Group::Flags, Cls::FlagNode, *head, kNode);
  if (!h) return std::nullopt;
  std::uint64_t node = h->ptr(b_.field(Fld::NodeParent)).value_or(0);  // the sentinel head's parent is the root
  for (std::uint32_t depth = 0; depth < kFlagTreeDepth; ++depth) {
    if (node == 0 || node == *head) return std::nullopt;  // no such group: its flags read false (ARCH §6.9)
    const auto n = object(gt, Group::Flags, Cls::FlagNode, node, kNode);
    if (!n) return std::nullopt;
    const auto key = n->integer(b_.field(Fld::NodeKey));
    if (!key) return std::nullopt;
    if (*key == group) {
      const auto mode = n->integer(b_.field(Fld::NodeMode));
      const auto value = n->u64(b_.field(Fld::NodeValue));
      // Mode 1: the value is the block's index in the holder, not a byte offset (the ninth read-only round: of the
      // game's 11,928 records only 94 hold a multiple of a block's size, and the flags decode only by index).
      if (mode == std::optional<std::int64_t>(1) && value && *value <= (kAddressEnd - *holder) / kFlagBlockBytes) {
        return *holder + *value * kFlagBlockBytes;
      }
      if (mode == std::optional<std::int64_t>(2) && value) return *value;
      fault(Group::Flags);  // a descriptor of no mode the design names
      return std::nullopt;
    }
    node = group < *key ? n->ptr(b_.field(Fld::NodeLeft)).value_or(0) : n->ptr(b_.field(Fld::NodeRight)).value_or(0);
  }
  fault(Group::Flags);  // deeper than a balanced tree of these sizes can be
  return std::nullopt;
}

void ErBackend::read_flags(const GameThread& gt, proto::WorldSnapshot& out) {
  const auto flags = item_object(gt, Group::Flags, Item::EventFlags);
  if (!flags) return;
  if (flags->integer(b_.field(Fld::Divisor)) != std::optional<std::int64_t>(kFlagDivisor)) {
    fault(Group::Flags);
    return;
  }
  proto::FlagFrame frame;
  frame.watch_version = watch_version_;
  std::uint32_t current = 0xFFFF'FFFF;
  bool have_block = false;
  const std::span<std::byte> block = scratch_->block();
  for (std::size_t k = 0; k < watch_count_ && watch_; ++k) {
    const std::uint16_t idx = watch_order_[k];
    if (idx >= watch_->size()) continue;
    const std::uint32_t id = (*watch_)[idx];
    const std::uint32_t group = id / kFlagDivisor;
    if (group != current) {
      current = group;
      have_block = false;
      if (const auto at = flag_block(gt, *flags, group)) {
        have_block = static_cast<bool>(reader_.copy_into(gt, Group::Flags, *at, block));
        if (!have_block) fault(Group::Flags);
      }
      if (reader_.spent() || !book_.live(Group::Flags)) return;
    }
    if (!have_block) continue;  // valid stays 0: the id reads false (DESIGN §4.5)
    const std::uint32_t at = (id % kFlagDivisor) / 8;
    const unsigned bit = 7 - id % 8;
    frame.valid.set(idx);
    frame.bits.set(idx, ((static_cast<unsigned>(block[at]) >> bit) & 1u) != 0);
  }
  out.flags = frame;
}

// ---- Text ----

bool ErBackend::text_lookup(const GameThread& gt, std::uint32_t table, std::uint32_t id, proto::TextEntry& out) {
  out.text.reset();
  const auto msgs = item_object(gt, Group::Text, Item::Messages);
  if (!msgs) return false;
  const auto tables = msgs->ptr(b_.field(Fld::MsgTables));
  const auto count = msgs->integer(b_.field(Fld::MsgCount));
  if (!tables || !count || *count < 0 || *count > 4096) {
    fault(Group::Text);
    return false;
  }
  if (table >= *count || *tables == 0) return true;  // answered: no such table
  const auto fmg_addr = reader_.copy<std::uint64_t>(gt, Group::Text, *tables + 8ull * table);
  if (!fmg_addr) {
    fault(Group::Text);
    return false;
  }
  if (*fmg_addr == 0) return true;
  const auto fmg = object(gt, Group::Text, Cls::Fmg, *fmg_addr, kFmg);
  if (!fmg) return false;
  const auto groups = fmg->integer(b_.field(Fld::FmgGroupCount));
  const auto groups_off = fmg->integer(b_.field(Fld::FmgGroupsOff));
  const auto offsets_off = fmg->integer(b_.field(Fld::FmgOffsetsOff));
  const std::uint32_t stride = b_.stride_of(Cls::FmgGroup);
  if (!groups || !groups_off || !offsets_off || *groups < 0 || *groups > (1 << 20) || *groups_off < 0 ||
      *offsets_off < 0 || *groups_off > 0x7FFF'FFFF || *offsets_off > 0x7FFF'FFFF || stride == 0) {
    fault(Group::Text);
    return false;
  }
  // Binary search of the groups for first_id <= id <= last_id (ARCH §6.9).
  std::int64_t lo = 0, hi = *groups;
  std::optional<std::int64_t> index;
  while (lo < hi) {
    const std::int64_t mid = lo + (hi - lo) / 2;
    const auto g = object(gt, Group::Text, Cls::FmgGroup,
                          *fmg_addr + static_cast<std::uint64_t>(*groups_off) + static_cast<std::uint64_t>(mid) * stride,
                          kFmgGroup);
    if (!g) return false;
    const auto first = g->integer(b_.field(Fld::FgFirst));
    const auto last = g->integer(b_.field(Fld::FgLast));
    const auto oi = g->integer(b_.field(Fld::FgOffsetIndex));
    if (!first || !last || !oi || *last < *first || *oi < 0) {
      fault(Group::Text);
      return false;
    }
    if (static_cast<std::int64_t>(id) < *first) {
      hi = mid;
    } else if (static_cast<std::int64_t>(id) > *last) {
      lo = mid + 1;
    } else {
      index = *oi + (static_cast<std::int64_t>(id) - *first);
      break;
    }
  }
  if (!index || *index > 0x0FFF'FFFF) return true;  // answered: no entry
  const auto off = reader_.copy<std::int64_t>(
      gt, Group::Text, *fmg_addr + static_cast<std::uint64_t>(*offsets_off) + 8ull * static_cast<std::uint64_t>(*index));
  if (!off) {
    fault(Group::Text);
    return false;
  }
  if (*off <= 0 || *off > 0x7FFF'FFFF) return true;  // 0: no entry
  // The string, copied a page at a time at most (a string near a page's end must not make a copy run past it), up to
  // its NUL, at most 512 bytes.
  const std::span<std::byte> buf = scratch_->text();
  std::uint64_t at = *fmg_addr + static_cast<std::uint64_t>(*off);
  std::size_t have = 0;
  std::optional<std::size_t> units;
  while (have < buf.size() && !units) {
    const std::uint64_t page_left = kPage - (at + have) % kPage;
    const std::size_t take = static_cast<std::size_t>(std::min<std::uint64_t>({64, page_left, buf.size() - have}));
    if (!reader_.copy_into(gt, Group::Text, at + have, buf.subspan(have, take))) {
      fault(Group::Text);
      return false;
    }
    for (std::size_t u = have / 2; 2 * u + 1 < have + take; ++u) {
      if (buf[2 * u] == std::byte{0} && buf[2 * u + 1] == std::byte{0}) {
        units = u;
        break;
      }
    }
    have += take;
  }
  if (!units || *units > 256) {
    fault(Group::Text);  // ARCH §6.7: a string of at most 256 UTF-16 units
    return false;
  }
  std::array<char16_t, 256> text{};
  for (std::size_t u = 0; u < *units; ++u) {
    text[u] = static_cast<char16_t>(static_cast<unsigned>(buf[2 * u]) | (static_cast<unsigned>(buf[2 * u + 1]) << 8));
  }
  proto::FixedString<256> s;
  to_utf8(std::span<const char16_t>(text.data(), *units), s);
  out.text = s;
  return true;
}

PhaseA ErBackend::text_phase_a(const GameThread& gt) {
  const auto msgs = item_object(gt, Group::Text, Item::Messages);
  if (!msgs) return tick_[roles::index(Item::Messages)] == kNull ? PhaseA::Running : PhaseA::Failed;
  const auto count = msgs->integer(b_.field(Fld::MsgCount));
  return count && *count >= kMinTextTables && *count <= 4096 ? PhaseA::Passed : PhaseA::Failed;
}

bool ErBackend::text_phase_b(const GameThread& gt) {
  const auto& fmg = d_.layout().fmg();
  const SelfTestIds& ids = d_.layout().selftest();
  const auto check = [&](const std::vector<std::uint32_t>& want, const std::vector<std::uint32_t>& tables) {
    for (const std::uint32_t id : want) {
      bool named = false;
      for (const std::uint32_t t : tables) {
        proto::TextEntry e;
        if (!text_lookup(gt, t, id, e)) return false;
        if (e.text) {
          named = printable_name(*e.text);
          break;
        }
      }
      if (!named) return false;
    }
    return true;
  };
  if (!fmg) return ids.npc_name_ids.empty() && ids.place_name_ids.empty();
  return check(ids.npc_name_ids, fmg->npc_names) && check(ids.place_name_ids, fmg->place_names);
}

// ---- Params ----

bool ErBackend::param_lookup(const GameThread& gt, proto::ParamKind kind, std::uint32_t id, proto::ParamRowInfo& out,
                             bool* found) {
  if (found != nullptr) *found = false;
  const ParamLayout* pl = d_.layout().find_param(proto::to_string(kind));
  if (pl == nullptr) return true;  // the layout reads no such param: answered with nothing
  const auto store = item_object(gt, Group::Params, Item::ParamStore);
  if (!store) return false;
  const auto tables = store->ptr(b_.field(Fld::PsTables));
  const auto count = store->integer(b_.field(Fld::PsCount));
  if (!tables || !count || *count < 0 || *count > 4096) {
    fault(Group::Params);
    return false;
  }
  if (pl->slot >= *count || *tables == 0) return true;
  const auto table_addr = reader_.copy<std::uint64_t>(gt, Group::Params, *tables + 8ull * pl->slot);
  if (!table_addr) {
    fault(Group::Params);
    return false;
  }
  if (*table_addr == 0) return true;
  const auto table = object(gt, Group::Params, Cls::ParamTable, *table_addr, kParamTable);
  if (!table) return false;
  const auto rows = table->ptr(b_.field(Fld::PtRows));
  const auto n = table->integer(b_.field(Fld::PtRowCount));
  const std::uint32_t stride = b_.stride_of(Cls::ParamRow);
  if (!rows || !n || *n < 0 || *n > 0x10000 || stride == 0) {
    fault(Group::Params);
    return false;
  }
  // A row size that disagrees with the layout's (the paramdef's) fails the group (ARCH §6.9).
  if (const auto size = table->integer(b_.field(Fld::PtRowSize)); size && *size != pl->row_size) {
    fault(Group::Params);
    return false;
  }
  std::int64_t lo = 0, hi = *n;
  while (lo < hi) {
    const std::int64_t mid = lo + (hi - lo) / 2;
    const auto row = object(gt, Group::Params, Cls::ParamRow, *rows + static_cast<std::uint64_t>(mid) * stride, kParamRow);
    if (!row) return false;
    const auto rid = row->integer(b_.field(Fld::PrId));
    if (!rid) return false;
    if (static_cast<std::int64_t>(id) < *rid) {
      hi = mid;
    } else if (static_cast<std::int64_t>(id) > *rid) {
      lo = mid + 1;
    } else {
      const auto data = row->ptr(b_.field(Fld::PrData));
      if (!data || *data == 0) return true;
      const std::span<std::byte> bytes = scratch_->slot(kRowData, pl->row_size);
      if (bytes.size() != pl->row_size || !reader_.copy_into(gt, Group::Params, *data, bytes)) {
        fault(Group::Params);
        return false;
      }
      const ObjView view(bytes);
      out.param = kind;
      out.id = id;
      out.fields.clear();
      for (std::size_t k = 0; k < pl->fields.size() && k < 8; ++k) {
        (void)out.fields.try_push_back(
            proto::ParamField{static_cast<std::uint8_t>(k), view.integer(&pl->fields[k]).value_or(0)});
      }
      if (found != nullptr) *found = true;
      return true;
    }
  }
  return true;
}

PhaseA ErBackend::params_phase_a(const GameThread& gt) {
  const auto store = item_object(gt, Group::Params, Item::ParamStore);
  if (!store) return tick_[roles::index(Item::ParamStore)] == kNull ? PhaseA::Running : PhaseA::Failed;
  // The known rows are found (ids only, never text: the layout's [selftest]), and each read reads its row's size.
  for (const std::uint32_t id : d_.layout().selftest().npc_param_ids) {
    proto::ParamRowInfo row;
    bool found = false;
    if (!param_lookup(gt, proto::ParamKind::NpcParam, id, row, &found) || !found) return PhaseA::Failed;
  }
  return PhaseA::Passed;
}

// ---- The core's look-ups, within the per-tick caps ----

void ErBackend::read_requests(const GameThread& gt, proto::WorldSnapshot& out) {
  std::size_t done = 0;
  if (book_.live(Group::Text)) {
    const std::size_t take = std::min<std::size_t>({text_count_, opts_.text_per_tick, proto::kMaxNewText});
    for (; done < take; ++done) {
      proto::TextEntry e;
      e.fmg = text_asks_[done].fmg;
      e.id = text_asks_[done].id;
      if (!text_lookup(gt, e.fmg, e.id, e)) break;
      (void)out.new_text.try_push_back(e);
    }
  } else {
    done = text_count_;  // text is off: the core falls back to its persona names (DESIGN §4.4)
  }
  std::move(text_asks_.begin() + static_cast<std::ptrdiff_t>(done), text_asks_.begin() + static_cast<std::ptrdiff_t>(text_count_),
            text_asks_.begin());
  text_count_ -= done;

  done = 0;
  if (book_.live(Group::Params)) {
    const std::size_t take = std::min<std::size_t>({param_count_, opts_.params_per_tick, proto::kMaxNewParams});
    for (; done < take; ++done) {
      proto::ParamRowInfo row;
      bool found = false;
      if (!param_lookup(gt, param_asks_[done].kind, param_asks_[done].id, row, &found)) break;
      if (found) (void)out.new_params.try_push_back(row);
    }
  } else {
    done = param_count_;
  }
  std::move(param_asks_.begin() + static_cast<std::ptrdiff_t>(done),
            param_asks_.begin() + static_cast<std::ptrdiff_t>(param_count_), param_asks_.begin());
  param_count_ -= done;
}

}  // namespace en::gal
