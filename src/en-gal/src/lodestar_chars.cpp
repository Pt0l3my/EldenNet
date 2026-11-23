// The character search (lodestar.hpp's CharFound; the thirteenth read-only round): from the characters manager's slot
// to the words that hold a character's address, the runs of elements that hold characters, the memory that holds the
// main player's address, and what the main player's object and the save data's point at. It names nothing: the
// review reads the offsets, and the layout's fields follow from them.
#include "en/gal/lodestar_trial.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace en::gal::lodestar {

namespace {

enum class Kind : std::uint8_t { None, Plain, Classed, Character, Player };  // None: nothing readable there

struct Target {
  std::uint64_t address = 0;  // 0: a free place of the table
  std::uint32_t vtable = 0;   // its first word less the image's base, where that word lies in the image
  std::uint32_t head = 0;     // which of the kept heads is its own
  std::uint16_t head_bytes = 0;  // 0: none kept
  Kind kind = Kind::None;
};

struct ListCandidate {
  std::uint32_t offset = 0;  // of the first address, in the manager's object
  std::uint64_t begin = 0;
  std::uint32_t bytes = 0;
  bool three = false;
};

struct Holder {
  std::uint32_t member = 0;   // which of the look's listed members
  std::uint64_t address = 0;  // the member's target
};

constexpr std::size_t kTableSize = 32768;  // a power of two; three quarters of it are used at most
constexpr std::size_t kFollowedPerList = 256;
constexpr std::uint32_t kSmallest = 0x40;

std::uint64_t word_at(std::span<const std::byte> bytes, std::size_t at) {
  std::uint64_t v = 0;
  std::memcpy(&v, bytes.data() + at, 8);
  return v;
}

}  // namespace

struct CharSearch::Impl {
  enum class Phase : std::uint8_t {
    Slot, Manager, Targets, ListRead, ListTargets, Player, PlayerTargets, Holders, Save, SaveTargets, End
  };

  Options o;
  std::vector<std::byte> manager;  // kManagerSpan
  std::vector<std::byte> scratch;  // kPiece: a run, the player's object or the save data's
  std::vector<std::byte> heads;    // kMaxTargets heads of kHead bytes
  std::vector<Target> table;
  std::array<ListCandidate, kMaxLists> lists{};
  std::array<Holder, kMaxHolders> holders{};
  std::array<CharLook, kPasses> looks;

  // The look under way (the stepping thread's).
  Phase phase = Phase::Slot;
  bool started = false;
  std::uint64_t start_us = 0;
  std::size_t targets = 0;
  std::size_t heads_used = 0;
  std::uint64_t manager_at = 0;
  std::uint32_t manager_bytes = 0;
  std::uint32_t piece = kPiece;  // the size the next read asks for: halved where memory ends sooner
  std::size_t cursor = 0;        // a word of the buffer under way
  std::size_t list_count = 0;
  std::size_t list = 0;
  std::uint32_t list_loaded = 0;
  std::size_t followed = 0;      // addresses followed for the run under way
  std::uint64_t player_at = 0;
  std::uint64_t object_at = 0;   // the object in `scratch`
  std::uint32_t object_bytes = 0;
  std::size_t holder_count = 0;
  std::size_t holder = 0;
  std::uint32_t reads = 0;
  bool cut = false;
  bool targets_cut = false;

  explicit Impl(Options options)
      : o(std::move(options)), manager(kManagerSpan), scratch(kPiece), heads(kMaxTargets * kHead), table(kTableSize) {
    std::sort(o.character_vtables.begin(), o.character_vtables.end());
    std::sort(o.player_vtables.begin(), o.player_vtables.end());
    for (CharLook& l : looks) {
      l.pointers.reserve(kListedPointers);
      l.lists.reserve(kListedLists);
      l.sets.reserve(kListedSets);
      l.members.reserve(kListedMembers);
      l.save_members.reserve(kListedSaveMembers);
    }
  }

