// ErBackend (backend.hpp; ARCH §6.6 to §6.8, §7.1): the snapshot's reads, group by group, with R5's re-resolution and
// vtable compares, the per-tick plausibility checks, phase A and phase B, R11's lifecycle-only reads while loading, and
// the commands. Text, params and flags' tree walk are in readers.cpp.
#include "en/gal/backend.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string_view>

#include "backend_slots.hpp"

namespace en::gal {

using proto::Expected;
using proto::GameThread;
using proto::Group;
using proto::Unexpected;
using roles::Cls;
using roles::Enum;
using roles::Fld;
using roles::Item;

const char* to_string(ReadMode m) noexcept {
  switch (m) {
    case ReadMode::Full: return "full";
    case ReadMode::Dormant: return "dormant";
    case ReadMode::Loading: return "loading";
  }
  return "full";
}

using namespace slots;


namespace {

std::uint32_t list_bytes(const roles::Bound& b) {
  return kWalkEntries * std::min<std::uint32_t>(b.stride_of(Cls::NearEntry), kMaxCopyBytes / kWalkEntries);
}

template <class E>
E enum_of(std::optional<std::string_view> name, std::initializer_list<std::pair<std::string_view, E>> table, E fallback) {
  if (!name) return fallback;
  for (const auto& [n, v] : table) {
    if (n == *name) return v;
  }
  return fallback;
}

float distance(const std::array<float, 3>& a, const std::array<float, 3>& b) {
  const float dx = a[0] - b[0], dy = a[1] - b[1], dz = a[2] - b[2];
  return std::sqrt(dx * dx + dy * dy + dz * dz);
}

bool finite3(const std::array<float, 3>& p) { return std::isfinite(p[0]) && std::isfinite(p[1]) && std::isfinite(p[2]); }

}  // namespace

ErBackend::ErBackend(SysLayer& sys, const Discovery& discovery, const BackendOptions& options)
    : sys_(sys),
      d_(discovery),
      b_(discovery.bound()),
      opts_(options),
      reader_(sys, options.budget),
      scratch_(std::make_unique<Scratch>(discovery.layout().max_copy_size(),  // far fields' bytes included
                                         list_bytes(discovery.bound()))),
      watch_order_(proto::kMaxWatchFlags, 0) {
  for (std::size_t g = 0; g < proto::kGroupCount; ++g) {
    const GroupWhy why = discovery.report().groups[g];
    if (why != GroupWhy::None) book_.trip(static_cast<Group>(g), why);
  }
}

ErBackend::~ErBackend() = default;

std::uint64_t ErBackend::now_ms() const noexcept { return sys_.now_us() / 1000; }

void ErBackend::trip(Group g, GroupWhy why) noexcept {
  if (!book_.trip(g, why)) return;
  push_lifecycle(proto::Lifecycle{proto::Lifecycle::GroupTripped{g}});
  if (proto::structural(g)) inert_ = true;
}

void ErBackend::fault(Group g) noexcept {
  if (reader_.spent()) return;  // a spent budget abandons the snapshot: a soft fault, not the group's
  if (opts_.steady && building_) {
    // Steady reading: noted, and counted when the snapshot is done, if the group faulted in the one before too.
    faulted_now_ = static_cast<std::uint16_t>(faulted_now_ | (1u << static_cast<unsigned>(g)));
    return;
  }
  count_fault(g);
}

void ErBackend::count_fault(Group g) noexcept {
  if (book_.hard_fault(g, now_ms())) {
    push_lifecycle(proto::Lifecycle{proto::Lifecycle::GroupTripped{g}});
    if (proto::structural(g)) inert_ = true;
  }
}

void ErBackend::soft(Group g) noexcept { ++soft_[static_cast<std::size_t>(g)]; }

void ErBackend::remember(std::uint64_t address, std::uint64_t value) noexcept {
  if (!opts_.steady || !building_ || word_count_ >= words_.size()) return;
  words_[word_count_++] = Word{address, value};
}

// Reads every remembered word again: each static's instance, each object's table of methods, the near list's ends.
// One that cannot be read now, or reads otherwise, means the structure changed while the snapshot was being read.
bool ErBackend::structure_held(const GameThread& gt) {
  for (std::size_t i = 0; i < word_count_ && i < words_.size(); ++i) {
    const auto again = reader_.copy<std::uint64_t>(gt, Group::Session, words_[i].address);
    if (!again || *again != words_[i].value) return false;
  }
  return true;
}

void ErBackend::push_lifecycle(const proto::Lifecycle& l) noexcept {
  if (life_count_ >= life_.size()) return;  // the core rebuilds what it lost from the snapshots (C2)
  life_[(life_head_ + life_count_) % life_.size()] = l;
  ++life_count_;
}

bool ErBackend::pop_lifecycle(proto::Lifecycle& out) noexcept {
  if (life_count_ == 0) return false;
  out = life_[life_head_];
  life_head_ = (life_head_ + 1) % life_.size();
  --life_count_;
  return true;
}

std::shared_ptr<const std::vector<std::uint32_t>> ErBackend::take_retired() noexcept {
  for (auto& r : retired_) {
    if (r) return std::move(r);
  }
  return nullptr;
}

// ---- Objects ----

std::optional<ObjView> ErBackend::object(const GameThread& gt, Group g, Cls c, std::uint64_t address, std::size_t slot,
                                         bool* mismatch, bool hard) {
  const ClassLayout* layout = b_.cls(c);
  const std::uint32_t size = b_.size_of(c);  // the copy, then the far fields' own bytes
  if (layout == nullptr || size == 0 || address == 0) return std::nullopt;
  const std::span<std::byte> dst = scratch_->slot(slot, size);
  const std::uint32_t copy = size - layout->far_bytes;
  bool read = dst.size() >= size && (copy == 0 || reader_.copy_into(gt, g, address, dst.first(copy), 8).has_value());
  // A far field (WHY§506) lies past the copy, in a large object: it is read where it is, by a copy of its own.
  for (const FieldLayout& f : layout->fields) {
    if (!read || !f.is_far) continue;
    read = reader_.copy_into(gt, g, address + f.off, dst.subspan(f.far_at, f.size), 1).has_value();
  }
  if (!read) {
    if (hard) fault(g);
    return std::nullopt;
  }
  const ObjView view(dst);
  const VtableSet& set = d_.vtables(c);
  if (!set.empty()) {
    std::uint64_t vt = 0;
    std::memcpy(&vt, dst.data(), sizeof vt);
    if (!set.contains(vt)) {
      if (mismatch != nullptr) *mismatch = true;
      return std::nullopt;
    }
    remember(address, vt);  // steady reading: the object is still of this class when the snapshot is done
  }
  return view;
}

void ErBackend::read_statics(const GameThread&) {
  // R5: nothing of the last tick is kept; each item's static is re-read on first use this tick (item_object).
  inst_.fill(0);
}

std::optional<ObjView> ErBackend::item_object(const GameThread& gt, Group g, Item item, bool hard) {
  const std::size_t i = roles::index(item);
  const auto cls = roles::class_of(item);
  const auto slot_addr = d_.address(item);
  if (!cls || !slot_addr) return std::nullopt;
  // inst_[i] holds the instance (bit 0 clear) once its static is read this tick; tick_[i] says how its object went.
  std::uint8_t& state = tick_[i];
  if (state == kObjectFailed || state == kNull) return std::nullopt;
  const std::uint32_t size = b_.size_of(*cls);
  if (state == kObjectOk) return ObjView(scratch_->slot(kItems + i, size));
  if (state == kUnread) {
    const auto inst = reader_.copy<std::uint64_t>(gt, g, *slot_addr);
    if (!inst) {
      if (hard) fault(g);
      state = kObjectFailed;
      return std::nullopt;
    }
    inst_[i] = *inst;
    state = kStaticRead;
    remember(*slot_addr, *inst);  // steady reading: the static still holds this instance when the snapshot is done
  }
  if (inst_[i] == 0) {
    state = kNull;  // null: not a fault (the title, a load), simply nothing to read
    return std::nullopt;
  }
  bool mismatch = false;
  const auto view = object(gt, g, *cls, inst_[i], kItems + i, &mismatch, hard);
  if (!view) {
    state = kObjectFailed;
    if (mismatch) {
      // R5 #3: the object moved or was torn down. Skipped this tick; a hard fault only on the third tick in a row with
      // the main player present.
      ++vtable_skips_;
      if (vt_miss_[i] < 255) ++vt_miss_[i];
      if (vt_miss_[i] >= 3 && main_player_ != 0 && hard) {
        vt_miss_[i] = 0;
        fault(g);
      }
    }
    return std::nullopt;
  }
  vt_miss_[i] = 0;
  state = kObjectOk;
  return view;
}

// ---- The snapshot ----

proto::Capabilities ErBackend::probe(const GameThread&) {
  proto::Capabilities caps;
  for (std::size_t g = 0; g < proto::kGroupCount; ++g) {
    if (book_.state(static_cast<Group>(g)) == proto::GroupState::Ok) caps.groups = caps.groups | proto::bit_of(static_cast<Group>(g));
  }
  return caps;
}

Expected<void, proto::ReadError> ErBackend::snapshot(const GameThread& gt, proto::WorldSnapshot& out) {
  if (inert_) return Unexpected{proto::ReadError::Inert};
  reader_.begin_tick();
  book_.begin_tick();
  tick_.fill(kUnread);
  read_statics(gt);
  building_ = true;
  word_count_ = 0;
  faulted_now_ = 0;
  // Steady reading's ends: the reading dropped, with nothing counted against any group.
  const auto unsteady = [&](std::uint64_t& counter) -> Expected<void, proto::ReadError> {
    ++counter;
    building_ = false;
    faulted_before_ = 0;  // the next reading is judged on its own
    book_.fill(out.health);
    return Unexpected{proto::ReadError::Unsteady};
  };
  out.session = proto::Session{};
  out.life = proto::Life{};
  out.ui = proto::Ui{};
  out.player.reset();
  out.place.reset();
  out.clock.reset();
  out.boss = proto::Boss{};
  out.chars.clear();
  out.flags.reset();
  out.new_text.clear();
  out.new_params.clear();

  read_lifecycle(gt, out);
  if (opts_.steady && !reader_.spent()) {
    // A change of the lifecycle is published at its second sighting running: the first reading that shows it is
    // dropped, so that a pointer or a screen caught between two states starts no playthrough and ends none.
    const LifeSeen seen{true, out.life.main_player, out.life.screen};
    const auto same = [](const LifeSeen& a, const LifeSeen& b) {
      return a.known == b.known && a.main_player == b.main_player && a.screen == b.screen;
    };
    if (!life_published_.known) {
      life_published_ = seen;
    } else if (!same(seen, life_published_)) {
      if (same(seen, life_waiting_)) {
        life_published_ = seen;
        life_waiting_ = LifeSeen{};
      } else {
        life_waiting_ = seen;
        return unsteady(steady_.held);
      }
    } else {
      life_waiting_ = LifeSeen{};
    }
  }
  if (out.life.screen == proto::Screen::Loading) {
    mode_ = ReadMode::Loading;  // R11: the lifecycle subset only
  } else if (!out.life.main_player || out.life.screen == proto::Screen::Title) {
    mode_ = ReadMode::Dormant;
  } else {
    mode_ = ReadMode::Full;
  }
  if (mode_ != ReadMode::Loading && book_.live(Group::Session)) read_session(gt, out);
  if (mode_ == ReadMode::Full) {
    const proto::Session& s = out.session;
    const bool solo = s.lobby == proto::LobbyState::None && s.role == proto::MultiplayRole::Host && s.is_my_world;
    if (book_.state(Group::Session) == proto::GroupState::Ok && !solo) mode_ = ReadMode::Dormant;
  }
  if (!out.life.main_player || mode_ == ReadMode::Loading) {
    in_world_ = false;
    seen_count_ = 0;
  }
  if (mode_ == ReadMode::Full) {
    if (!in_world_) {
      in_world_ = true;
      phase_b_pending_ = true;
      last_play_time_.reset();
    }
    const bool player_ok = book_.live(Group::Player) && read_player(gt, out);
    (void)player_ok;
    if (book_.live(Group::Camera)) read_camera(gt, out);
    if (book_.live(Group::World)) read_world(gt, out);
    if (book_.live(Group::Ui)) read_ui(gt, out);
    if (book_.live(Group::Place)) read_place(gt, out);
    if (flag_frame_due_ && book_.live(Group::Flags)) read_flags(gt, out);
    read_requests(gt, out);
  }
  // Steady reading: the structure this snapshot rests on must still be what it was, before anything is concluded
  // from it (a playthrough's start, phase B) or counted against a group.
  if (opts_.steady && !reader_.spent() && !structure_held(gt)) return unsteady(steady_.torn);
  if (mode_ == ReadMode::Full && phase_b_pending_ && !reader_.spent()) {
    phase_b_pending_ = false;
    phase_b(gt, out);
    playthrough_ready(gt, out);
  }
  building_ = false;
  if (opts_.steady && !reader_.spent()) {
    for (std::size_t g = 0; g < proto::kGroupCount; ++g) {
      const auto bit = static_cast<std::uint16_t>(1u << g);
      if ((faulted_now_ & bit) == 0) continue;
      if ((faulted_before_ & bit) != 0) {
        count_fault(static_cast<Group>(g));  // twice running: the group's fault
      } else {
        ++steady_.unconfirmed;
      }
    }
    faulted_before_ = faulted_now_;
  }
  book_.fill(out.health);
  if (reader_.spent()) return Unexpected{proto::ReadError::BudgetSpent};
  if (inert_) return Unexpected{proto::ReadError::Faulted};
  return {};
}

void ErBackend::read_lifecycle(const GameThread& gt, proto::WorldSnapshot& out) {
  // The lifecycle subset (ARCH §6.1): the main player (world), the screen state and the load counter (ui).
  main_player_ = 0;
  if (book_.live(Group::World)) {
    if (const auto chars = item_object(gt, Group::World, Item::Characters)) {
      main_player_ = chars->ptr(b_.field(Fld::MainPlayer)).value_or(0);
    }
  }
  out.life.main_player = main_player_ != 0;
  if (book_.live(Group::Ui)) {
    if (const auto menus = item_object(gt, Group::Ui, Item::Menus)) {
      const auto raw = menus->integer(b_.field(Fld::Screen));
      const auto* t = b_.table(Enum::Screen);
      const auto name = raw && t != nullptr ? t->name_of(*raw) : std::nullopt;
      if (!name) {
        fault(Group::Ui);
      } else {
        out.life.screen = enum_of<proto::Screen>(name, {{"InGame", proto::Screen::InGame},
                                                        {"Loading", proto::Screen::Loading},
                                                        {"Title", proto::Screen::Title}},
                                                 proto::Screen::Unknown);
      }
    }
    if (d_.address(Item::Loading)) {
      if (const auto loading = item_object(gt, Group::Ui, Item::Loading)) {
        if (const auto n = loading->integer(b_.field(Fld::LoadCounter)); n && *n >= 0 && *n <= 0xFFFF'FFFF) {
          out.life.load_counter = static_cast<std::uint32_t>(*n);
        }
      }
    }
  }
}

void ErBackend::read_session(const GameThread& gt, proto::WorldSnapshot& out) {
  const auto session = item_object(gt, Group::Session, Item::Session);
  if (!session) return;
  {
    const auto raw = session->integer(b_.field(Fld::Lobby));
    const auto* t = b_.table(Enum::Lobby);
    const auto name = raw && t != nullptr ? t->name_of(*raw) : std::nullopt;
    if (!name) {
      fault(Group::Session);
      return;
    }
    out.session.lobby = enum_of<proto::LobbyState>(name, {{"None", proto::LobbyState::None},
                                                          {"Hosting", proto::LobbyState::Hosting},
                                                          {"Joined", proto::LobbyState::Joined},
                                                          {"Other", proto::LobbyState::Other}},
                                                   proto::LobbyState::Unknown);
  }
  // The role and the own-world flag, from the player's sheet through the save data (Unknown at the title, where the
  // sheet may be empty).
  if (const auto save = item_object(gt, Group::Session, Item::SaveData)) {
    const auto pd_addr = save->ptr(b_.field(Fld::SavePlayerData));
    if (pd_addr && *pd_addr != 0) {
      if (const auto pd = object(gt, Group::Session, Cls::PlayerData, *pd_addr, kPlayerData)) {
        const auto raw = pd->integer(b_.field(Fld::Role));
        const auto* t = b_.table(Enum::Role);
        const auto name = raw && t != nullptr ? t->name_of(*raw) : std::nullopt;
        if (name) {
          out.session.role = enum_of<proto::MultiplayRole>(name, {{"Host", proto::MultiplayRole::Host},
                                                                  {"Cooperator", proto::MultiplayRole::Cooperator},
                                                                  {"Invader", proto::MultiplayRole::Invader},
                                                                  {"Other", proto::MultiplayRole::Other}},
                                                           proto::MultiplayRole::Unknown);
        } else if (out.life.main_player) {
          fault(Group::Session);  // in the world the role must be one the layout names
        }
        out.session.is_my_world = pd->integer(b_.field(Fld::MyWorld)).value_or(0) != 0;
      }
    }
  }
  // The player set: its non-null entries.
  if (out.life.main_player) {
    if (const auto chars = item_object(gt, Group::World, Item::Characters)) {
      const auto set = chars->ptr(b_.field(Fld::PlayerSet));
      const auto len = chars->integer(b_.field(Fld::PlayerSetLen));
      const std::uint32_t stride = b_.stride_of(Cls::PlayerSlot);
      if (set && len && *set != 0 && *len > 0 && stride >= 8) {
        const auto n = static_cast<std::uint32_t>(std::min<std::int64_t>(*len, 16));
        const std::span<std::byte> buf = scratch_->players().first(std::min<std::size_t>(std::size_t{n} * stride, 4096));
        if (buf.size() == std::size_t{n} * stride && reader_.copy_into(gt, Group::Session, *set, buf, 8)) {
          std::uint8_t count = 0;
          for (std::uint32_t k = 0; k < n; ++k) {
            const ObjView slot(buf.subspan(std::size_t{k} * stride, stride));
            if (slot.ptr(b_.field(Fld::SlotChr)).value_or(0) != 0) ++count;
          }
          out.session.player_set_len = count;
        } else {
          fault(Group::Session);
        }
      } else if (len && *len < 0) {
        fault(Group::Session);
      }
    }
  }
  out.session.anticheat = false;  // C14's atomics are the DLL's (part 6): it fills them in
  out.session.ersc_loaded = false;
}

bool ErBackend::read_parts(const GameThread& gt, const ObjView& chr_obj, proto::Meter& hp, std::array<float, 3>& pos,
                           std::int32_t* recv_dmg, std::uint8_t* throw_state, proto::Meter* fp, proto::Meter* stamina) {
  const auto parts_addr = chr_obj.ptr(b_.field(Fld::ChrParts));
  if (!parts_addr || *parts_addr == 0) return false;
  const auto parts = object(gt, Group::World, Cls::Parts, *parts_addr, kParts);
  if (!parts) return false;
  const auto vit_addr = parts->ptr(b_.field(Fld::PartsVitals));
  const auto mot_addr = parts->ptr(b_.field(Fld::PartsMotion));
  if (!vit_addr || !mot_addr || *vit_addr == 0 || *mot_addr == 0) return false;
  const auto vit = object(gt, Group::World, Cls::Vitals, *vit_addr, kVitals);
  if (!vit) return false;
  const auto meter = [&](Fld cur, Fld max) -> std::optional<proto::Meter> {
    const auto c = vit->integer(b_.field(cur));
    const auto m = vit->integer(b_.field(max));
    if (!c || !m || *c < 0 || *m < 0 || *c > 0xFFFF'FFFF || *m > 0xFFFF'FFFF) return std::nullopt;
    return proto::Meter{static_cast<std::uint32_t>(*c), static_cast<std::uint32_t>(*m)};
  };
  const auto h = meter(Fld::Hp, Fld::MaxHp);
  if (!h) return false;
  hp = *h;
  if (fp != nullptr) *fp = meter(Fld::Fp, Fld::MaxFp).value_or(proto::Meter{});
  if (stamina != nullptr) *stamina = meter(Fld::Stamina, Fld::MaxStamina).value_or(proto::Meter{});
  const auto mot = object(gt, Group::World, Cls::Motion, *mot_addr, kMotion);
  if (!mot) return false;
  for (std::size_t i = 0; i < 3; ++i) pos[i] = mot->f32(b_.field(Fld::Pos), i).value_or(NAN);
  if (!finite3(pos)) return false;
  if (book_.live(Group::Combat)) {
    if (recv_dmg != nullptr) {
      const auto a = parts->ptr(b_.field(Fld::PartsReaction));
      if (a && *a != 0) {
        if (const auto r = object(gt, Group::Combat, Cls::Reaction, *a, kReaction)) {
          *recv_dmg = static_cast<std::int32_t>(r->integer(b_.field(Fld::RecvDmg)).value_or(0));
        }
      }
    }
    if (throw_state != nullptr) {
      const auto a = parts->ptr(b_.field(Fld::PartsGrip));
      if (a && *a != 0) {
        if (const auto r = object(gt, Group::Combat, Cls::Grip, *a, kGrip)) {
          const auto t = r->integer(b_.field(Fld::Throw)).value_or(0);
          if (t < 0 || t > 6) {
            fault(Group::Combat);  // ARCH §6.7: the throw state lies in 0 to 6
          } else {
            *throw_state = static_cast<std::uint8_t>(t);
          }
        }
      }
    }
  }
  return true;
}

bool ErBackend::read_player(const GameThread& gt, proto::WorldSnapshot& out) {
  bool mismatch = false;
  const auto p = object(gt, Group::Player, Cls::PlayerCharacter, main_player_, kPlayer, &mismatch);
  if (!p) {
    if (mismatch) {
      ++vtable_skips_;
      soft(Group::Player);
    }
    return false;
  }
  proto::Player pl;
  pl.handle = proto::ChrHandle{p->u64(b_.field(Fld::ChrHandle)).value_or(0)};
  pl.dead = p->bit(b_.field(Fld::ChrDead)).value_or(false);
  pl.block = proto::BlockId{static_cast<std::uint32_t>(p->integer(b_.field(Fld::ChrBlock)).value_or(0))};
  if (book_.live(Group::Combat)) {
    if (const auto h = p->u64(b_.field(Fld::PlayerLockedOn)); h && *h != 0) pl.locked_on = proto::ChrHandle{*h};
    if (const auto h = p->u64(b_.field(Fld::ChrLastHit)); h && *h != 0) pl.last_hit_by = proto::ChrHandle{*h};
  }
  if (const auto* ride = b_.table(Enum::Ride)) {
    if (const auto raw = p->integer(b_.field(Fld::PlayerRide))) pl.riding = ride->name_of(*raw) == std::optional<std::string_view>("Riding");
  }
  std::int32_t recv = 0;
  if (!read_parts(gt, *p, pl.hp, pl.pos, &recv, &pl.throw_state, &pl.fp, &pl.stamina)) {
    fault(Group::Player);
    return false;
  }
  const auto save = item_object(gt, Group::Player, Item::SaveData);
  const auto game = item_object(gt, Group::Player, Item::GameState);
  if (!save || !game) {
    fault(Group::Player);
    return false;
  }
  const auto play = save->integer(b_.field(Fld::PlayTime));
  const auto deaths = save->integer(b_.field(Fld::Deaths));
  const auto ng = save->integer(b_.field(Fld::NgLevel));
  const auto slot = game->integer(b_.field(Fld::SaveSlot));
  if (!play || !deaths || !ng || !slot || *play < 0 || *deaths < 0 || *ng < 0 || *ng > 255 || *slot < 0 ||
      *slot > 255 || !ObjView::in_range(b_.field(Fld::SaveSlot), *slot) ||
      !ObjView::in_range(b_.field(Fld::NgLevel), *ng)) {
    fault(Group::Player);
    return false;
  }
  // Play time never goes back within a playthrough (ARCH §6.7).
  if (last_play_time_ && static_cast<std::uint64_t>(*play) < *last_play_time_) {
    fault(Group::Player);
    return false;
  }
  last_play_time_ = static_cast<std::uint64_t>(*play);
  pl.play_time_ms = static_cast<std::uint64_t>(*play);
  pl.deaths = static_cast<std::uint32_t>(std::min<std::int64_t>(*deaths, 0xFFFF'FFFF));
  pl.ng_lvl = static_cast<std::uint8_t>(*ng);
  pl.save_slot = static_cast<std::uint8_t>(*slot);
  pl.boss_flag = save->integer(b_.field(Fld::BossFight)).value_or(0) != 0;
  const auto pd_addr = save->ptr(b_.field(Fld::SavePlayerData));
  const auto pd = pd_addr ? object(gt, Group::Player, Cls::PlayerData, *pd_addr, kPlayerData) : std::nullopt;
  if (!pd) {
    fault(Group::Player);
    return false;
  }
  const auto level = pd->integer(b_.field(Fld::Level));
  if (!level || *level < 0 || *level > 0xFFFF || !ObjView::in_range(b_.field(Fld::Level), *level)) {
    fault(Group::Player);
    return false;
  }
  pl.level = static_cast<std::uint16_t>(*level);
  pl.archetype = static_cast<std::uint8_t>(pd->integer(b_.field(Fld::Archetype)).value_or(0));
  pl.runes_held = static_cast<std::uint32_t>(std::clamp<std::int64_t>(pd->integer(b_.field(Fld::Runes)).value_or(0), 0,
                                                                      0xFFFF'FFFF));
  if (const auto* body = b_.table(Enum::Body)) {
    const auto raw = pd->integer(b_.field(Fld::Body));
    pl.body = enum_of<proto::BodyType>(raw ? body->name_of(*raw) : std::nullopt,
                                       {{"A", proto::BodyType::A}, {"B", proto::BodyType::B}}, proto::BodyType::Unknown);
  }
  // The gear: an array of item ids inside the sheet.
  if (const FieldLayout* gear = b_.field(Fld::Gear); gear != nullptr && b_.field(Fld::ItemValue) != nullptr) {
    std::array<std::uint32_t, 6> ids{};
    for (std::uint32_t k = 0; k < std::min<std::uint32_t>(gear->len, 6); ++k) {
      const std::size_t at = std::size_t{gear->off} + std::size_t{k} * gear->stride;
      if (at + gear->stride > pd->bytes().size()) break;
      const ObjView elem(pd->bytes().subspan(at, gear->stride));
      ids[k] = static_cast<std::uint32_t>(std::clamp<std::int64_t>(elem.integer(b_.field(Fld::ItemValue)).value_or(0),
                                                                   0, 0xFFFF'FFFF));
    }
    pl.gear = proto::Gear{ids[0], ids[1], ids[2], ids[3], ids[4], ids[5]};
  }
  pl.statuses = proto::StatusSet::None;  // status stays off until S8 (O40)
  out.player = pl;
  if (book_.state(Group::Player) == proto::GroupState::Untested) book_.set_ok(Group::Player);
  return true;
}

void ErBackend::read_camera(const GameThread& gt, proto::WorldSnapshot&) {
  cam_ok_ = false;
  const auto cam = item_object(gt, Group::Camera, Item::Camera);
  if (!cam) return;
  const auto view_addr = cam->ptr(b_.field(Fld::View));
  if (!view_addr || *view_addr == 0) return;
  const auto view = object(gt, Group::Camera, Cls::View, *view_addr, kView);
  if (!view) return;
  for (std::size_t i = 0; i < 16; ++i) cam_[i] = view->f32(b_.field(Fld::ViewMatrix), i).value_or(NAN);
  cam_fov_ = view->f32(b_.field(Fld::ViewFov)).value_or(NAN);
  cam_aspect_ = view->f32(b_.field(Fld::ViewAspect)).value_or(0.0f);
  // ARCH §6.7: an orthonormal rotation (rows of length 1 ± 0.01, orthogonal within 0.01), and a field of view of 0.3
  // to 2.5 rad.
  bool ok = std::isfinite(cam_fov_) && cam_fov_ >= 0.3f && cam_fov_ <= 2.5f;
  for (std::size_t r = 0; r < 3 && ok; ++r) {
    const float len = std::sqrt(cam_[4 * r] * cam_[4 * r] + cam_[4 * r + 1] * cam_[4 * r + 1] +
                                cam_[4 * r + 2] * cam_[4 * r + 2]);
    ok = std::isfinite(len) && std::fabs(len - 1.0f) <= 0.01f;
    for (std::size_t s = r + 1; s < 3 && ok; ++s) {
      const float dot = cam_[4 * r] * cam_[4 * s] + cam_[4 * r + 1] * cam_[4 * s + 1] + cam_[4 * r + 2] * cam_[4 * s + 2];
      ok = std::fabs(dot) <= 0.01f;
    }
  }
  if (!ok) {
    fault(Group::Camera);
    return;
  }
  cam_ok_ = true;
  if (book_.state(Group::Camera) == proto::GroupState::Untested) book_.set_ok(Group::Camera);
}

void ErBackend::read_world(const GameThread& gt, proto::WorldSnapshot& out) {
  seen_count_ = 0;
  const auto chars = item_object(gt, Group::World, Item::Characters);
  if (!chars) return;
  const FieldLayout* vec = b_.field(Fld::ByDistance);
  const auto begin = chars->u64(vec);
  const auto end = chars->second(vec);
  const std::uint32_t stride = b_.stride_of(Cls::NearEntry);
  if (!begin || !end || stride == 0 || *end < *begin || (*end - *begin) % stride != 0 ||
      (*end - *begin) / stride > kWalkEntries) {
    fault(Group::World);  // ARCH §6.6 #4: an aligned list of at most 48 entries
    return;
  }
  const auto count = static_cast<std::uint32_t>((*end - *begin) / stride);
  // Steady reading: the list's two ends are still these when the snapshot is done.
  if (vec != nullptr && instance(Item::Characters) != 0) {
    remember(instance(Item::Characters) + vec->off, *begin);
    remember(instance(Item::Characters) + vec->off + 8, *end);
  }
  if (count == 0) return;
  const std::span<std::byte> list = scratch_->list().first(std::min<std::size_t>(std::size_t{count} * stride, scratch_->list().size()));
  if (list.size() != std::size_t{count} * stride || !reader_.copy_into(gt, Group::World, *begin, list, 8)) {
    fault(Group::World);
    return;
  }
  const std::uint32_t keep = throttled_ ? opts_.throttled_keep : opts_.keep;
  const float within = throttled_ ? opts_.throttled_within_m : opts_.keep_within_m;
  const bool combat = book_.live(Group::Combat);
  // The camera's forward, for the bearing: the view's third row, flattened (S1 confirms the convention).
  const float fx = cam_ok_ ? cam_[8] : 0.0f, fz = cam_ok_ ? cam_[10] : 0.0f;
  std::uint32_t examined = 0, mismatched = 0;
  float last = -1.0f;
  for (std::uint32_t k = 0; k < count && out.chars.size() < keep; ++k) {
    const ObjView entry(list.subspan(std::size_t{k} * stride, stride));
    const auto chr = entry.ptr(b_.field(Fld::NearChr));
    const auto dist = entry.f32(b_.field(Fld::NearDistance));
    if (!chr || !dist || *chr == 0 || *chr % 8 != 0 || !std::isfinite(*dist)) {
      soft(Group::World);
      continue;
    }
    if (*dist > within) break;  // the list is nearest first: the rest are farther
    ++examined;
    if (*dist + 0.5f < last) soft(Group::World);  // not ascending within 0.5 m: counted, kept
    last = std::max(last, *dist);
    bool mismatch = false;
    const auto c = object(gt, Group::World, Cls::Character, *chr, kChr, &mismatch);
    if (!c) {
      if (!mismatch) return;  // a copy fault: a hard fault for world, counted; the walk stops
      ++mismatched;
      soft(Group::World);  // R5 #4: an entry whose vtable is no character's is dropped from this snapshot
      continue;
    }
    proto::Chr ch;
    ch.handle = proto::ChrHandle{c->u64(b_.field(Fld::ChrHandle)).value_or(0)};
    ch.param = static_cast<std::uint32_t>(c->integer(b_.field(Fld::ChrParam)).value_or(0));
    ch.model = static_cast<std::uint32_t>(c->integer(b_.field(Fld::ChrModel)).value_or(0));
    ch.entity = static_cast<std::uint32_t>(c->integer(b_.field(Fld::ChrEntity)).value_or(0));
    ch.kind = static_cast<std::uint8_t>(c->integer(b_.field(Fld::ChrType)).value_or(0));
    ch.team = static_cast<std::uint8_t>(c->integer(b_.field(Fld::ChrTeam)).value_or(0));
    ch.block = proto::BlockId{static_cast<std::uint32_t>(c->integer(b_.field(Fld::ChrBlock)).value_or(0))};
    ch.on_screen = c->bit(b_.field(Fld::ChrOnScreen)).value_or(false);
    ch.dead = c->bit(b_.field(Fld::ChrDead)).value_or(false);
    if (combat) {
      if (const auto h = c->u64(b_.field(Fld::ChrLastHit)); h && *h != 0) ch.last_hit_by = proto::ChrHandle{*h};
    }
    std::int32_t recv = 0;
    std::uint8_t thr = 0;
    if (!read_parts(gt, *c, ch.hp, ch.pos, &recv, &thr, nullptr, nullptr)) {
      if (reader_.spent() || book_.state(Group::World) == proto::GroupState::Tripped) return;
      soft(Group::World);
      continue;
    }
    // ARCH §6.7: 0 < max_hp <= 100000 and hp <= max_hp, per entry.
    if (ch.hp.max == 0 || ch.hp.max > 100000 || ch.hp.cur > ch.hp.max) {
      soft(Group::World);
      continue;
    }
    ch.recv_dmg_type = recv;
    ch.throw_state = thr;
    ch.dist_m = out.player ? distance(ch.pos, out.player->pos) : *dist;
    if (cam_ok_ && out.player) {
      const float dx = ch.pos[0] - out.player->pos[0], dz = ch.pos[2] - out.player->pos[2];
      const float a = std::atan2(dx * fz - dz * fx, dx * fx + dz * fz);
      ch.bearing_deg = a * 57.29578f;
    } else {
      ch.bearing_deg = NAN;
    }
    if (out.chars.try_push_back(ch) && seen_count_ < seen_.size()) seen_[seen_count_++] = Seen{ch.handle.raw, *chr};
  }
  // More than a quarter of the entries examined mismatching in one tick is a hard fault for world (ARCH §6.5).
  if (examined > 0 && mismatched * 4 > examined) fault(Group::World);
  if (book_.state(Group::World) == proto::GroupState::Untested && !reader_.spent()) book_.set_ok(Group::World);
  if (combat && book_.state(Group::Combat) == proto::GroupState::Untested) book_.set_ok(Group::Combat);
}

void ErBackend::read_ui(const GameThread& gt, proto::WorldSnapshot& out) {
  if (const auto hud = item_object(gt, Group::Ui, Item::Hud)) {
    const auto raw = hud->integer(b_.field(Fld::HudState));
    const auto* t = b_.table(Enum::Hud);
    const auto name = raw && t != nullptr ? t->name_of(*raw) : std::nullopt;
    if (!name) {
      fault(Group::Ui);
      return;
    }
    out.ui.hud = enum_of<proto::HudState>(name, {{"HideAll", proto::HudState::HideAll},
                                                 {"ShowAll", proto::HudState::ShowAll},
                                                 {"PopupMenu", proto::HudState::PopupMenu},
                                                 {"Default", proto::HudState::Default}},
                                          proto::HudState::Unknown);
    const auto banner = hud->integer(b_.field(Fld::Banner)).value_or(0);
    if (banner != 0 && banner != -1) {
      if (banner < 0 || banner > 0xFFFF || !ObjView::in_range(b_.field(Fld::Banner), banner)) {
        fault(Group::Ui);
        return;
      }
      out.ui.banner = static_cast<std::uint16_t>(banner);
    }
    if (const FieldLayout* bars = b_.field(Fld::BossBars)) {
      for (std::uint32_t k = 0; k < std::min<std::uint32_t>(bars->len, 3); ++k) {
        const std::size_t at = std::size_t{bars->off} + std::size_t{k} * bars->stride;
        if (at + bars->stride > hud->bytes().size()) break;
        const ObjView bar(hud->bytes().subspan(at, bars->stride));
        const auto h = bar.u64(b_.field(Fld::BarHandle)).value_or(0);
        if (h == 0) continue;
        (void)out.boss.bars.try_push_back(proto::BossBar{
            proto::ChrHandle{h}, static_cast<std::int32_t>(bar.integer(b_.field(Fld::BarNameId)).value_or(0)),
            static_cast<std::int32_t>(bar.integer(b_.field(Fld::BarDamage)).value_or(0))});
      }
    }
  }
  out.boss.active = !out.boss.bars.empty() || (out.player && out.player->boss_flag);
  if (const auto menus = item_object(gt, Group::Ui, Item::Menus)) {
    const auto popup = menus->ptr(b_.field(Fld::Popup));
    if (popup && *popup != 0 && b_.cls(Cls::Popup) != nullptr) {
      if (const auto p = object(gt, Group::Ui, Cls::Popup, *popup, kPopup)) {
        const auto talk = p->integer(b_.field(Fld::PopupTalk));
        if (talk && *talk >= 0) out.ui.popup_talk_id = static_cast<std::uint32_t>(*talk);
      }
    }
  }
  if (const auto game = item_object(gt, Group::Ui, Item::GameState)) {
    if (const auto cam = game->integer(b_.field(Fld::TalkCam))) out.ui.talk_cam = *cam != -1;
  }
  if (const auto save = item_object(gt, Group::Ui, Item::SaveData)) {
    const auto s = save->ptr(b_.field(Fld::SaveSettings));
    if (s && *s != 0 && b_.cls(Cls::Settings) != nullptr) {
      if (const auto set = object(gt, Group::Ui, Cls::Settings, *s, kSettings)) {
        out.ui.subtitles_on = set->integer(b_.field(Fld::Subtitles)).value_or(0) != 0;
      }
    }
  }
}

void ErBackend::read_place(const GameThread& gt, proto::WorldSnapshot& out) {
  const auto area = item_object(gt, Group::Place, Item::FieldArea);
  const auto clock = item_object(gt, Group::Place, Item::WorldClock);
  if (!area || !clock) return;
  const auto hour = clock->integer(b_.field(Fld::Hour));
  const auto minute = clock->integer(b_.field(Fld::Minute));
  if (!hour || !minute || *hour < 0 || *hour >= 24 || *minute < 0 || *minute >= 60) {
    fault(Group::Place);  // ARCH §6.7: the hour below 24
    return;
  }
  out.clock = proto::Clock{static_cast<std::uint8_t>(*hour), static_cast<std::uint8_t>(*minute)};
  proto::Place place;
  if (const auto id = area->integer(b_.field(Fld::PlaceId)); id && *id > 0 && *id <= 0xFFFF'FFFF) {
    place.place_id = static_cast<std::uint32_t>(*id);
  }
  out.place = place;  // the region, legacy and overworld are the core's, from the block (regions.toml)
  if (book_.state(Group::Place) == proto::GroupState::Untested) book_.set_ok(Group::Place);
}

// ---- Phase A and phase B (ARCH §6.7) ----

GroupCheck ErBackend::first_tick(const GameThread& gt, bool check_group) {
  sys_.note_own_stack();
  reader_.bind_thread();
  reader_.begin_tick();
  tick_.fill(kUnread);
  GroupCheck out;
  if (!check_group) {  // a reader on a thread of our own is in no group of the game's
    out.ok = true;
    return out;
  }
  const auto& task = d_.layout().task();
  const auto hub = item_object(gt, Group::Session, Item::TaskHub, false);  // counted nowhere: boot has no breaker
  if (!task || task->partial || !hub) return out;
  const auto table = hub->ptr(b_.field(Fld::HubGroups));
  const auto count = hub->integer(b_.field(Fld::HubGroupCount));
  const std::uint32_t stride = b_.stride_of(Cls::TaskGroup);
  if (!table || !count || *table == 0 || *count <= 0 || *count > 4096 || stride == 0) return out;
  out.readable = true;
  const FieldLayout* name_f = b_.field(Fld::GroupName);
  const auto name_matches = [&](std::uint32_t index) -> std::optional<bool> {
    const auto entry =
        object(gt, Group::Session, Cls::TaskGroup, *table + std::uint64_t{index} * stride, kTaskGroup, nullptr, false);
    if (!entry) return std::nullopt;
    std::array<char16_t, 128> units{};
    const auto len = entry->utf16(name_f, units);
    if (!len || *len != task->group_name.size()) return false;
    for (std::size_t c = 0; c < *len; ++c) {
      if (units[c] != static_cast<char16_t>(static_cast<unsigned char>(task->group_name[c]))) return false;
    }
    return true;
  };
  if (task->group_index < *count) out.ok = name_matches(task->group_index).value_or(false);
  if (!out.ok) {
    // Where the name is, for the probe's record; the task goes inert either way (ARCH §3.2's FirstTick).
    const auto n = static_cast<std::uint32_t>(std::min<std::int64_t>(*count, 256));
    for (std::uint32_t i = 0; i < n && !reader_.spent(); ++i) {
      if (name_matches(i).value_or(false)) {
        out.found_at = i;
        break;
      }
    }
  }
  return out;
}

PhaseA ErBackend::phase_a(const GameThread& gt) {
  if (phase_a_done_) return inert_ ? PhaseA::Failed : PhaseA::Passed;
  ++phase_a_ticks_;
  reader_.begin_tick();
  book_.begin_tick();
  tick_.fill(kUnread);
  const bool last_chance = phase_a_ticks_ >= kPhaseATicks;
  bool waiting = false;
  // One group's outcome: passed, failed, or not yet (an instance still null: retried on the next tick).
  const auto settle = [&](Group g, std::optional<bool> passed) {
    if (book_.state(g) != proto::GroupState::Untested) return;
    if (!passed) {
      if (last_chance) {
        trip(g, GroupWhy::PhaseA);
      } else {
        waiting = true;
      }
    } else if (*passed) {
      book_.set_ok(g);
    } else {
      trip(g, GroupWhy::PhaseA);
    }
  };
  const auto inst_ok = [&](Group g, Item i) -> std::optional<bool> {
    if (item_object(gt, g, i)) return true;
    return tick_[roles::index(i)] == kNull ? std::nullopt : std::optional<bool>(false);
  };
  // session: the instance's vtable, the lobby in its enum.
  if (book_.state(Group::Session) == proto::GroupState::Untested) {
    std::optional<bool> ok = inst_ok(Group::Session, Item::Session);
    if (ok && *ok) {
      const auto v = item_object(gt, Group::Session, Item::Session);
      const auto raw = v ? v->integer(b_.field(Fld::Lobby)) : std::nullopt;
      ok = raw && b_.table(Enum::Lobby) != nullptr && b_.table(Enum::Lobby)->name_of(*raw).has_value();
    }
    settle(Group::Session, ok);
  }
  // world: the characters' static resolves (the main player may be null at the title).
  if (book_.state(Group::World) == proto::GroupState::Untested) settle(Group::World, inst_ok(Group::World, Item::Characters));
  if (book_.state(Group::Combat) == proto::GroupState::Untested && book_.state(Group::World) == proto::GroupState::Ok) {
    settle(Group::Combat, true);
  }
  // player: the save data and the game state, non-null, with their vtables.
  if (book_.state(Group::Player) == proto::GroupState::Untested) {
    const auto a = inst_ok(Group::Player, Item::SaveData);
    const auto b = inst_ok(Group::Player, Item::GameState);
    settle(Group::Player, !a || !b ? std::nullopt : std::optional<bool>(*a && *b));
  }
  // flags: the divisor is 1000.
  if (book_.state(Group::Flags) == proto::GroupState::Untested) {
    std::optional<bool> ok = inst_ok(Group::Flags, Item::EventFlags);
    if (ok && *ok) {
      const auto v = item_object(gt, Group::Flags, Item::EventFlags);
      ok = v && v->integer(b_.field(Fld::Divisor)) == std::optional<std::int64_t>(kFlagDivisor);
    }
    settle(Group::Flags, ok);
  }
  // ui: the HUD's state and the screen state within their enums.
  if (book_.state(Group::Ui) == proto::GroupState::Untested) {
    const auto a = inst_ok(Group::Ui, Item::Hud);
    const auto b = inst_ok(Group::Ui, Item::Menus);
    std::optional<bool> ok = !a || !b ? std::nullopt : std::optional<bool>(*a && *b);
    if (ok && *ok) {
      const auto hud = item_object(gt, Group::Ui, Item::Hud);
      const auto menus = item_object(gt, Group::Ui, Item::Menus);
      const auto h = hud ? hud->integer(b_.field(Fld::HudState)) : std::nullopt;
      const auto s = menus ? menus->integer(b_.field(Fld::Screen)) : std::nullopt;
      ok = h && s && b_.table(Enum::Hud) != nullptr && b_.table(Enum::Screen) != nullptr &&
           b_.table(Enum::Hud)->name_of(*h) && b_.table(Enum::Screen)->name_of(*s);
    }
    settle(Group::Ui, ok);
  }
  // place and camera: nothing before the world (phase B); their instances must exist.
  if (book_.state(Group::Place) == proto::GroupState::Untested) {
    const auto a = inst_ok(Group::Place, Item::FieldArea);
    const auto b = inst_ok(Group::Place, Item::WorldClock);
    settle(Group::Place, !a || !b ? std::nullopt : std::optional<bool>(*a && *b));
  }
  if (book_.state(Group::Camera) == proto::GroupState::Untested) settle(Group::Camera, inst_ok(Group::Camera, Item::Camera));
  // text: at least 20 tables; params: the known rows found, their sizes the layout's.
  if (book_.state(Group::Text) == proto::GroupState::Untested) {
    const PhaseA t = text_phase_a(gt);
    settle(Group::Text, t == PhaseA::Running ? std::nullopt : std::optional<bool>(t == PhaseA::Passed));
  }
  if (book_.state(Group::Params) == proto::GroupState::Untested) {
    const PhaseA t = params_phase_a(gt);
    settle(Group::Params, t == PhaseA::Running ? std::nullopt : std::optional<bool>(t == PhaseA::Passed));
  }
  if (inert_) {
    phase_a_done_ = true;
    return PhaseA::Failed;
  }
  if (waiting && !last_chance) return PhaseA::Running;
  phase_a_done_ = true;
  return PhaseA::Passed;
}

ErBackend::FlagsAlone ErBackend::flags_alone(const GameThread& gt, proto::WorldSnapshot& out) {
  FlagsAlone done;
  out.flags.reset();
  if (inert_ || !book_.live(Group::Flags)) return done;
  reader_.begin_tick();
  book_.begin_tick();
  tick_.fill(kUnread);
  read_statics(gt);
  read_flags(gt, out);
  done.read = out.flags.has_value();
  // The known flag, as phase B reads it: through the same tree, one byte of its block.
  if (const auto flags = item_object(gt, Group::Flags, Item::EventFlags)) {
    if (const auto block = flag_block(gt, *flags, kFlagKnownSet / kFlagDivisor)) {
      std::array<std::byte, 1> byte{};
      const std::uint32_t at = (kFlagKnownSet % kFlagDivisor) / 8;
      if (reader_.copy_into(gt, Group::Flags, *block + at, byte)) {
        done.known = ((static_cast<unsigned>(byte[0]) >> (7 - kFlagKnownSet % 8)) & 1u) != 0;
      }
    } else if (!reader_.spent()) {
      done.known = false;  // no such group: its flags read false (ARCH §6.9)
    }
  }
  return done;
}

void ErBackend::phase_b(const GameThread& gt, const proto::WorldSnapshot& out) {
  // session: solo play reads Host and the own world, with one player in the set.
  if (book_.live(Group::Session)) {
    const proto::Session& s = out.session;
    const bool ok = s.lobby != proto::LobbyState::None ||
                    (s.role == proto::MultiplayRole::Host && s.is_my_world && s.player_set_len == 1);
    book_.set_phase_b(Group::Session, ok);
    if (!ok) trip(Group::Session, GroupWhy::PhaseB);
  }
  // player: the main player's class, a name of 1 to 16 printable units, a slot in its range.
  if (book_.live(Group::Player)) {
    bool ok = out.player.has_value();
    if (ok) {
      const auto save = item_object(gt, Group::Player, Item::SaveData);
      const auto pd_addr = save ? save->ptr(b_.field(Fld::SavePlayerData)) : std::nullopt;
      const auto pd = pd_addr ? object(gt, Group::Player, Cls::PlayerData, *pd_addr, kPlayerData) : std::nullopt;
      std::array<char16_t, 64> units{};
      const auto len = pd ? pd->utf16(b_.field(Fld::Name), units) : std::nullopt;
      ok = len && *len >= 1 && *len <= 16 &&
           std::all_of(units.begin(), units.begin() + static_cast<std::ptrdiff_t>(*len),
                       [](char16_t c) { return c >= 0x20 && c != 0x7F && (c < 0xD800 || c > 0xDFFF); });
    }
    book_.set_phase_b(Group::Player, ok);
    if (!ok) trip(Group::Player, GroupWhy::PhaseB);
  }
  // world: what was walked passed its checks entry by entry; the group stands.
  if (book_.live(Group::World)) book_.set_phase_b(Group::World, true);
  if (book_.live(Group::Combat)) {
    bool ok = true;
    for (const proto::Chr& c : out.chars) ok = ok && c.throw_state <= 6;
    book_.set_phase_b(Group::Combat, ok);
    if (!ok) trip(Group::Combat, GroupWhy::PhaseB);
  }
  // flags: the known flag is set.
  if (book_.live(Group::Flags)) {
    const auto flags = item_object(gt, Group::Flags, Item::EventFlags);
    std::optional<bool> set;
    if (flags) {
      if (const auto block = flag_block(gt, *flags, kFlagKnownSet / kFlagDivisor)) {
        std::array<std::byte, 1> byte{};
        const std::uint32_t at = (kFlagKnownSet % kFlagDivisor) / 8;
        if (reader_.copy_into(gt, Group::Flags, *block + at, byte)) {
          set = ((static_cast<unsigned>(byte[0]) >> (7 - kFlagKnownSet % 8)) & 1u) != 0;
        }
      } else {
        set = false;
      }
    }
    const bool ok = set.value_or(false);
    book_.set_phase_b(Group::Flags, ok);
    if (!ok) trip(Group::Flags, GroupWhy::PhaseB);
  }
  if (book_.live(Group::Text)) {
    const bool ok = text_phase_b(gt);
    book_.set_phase_b(Group::Text, ok);
    if (!ok) trip(Group::Text, GroupWhy::PhaseB);
  }
  if (book_.live(Group::Ui)) book_.set_phase_b(Group::Ui, true);
  if (book_.live(Group::Place)) {
    const bool ok = out.clock.has_value();
    book_.set_phase_b(Group::Place, ok);
    if (!ok) trip(Group::Place, GroupWhy::PhaseB);
  }
  if (book_.live(Group::Camera)) {
    book_.set_phase_b(Group::Camera, cam_ok_);
    if (!cam_ok_) trip(Group::Camera, GroupWhy::PhaseB);
  }
  if (book_.live(Group::Params)) book_.set_phase_b(Group::Params, true);
}

void ErBackend::playthrough_ready(const GameThread& gt, const proto::WorldSnapshot& out) {
  if (!out.player || inert_) return;
  proto::Lifecycle::PlaythroughReady ready;
  ready.slot = out.player->save_slot;
  ready.archetype = out.player->archetype;
  ready.play_time_ms = out.player->play_time_ms;
  // The name, read once per playthrough and sent only here (ARCH §7.1): UTF-16 to UTF-8, printable only.
  const auto save = item_object(gt, Group::Player, Item::SaveData);
  const auto pd_addr = save ? save->ptr(b_.field(Fld::SavePlayerData)) : std::nullopt;
  if (const auto pd = pd_addr ? object(gt, Group::Player, Cls::PlayerData, *pd_addr, kPlayerData) : std::nullopt) {
    ready.keepsake = static_cast<std::uint8_t>(pd->integer(b_.field(Fld::Keepsake)).value_or(0));
    std::array<char16_t, 64> units{};
    if (const auto len = pd->utf16(b_.field(Fld::Name), units)) {
      std::array<char, 48> utf8{};
      std::size_t n = 0;
      for (std::size_t k = 0; k < *len; ++k) {
        const char16_t c = units[k];
        if (c < 0x20 || c == 0x7F || (c >= 0xD800 && c <= 0xDFFF)) continue;
        if (c < 0x80 && n + 1 <= utf8.size()) {
          utf8[n++] = static_cast<char>(c);
        } else if (c < 0x800 && n + 2 <= utf8.size()) {
          utf8[n++] = static_cast<char>(0xC0 | (c >> 6));
          utf8[n++] = static_cast<char>(0x80 | (c & 0x3F));
        } else if (c >= 0x800 && n + 3 <= utf8.size()) {
          utf8[n++] = static_cast<char>(0xE0 | (c >> 12));
          utf8[n++] = static_cast<char>(0x80 | ((c >> 6) & 0x3F));
          utf8[n++] = static_cast<char>(0x80 | (c & 0x3F));
        }
      }
      (void)ready.name.assign_prefix(std::string_view(utf8.data(), n));
    }
  }
  push_lifecycle(proto::Lifecycle{ready});
}

// ---- Commands and the render feed ----

Expected<void, proto::ApplyError> ErBackend::apply(const GameThread&, const proto::GameCommand& cmd) {
  if (inert_) return Unexpected{proto::ApplyError::Inert};
  if (const auto* w = std::get_if<proto::GameCommand::SetWatchFlags>(&cmd.payload)) {
    if (w->ids && w->ids->size() > proto::kMaxWatchFlags) return Unexpected{proto::ApplyError::Invalid};
    // The old list goes back to the core (C5): never freed here. With no room, the newer list is refused.
    if (watch_) {
      auto room = std::find_if(retired_.begin(), retired_.end(), [](const auto& r) { return !r; });
      if (room == retired_.end()) return Unexpected{proto::ApplyError::Invalid};
      *room = std::move(watch_);
    }
    watch_ = w->ids;
    watch_version_ = w->version;
    watch_count_ = watch_ ? watch_->size() : 0;
    for (std::size_t k = 0; k < watch_count_; ++k) watch_order_[k] = static_cast<std::uint16_t>(k);
    if (watch_) {
      const auto& ids = *watch_;
      std::sort(watch_order_.begin(), watch_order_.begin() + static_cast<std::ptrdiff_t>(watch_count_),
                [&ids](std::uint16_t a, std::uint16_t b) { return ids[a] / kFlagDivisor < ids[b] / kFlagDivisor; });
    }
    return {};
  }
  if (const auto* t = std::get_if<proto::GameCommand::SetTracked>(&cmd.payload)) {
    tracked_count_ = 0;
    for (const proto::ChrHandle h : t->handles) tracked_[tracked_count_++] = h;
    return {};
  }
  if (const auto* r = std::get_if<proto::GameCommand::ResolveText>(&cmd.payload)) {
    for (const std::uint32_t id : r->ids) {
      if (text_count_ >= text_asks_.size()) return Unexpected{proto::ApplyError::Invalid};
      text_asks_[text_count_++] = TextAsk{r->fmg, id};
    }
    return {};
  }
  if (const auto* r = std::get_if<proto::GameCommand::ResolveParams>(&cmd.payload)) {
    for (const std::uint32_t id : r->ids) {
      if (param_count_ >= param_asks_.size()) return Unexpected{proto::ApplyError::Invalid};
      param_asks_[param_count_++] = ParamAsk{r->param, id};
    }
    return {};
  }
  return {};  // Noop
}

Expected<void, proto::ReadError> ErBackend::render_feed(const GameThread& gt, std::span<const proto::ChrHandle> want,
                                                        proto::RenderFeed& out) {
  if (inert_) return Unexpected{proto::ReadError::Inert};
  if (mode_ != ReadMode::Full) return Unexpected{proto::ReadError::Loading};
  // The camera from the last snapshot, and each wanted handle's position, through the address the last walk found it
  // at, checked again now: its vtable a character's and its handle the same (ARCH §7.4; WHY§159).
  reader_.begin_tick();
  out.camera_ok = cam_ok_;
  out.view = cam_;
  out.fov_rad = cam_fov_;
  out.aspect = cam_aspect_;
  out.heads.clear();
  for (const proto::ChrHandle h : want) {
    proto::Head head{h, {}, false};
    for (std::size_t k = 0; k < seen_count_; ++k) {
      if (seen_[k].handle != h.raw) continue;
      bool mismatch = false;
      // Not the group's faults: a character gone since the snapshot is simply not present.
      const auto c = object(gt, Group::World, Cls::Character, seen_[k].address, kRenderChr, &mismatch, false);
      if (c && c->u64(b_.field(Fld::ChrHandle)) == std::optional<std::uint64_t>(h.raw)) {
        const auto parts_addr = c->ptr(b_.field(Fld::ChrParts));
        const auto parts = parts_addr ? object(gt, Group::World, Cls::Parts, *parts_addr, kRenderParts, nullptr, false)
                                      : std::nullopt;
        const auto mot_addr = parts ? parts->ptr(b_.field(Fld::PartsMotion)) : std::nullopt;
        const auto mot =
            mot_addr ? object(gt, Group::World, Cls::Motion, *mot_addr, kRenderMotion, nullptr, false) : std::nullopt;
        if (mot) {
          for (std::size_t i = 0; i < 3; ++i) head.pos[i] = mot->f32(b_.field(Fld::Pos), i).value_or(NAN);
          head.present = finite3(head.pos);
        }
      }
      break;
    }
    if (!out.heads.try_push_back(head)) break;
  }
  return {};
}

}  // namespace en::gal