  void begin(CharLook& look, std::uint64_t now_us) {
    started = true;
    start_us = now_us;
    std::fill(table.begin(), table.end(), Target{});
    targets = 0;
    heads_used = 0;
    manager_at = 0;
    manager_bytes = 0;
    piece = kPiece;
    cursor = 0;
    list_count = 0;
    list = 0;
    player_at = 0;
    holder_count = 0;
    holder = 0;
    reads = 0;
    cut = false;
    targets_cut = false;
    phase = Phase::Slot;
    look.through = look.overrun = look.cut = look.targets_cut = false;
    look.took_ms = look.reads = look.manager_bytes = look.pointer_words = look.targets_read = 0;
    look.characters = look.players = look.player_bytes = look.members_total = 0;
    look.save_bytes = look.save_members_total = 0;
    look.pointers.clear();  // the lists keep their room
    look.lists.clear();
    look.sets.clear();
    look.members.clear();
    look.save_members.clear();
  }

  [[nodiscard]] bool in_image(std::uint64_t v) const noexcept { return v >= o.base && v - o.base < o.image_size; }
  [[nodiscard]] bool pointer_like(std::uint64_t v) const noexcept {
    return v % 8 == 0 && v >= kLowestAddress && v <= kAddressEnd && !in_image(v);
  }

  [[nodiscard]] std::size_t place(std::uint64_t address) const noexcept {
    return static_cast<std::size_t>(((address >> 3) * 0x9E3779B97F4A7C15ull) >> 49) & (kTableSize - 1);
  }
  [[nodiscard]] const Target* find(std::uint64_t address) const noexcept {
    for (std::size_t i = place(address);; i = (i + 1) & (kTableSize - 1)) {
      if (table[i].address == address) return &table[i];
      if (table[i].address == 0) return nullptr;
    }
  }
  Target* insert(std::uint64_t address) noexcept {
    for (std::size_t i = place(address);; i = (i + 1) & (kTableSize - 1)) {
      if (table[i].address == 0) {
        table[i].address = address;
        ++targets;
        return &table[i];
      }
    }
  }

  [[nodiscard]] std::span<const std::byte> head_of(const Target& t) const noexcept {
    return std::span<const std::byte>(heads).subspan(std::size_t{t.head} * kHead, t.head_bytes);
  }

  // The slot's value, from the image as loaded; 0 where it holds no address of memory.
  [[nodiscard]] std::uint64_t slot(std::uint32_t rva) const noexcept {
    if (o.image == nullptr || rva == 0) return 0;
    std::array<std::byte, 8> raw{};
    if (!o.image->read(rva, raw)) return 0;
    const std::uint64_t at = word_at(raw, 0);
    return pointer_like(at) ? at : 0;
  }

  // Makes `address` known: 0 where it is already or cannot be (the limits), 1 where it was followed, 2 where the
  // reader's time was up (the same address again at the next call).
  int follow(const proto::GameThread& gt, GuardedReader& reader, std::uint64_t address) {
    if (find(address) != nullptr) return 0;
    if (reads >= kMaxReads) {
      cut = true;
      return 0;
    }
    if (targets >= kTableSize / 4 * 3) {
      targets_cut = true;
      return 0;
    }
    std::array<std::byte, 8> small{};
    const bool keep = heads_used < kMaxTargets;
    const std::span<std::byte> dst =
        keep ? std::span<std::byte>(heads).subspan(heads_used * kHead, kHead) : std::span<std::byte>(small);
    std::uint32_t got = 0;
    for (const std::uint32_t size : {kHead, kSmallest, std::uint32_t{8}}) {
      if (size > dst.size()) continue;
      ++reads;
      if (reader.copy_into(gt, proto::Group::Session, address, dst.first(size), 8).has_value()) {
        got = size;
        break;
      }
      if (reader.spent()) {
        --reads;
        return 2;
      }
    }
    Target* t = insert(address);
    if (got == 0) return 1;  // nothing readable there
    const std::uint64_t first = word_at(dst, 0);
    if (in_image(first)) {
      t->vtable = static_cast<std::uint32_t>(first - o.base);
      t->kind = std::binary_search(o.player_vtables.begin(), o.player_vtables.end(), t->vtable)
                    ? Kind::Player
                    : std::binary_search(o.character_vtables.begin(), o.character_vtables.end(), t->vtable)
                          ? Kind::Character
                          : Kind::Classed;
    } else {
      t->kind = Kind::Plain;
    }
    if (keep) {
      t->head = static_cast<std::uint32_t>(heads_used++);
      t->head_bytes = static_cast<std::uint16_t>(got);
    }
    return 1;
  }

  // One attempt at `piece` bytes from `address` into `dst`: 1 read; 2 the reader's time was up; 0 not readable (the
  // piece is halved for the next attempt, and 0 once it would fall below the smallest).
  int attempt(const proto::GameThread& gt, GuardedReader& reader, std::uint64_t address, std::span<std::byte> dst) {
    ++reads;
    if (reader.copy_into(gt, proto::Group::Session, address, dst.first(piece), 8).has_value()) return 1;
    if (reader.spent()) {
      --reads;
      return 2;
    }
    piece = piece > kSmallest ? piece / 2 : 0;
    return 0;
  }

  // Follows the addresses in `bytes` from the cursor on, one a call: 1 where one was followed (or the reader's time
  // was up), 0 when all are through.
  int follow_words(const proto::GameThread& gt, GuardedReader& reader, std::span<const std::byte> bytes,
                   std::uint64_t self, std::size_t most) {
    const std::size_t words = bytes.size() / 8;
    while (cursor < words && followed < most) {
      const std::uint64_t v = word_at(bytes, cursor * 8);
      if (!pointer_like(v) || (v >= self && v - self < bytes.size())) {
        ++cursor;
        continue;
      }
      const int f = follow(gt, reader, v);
      if (f == 2) return 1;
      ++cursor;
      if (f == 1) {
        ++followed;
        return 1;
      }
    }
    return 0;
  }

  [[nodiscard]] bool is_character(std::uint64_t v) const noexcept {
    if (!pointer_like(v)) return false;
    const Target* t = find(v);
    return t != nullptr && (t->kind == Kind::Character || t->kind == Kind::Player);
  }

  // After the manager's addresses are known: the characters among them, the runs to try, and the player's sets.
  void after_targets(CharLook& look) {
    const std::span<const std::byte> bytes = std::span<const std::byte>(manager).first(manager_bytes);
    const std::size_t words = bytes.size() / 8;
    look.manager_bytes = manager_bytes;
    std::array<std::uint64_t, 8> players{};
    for (int round = 0; round < 2; ++round) {  // the players' words first, then the other characters'
      for (std::size_t i = 0; i < words; ++i) {
        const std::uint64_t v = word_at(bytes, i * 8);
        if (!pointer_like(v)) continue;
        const Target* t = find(v);
        if (t == nullptr || t->kind == Kind::None) continue;
        if (round == 0) ++look.pointer_words;
        const bool player = t->kind == Kind::Player;
        if (!player && t->kind != Kind::Character) continue;
        if (player != (round == 0)) continue;
        ++look.characters;
        if (player) {
          if (player_at == 0) player_at = v;
          if (std::find(players.begin(), players.begin() + look.players, v) == players.begin() + look.players &&
              look.players < players.size()) {
            players[look.players++] = v;
          }
        }
        if (look.pointers.size() < kListedPointers) {
          look.pointers.push_back(CharPointer{static_cast<std::uint32_t>(i * 8), player});
        }
      }
    }
    // Two neighbouring addresses, the second past the first by no more than a run may be long: those followed by a
    // third first (begin, end, capacity), then the others.
    for (int round = 0; round < 2; ++round) {
      for (std::size_t i = 0; i + 1 < words && list_count < kMaxLists; ++i) {
        const std::uint64_t b = word_at(bytes, i * 8), e = word_at(bytes, (i + 1) * 8);
        if (!pointer_like(b) || e <= b || e - b > kListSpan || (e - b) % 8 != 0) continue;
        const Target* t = find(b);
        if (t == nullptr || t->kind == Kind::None) continue;
        const std::uint64_t c = i + 2 < words ? word_at(bytes, (i + 2) * 8) : 0;
        const bool three = c >= e && c - b <= 4ull * kListSpan;
        if (three != (round == 0)) continue;
        lists[list_count++] = ListCandidate{static_cast<std::uint32_t>(i * 8), b, static_cast<std::uint32_t>(e - b), three};
      }
    }
    if (player_at == 0) return;
    for (std::size_t i = 0; i < words && look.sets.size() < kListedSets; ++i) {
      const std::uint64_t v = word_at(bytes, i * 8);
      if (!pointer_like(v)) continue;
      const Target* t = find(v);
      if (t == nullptr || (t->kind != Kind::Plain && t->kind != Kind::Classed) || t->head_bytes < 8) continue;
      const std::span<const std::byte> head = head_of(*t);
      for (std::size_t k = 0; k + 8 <= head.size(); k += 8) {
        if (word_at(head, k) != player_at) continue;
        look.sets.push_back(CharSet{static_cast<std::uint32_t>(i * 8), static_cast<std::uint32_t>(k), t->vtable});
        break;
      }
    }
  }

  // The run in `scratch`: the element size and the place in an element at which the most elements hold a
  // character's address; listed where at least half of them do.
  void judge_list(CharLook& look) {
    const ListCandidate& c = lists[list];
    const std::span<const std::byte> bytes = std::span<const std::byte>(scratch).first(list_loaded);
    std::uint32_t best_stride = 0, best_at = 0, best_count = 0, best_elements = 0;
    for (const std::uint32_t stride : {8u, 16u, 24u, 32u, 40u, 48u, 56u, 64u, 72u, 80u, 96u, 128u}) {
      if (c.bytes % stride != 0) continue;
      const std::uint32_t elements = list_loaded / stride;
      if (elements == 0) continue;
      for (std::uint32_t k = 0; k + 8 <= stride; k += 8) {
        std::uint32_t count = 0;
        for (std::uint32_t i = 0; i < elements; ++i) count += is_character(word_at(bytes, i * stride + k)) ? 1 : 0;
        // The larger share of elements wins, then the larger count: a run of 16-byte elements read as 8-byte ones
        // holds a character in every second, and read whole it is one element that holds one.
        const std::uint64_t share = std::uint64_t{count} * best_elements, best_share = std::uint64_t{best_count} * elements;
        if (count != 0 && (best_count == 0 || share > best_share || (share == best_share && count > best_count))) {
          best_stride = stride;
          best_at = k;
          best_count = count;
          best_elements = elements;
        }
      }
    }
    if (best_count == 0 || best_count * 2 < best_elements || look.lists.size() >= kListedLists) return;
    CharList out;
    out.offset = c.offset;
    out.bytes = c.bytes;
    out.three = c.three;
    out.stride = best_stride;
    out.chr_at = best_at;
    out.elements = best_elements;
    out.characters = best_count;
    for (std::uint32_t i = 0; i < best_elements; ++i) {
      out.has_player = out.has_player || (player_at != 0 && word_at(bytes, i * best_stride + best_at) == player_at);
    }
    // A fraction beside the address that never falls along the run and rises somewhere: a list kept by distance.
    for (std::uint32_t j = 0; j + 4 <= best_stride && !out.has_distance && best_elements >= 2; j += 4) {
      if (j >= best_at && j < best_at + 8) continue;
      float first = 0.0f, last = 0.0f;
      bool holds = true;
      for (std::uint32_t i = 0; i < best_elements && holds; ++i) {
        float f = 0.0f;
        std::memcpy(&f, bytes.data() + i * best_stride + j, 4);
        holds = std::isfinite(f) && f >= 0.0f && f <= 1.0e6f && (i == 0 || f >= last);
        if (i == 0) first = f;
        last = f;
      }
      if (holds && last > first) {
        out.has_distance = true;
        out.distance_at = j;
      }
    }
    look.lists.push_back(out);
  }

  // The addresses in the object in `scratch`, as members; with `hold`, those whose first 16 words are mostly
  // addresses are kept for a closer look.
  void members_of(std::vector<CharMember>& list, std::uint32_t& total, std::size_t most, bool hold) {
    const std::span<const std::byte> bytes = std::span<const std::byte>(scratch).first(object_bytes);
    for (std::size_t i = 0; i + 8 <= bytes.size(); i += 8) {
      const std::uint64_t v = word_at(bytes, i);
      if (!pointer_like(v) || (v >= object_at && v - object_at < bytes.size())) continue;
      const Target* t = find(v);
      if (t == nullptr || t->kind == Kind::None) continue;
      ++total;
      if (list.size() >= most) continue;
      CharMember m;
      m.offset = static_cast<std::uint32_t>(i);
      m.vtable = t->vtable;
      const std::span<const std::byte> head = head_of(*t);
      std::uint32_t first16 = 0;
      for (std::size_t k = 0; k + 8 <= head.size() && k < 32 * 8; k += 8) {
        if (!pointer_like(word_at(head, k))) continue;
        ++m.pointers;
        first16 += k < 16 * 8 ? 1 : 0;
      }
      if (hold && first16 >= 8 && holder_count < kMaxHolders) {
        holders[holder_count++] = Holder{static_cast<std::uint32_t>(list.size()), v};
      }
      list.push_back(m);
    }
  }
};

CharSearch::CharSearch(Options options) : impl_(std::make_unique<Impl>(std::move(options))) {
  save_slot_.store(impl_->o.save_slot, std::memory_order_release);
}
CharSearch::~CharSearch() = default;

void CharSearch::ask(std::uint32_t mark) noexcept {
  const std::uint32_t n = asked_.load(std::memory_order_relaxed);
  if (n >= kPasses) return;
  if (n > 0 && done_.load(std::memory_order_acquire) < n) overrun_[n - 1].store(true, std::memory_order_relaxed);
  mark_[n].store(mark, std::memory_order_relaxed);
  asked_.store(n + 1, std::memory_order_release);
}

void CharSearch::set_save_slot(std::uint32_t rva) noexcept { save_slot_.store(rva, std::memory_order_release); }

bool CharSearch::step(const proto::GameThread& gt, GuardedReader& reader, std::uint64_t now_us) {
  Impl& m = *impl_;
  using Phase = Impl::Phase;
  const std::uint32_t pass = done_.load(std::memory_order_relaxed);
  if (pass >= asked_.load(std::memory_order_acquire) || pass >= kPasses) return false;
  CharLook& look = m.looks[pass];
  if (!m.started) {
    m.o.save_slot = save_slot_.load(std::memory_order_acquire);
    m.begin(look, now_us);
  }
  for (;;) {
    switch (m.phase) {
      case Phase::Slot: {
        m.manager_at = m.slot(m.o.manager_slot);
        if (m.manager_at != 0) {
          m.phase = Phase::Manager;
        } else {
          m.piece = kSaveSpan;
          m.phase = Phase::Save;
        }
        break;
      }
      case Phase::Manager: {
        if (m.manager_bytes >= kManagerSpan || m.piece == 0 || m.reads >= kMaxReads) {
          m.cursor = 0;
          m.followed = 0;
          m.phase = Phase::Targets;
          break;
        }
        // A piece that is not readable is halved, and the reading goes on in the smaller pieces to where memory ends.
        m.piece = std::min(m.piece, kManagerSpan - m.manager_bytes);
        if (m.attempt(gt, reader, m.manager_at + m.manager_bytes,
                      std::span<std::byte>(m.manager).subspan(m.manager_bytes)) == 1) {
          m.manager_bytes += m.piece;
        }
        return false;
      }
      case Phase::Targets: {
        if (m.follow_words(gt, reader, std::span<const std::byte>(m.manager).first(m.manager_bytes), m.manager_at,
                           kMaxTargets) != 0) {
          return false;
        }
        m.after_targets(look);
        m.list = 0;
        m.phase = Phase::ListRead;
        break;
      }
      case Phase::ListRead: {
        if (m.list >= m.list_count || m.reads >= kMaxReads) {
          m.cut = m.cut || m.list < m.list_count;
          m.piece = kPlayerSpan;
          m.phase = Phase::Player;
          break;
        }
        const ListCandidate& c = m.lists[m.list];
        const std::uint32_t want = std::min(c.bytes, kPiece);
        ++m.reads;
        if (!reader.copy_into(gt, proto::Group::Session, c.begin, std::span<std::byte>(m.scratch).first(want), 8)
                 .has_value()) {
          if (reader.spent()) {
            --m.reads;
          } else {
            ++m.list;
          }
          return false;
        }
        m.list_loaded = want;
        m.cursor = 0;
        m.followed = 0;
        m.phase = Phase::ListTargets;
        return false;
      }
      case Phase::ListTargets: {
        if (m.follow_words(gt, reader, std::span<const std::byte>(m.scratch).first(m.list_loaded), 0,
                           kFollowedPerList) != 0) {
          return false;
        }
        m.judge_list(look);
        ++m.list;
        m.phase = Phase::ListRead;
        break;
      }
      case Phase::Player: {
        if (m.player_at == 0 || m.piece == 0 || m.reads >= kMaxReads) {
          m.piece = kSaveSpan;
          m.phase = Phase::Save;
          break;
        }
        if (m.attempt(gt, reader, m.player_at, m.scratch) == 1) {
          m.object_at = m.player_at;
          m.object_bytes = m.piece;
          look.player_bytes = m.piece;
          m.cursor = 0;
          m.followed = 0;
          m.phase = Phase::PlayerTargets;
        }
        return false;
      }
      case Phase::PlayerTargets: {
        if (m.follow_words(gt, reader, std::span<const std::byte>(m.scratch).first(m.object_bytes), m.object_at,
                           kMaxTargets) != 0) {
          return false;
        }
        m.members_of(look.members, look.members_total, kListedMembers, true);
        m.holder = 0;
        m.cursor = 0;
        m.followed = 0;
        m.phase = Phase::Holders;
        break;
      }
      case Phase::Holders: {
        if (m.holder >= m.holder_count) {
          m.piece = kSaveSpan;
          m.phase = Phase::Save;
          break;
        }
        const Holder& h = m.holders[m.holder];
        const Target* t = m.find(h.address);
        const std::span<const std::byte> head =
            t != nullptr ? m.head_of(*t).first(std::min<std::size_t>(t->head_bytes, 16 * 8)) : std::span<const std::byte>();
        if (m.follow_words(gt, reader, head, 0, 16) != 0) return false;
        std::uint32_t classed = 0;
        for (std::size_t k = 0; k + 8 <= head.size(); k += 8) {
          const std::uint64_t v = word_at(head, k);
          if (!m.pointer_like(v)) continue;
          const Target* inner = m.find(v);
          classed += inner != nullptr && inner->kind >= Kind::Classed ? 1 : 0;
        }
        look.members[h.member].classed = classed;
        ++m.holder;
        m.cursor = 0;
        m.followed = 0;
        break;
      }
      case Phase::Save: {
        const std::uint64_t at = m.slot(m.o.save_slot);
        if (at == 0 || m.piece == 0 || m.reads >= kMaxReads) {
          m.phase = Phase::End;
          break;
        }
        if (m.attempt(gt, reader, at, m.scratch) == 1) {
          m.object_at = at;
          m.object_bytes = m.piece;
          look.save_bytes = m.piece;
          m.cursor = 0;
          m.followed = 0;
          m.phase = Phase::SaveTargets;
        }
        return false;
      }
      case Phase::SaveTargets: {
        if (m.follow_words(gt, reader, std::span<const std::byte>(m.scratch).first(m.object_bytes), m.object_at,
                           kMaxTargets) != 0) {
          return false;
        }
        m.members_of(look.save_members, look.save_members_total, kListedSaveMembers, false);
        m.phase = Phase::End;
        break;
      }
      case Phase::End: {
        const std::uint64_t took = now_us >= m.start_us ? (now_us - m.start_us) / 1000 : 0;
        look.took_ms = static_cast<std::uint32_t>(std::min<std::uint64_t>(took, 0xFFFF'FFFFu));
        look.reads = m.reads;
        look.cut = m.cut;
        look.targets_read = static_cast<std::uint32_t>(m.targets);
        look.targets_cut = m.targets_cut;
        look.through = true;
        m.started = false;
        done_.store(pass + 1, std::memory_order_release);
        return true;
      }
    }
  }
}

CharFound CharSearch::found() const {
  CharFound out;
  out.rva = impl_->o.manager_slot;
  out.asked = asked_.load(std::memory_order_acquire);
  out.passes = done_.load(std::memory_order_acquire);
  for (std::uint32_t p = 0; p < out.asked && p < kPasses; ++p) {
    const bool overrun = overrun_[p].load(std::memory_order_relaxed);
    out.overrun = out.overrun || overrun;
    if (p >= out.passes) continue;  // a look under way is the stepping thread's
    CharLook look = impl_->looks[p];
    look.overrun = overrun;
    look.mark = mark_[p].load(std::memory_order_relaxed);
    out.looks.push_back(std::move(look));
  }
  return out;
}

}  // namespace en::gal::lodestar
