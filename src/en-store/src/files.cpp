// The playthrough files on disk (files.hpp; MEM §8.1 to §8.3).
#include "en/store/files.hpp"

#include <algorithm>
#include <chrono>
#include <system_error>

namespace en::store {

namespace fs = std::filesystem;

namespace {

bool starts_with(std::string_view s, std::string_view p) { return s.substr(0, p.size()) == p; }
bool ends_with(std::string_view s, std::string_view p) {
  return s.size() >= p.size() && s.substr(s.size() - p.size()) == p;
}

// The files of a folder whose names begin with `prefix` and end with `suffix`, sorted by name.
std::vector<fs::path> files_like(const fs::path& dir, std::string_view prefix, std::string_view suffix) {
  std::vector<fs::path> out;
  std::error_code ec;
  for (const fs::directory_entry& e : fs::directory_iterator(dir, ec)) {
    if (!e.is_regular_file(ec)) continue;
    const std::string name = e.path().filename().string();
    if (starts_with(name, prefix) && ends_with(name, suffix)) out.push_back(e.path());
  }
  std::sort(out.begin(), out.end());
  return out;
}

fs::path with_suffix(const fs::path& p, std::string_view suffix) {
  fs::path q = p;
  q += std::string(suffix);
  return q;
}

void remove_quietly(const fs::path& p) {
  std::error_code ec;
  fs::remove(p, ec);
}

bool rename_with_sidecars(const fs::path& from, const fs::path& to) {
  std::error_code ec;
  fs::rename(from, to, ec);
  if (ec) return false;
  for (const char* side : {"-wal", "-shm"}) {
    if (fs::exists(with_suffix(from, side), ec)) {
      std::error_code ec2;
      fs::rename(with_suffix(from, side), with_suffix(to, side), ec2);
    }
  }
  return true;
}

// Empties a closed file's WAL into it: opened, a TRUNCATE checkpoint, closed. A file that does not open is left as it
// is (it is archived or set aside whole, its sidecars with it).
void settle(const fs::path& p) {
  auto db = Db::open(p, FileKind::Playthrough, false);
  if (!db) return;
  (void)checkpoint(*db, true);
  db->close();
}

std::int64_t wall_of(const fs::path& p) {
  std::error_code ec;
  const auto t = fs::last_write_time(p, ec);
  if (ec) return 0;
  const auto sys = std::chrono::clock_cast<std::chrono::system_clock>(t);
  return std::chrono::duration_cast<std::chrono::seconds>(sys.time_since_epoch()).count();
}

}  // namespace

fs::path SaveFolder::live(std::uint8_t slot, std::string_view fp8) const {
  return dir / (file_stem(slot, fp8) + ".sqlite");
}

SaveFolder save_folder(const fs::path& saves, std::uint64_t steamid64) {
  return SaveFolder{saves / std::to_string(steamid64)};
}

std::string file_stem(std::uint8_t slot, std::string_view fp8) {
  return "pt-s" + std::to_string(slot) + "-" + std::string(fp8);
}

std::string utc_stamp(std::int64_t wall) {
  // Days from the civil calendar's epoch, by Howard Hinnant's public-domain algorithm (no gmtime, no locale).
  std::int64_t days = wall >= 0 ? wall / 86400 : (wall - 86399) / 86400;
  const std::int64_t secs = wall - days * 86400;
  days += 719468;
  const std::int64_t era = (days >= 0 ? days : days - 146096) / 146097;
  const std::int64_t doe = days - era * 146097;
  const std::int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  std::int64_t y = yoe + era * 400;
  const std::int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const std::int64_t mp = (5 * doy + 2) / 153;
  const std::int64_t d = doy - (153 * mp + 2) / 5 + 1;
  const std::int64_t m = mp < 10 ? mp + 3 : mp - 9;
  if (m <= 2) ++y;
  const auto digits = [](std::int64_t v, std::size_t width) {
    std::string s = number_text(v < 0 ? -v : v);
    if (s.size() < width) s.insert(0, width - s.size(), '0');
    return s;
  };
  return digits(y, 4) + digits(m, 2) + digits(d, 2) + "T" + digits(secs / 3600, 2) + digits(secs % 3600 / 60, 2) +
         digits(secs % 60, 2) + "Z";
}

Result<FileIdentity> identity_of(const Db& db) {
  FileIdentity id;
  const auto text = [&](std::string_view key) -> Result<std::string> {
    auto v = meta_get(db, key);
    if (!v) return Unexpected{v.error()};
    return v->value_or(std::string());
  };
  auto fp8 = text("fp8");
  if (!fp8) return Unexpected{fp8.error()};
  auto name = text("name_hash");
  if (!name) return Unexpected{name.error()};
  auto arch = text("archetype");
  if (!arch) return Unexpected{arch.error()};
  auto gift = text("gift");
  if (!gift) return Unexpected{gift.error()};
  id.fp8 = *fp8;
  id.name_hash = *name;
  if (const auto a = number_of(*arch); a && *a >= 0 && *a <= 255) id.archetype = static_cast<std::uint8_t>(*a);
  if (const auto g = number_of(*gift); g && *g >= 0 && *g <= 255) id.gift = static_cast<std::uint8_t>(*g);
  return id;
}

bool soft_match(const FileIdentity& file, const proto::PlaythroughKey& key) noexcept {
  if (file.name_hash.empty() || file.name_hash != key.name_hash) return false;
  if (!file.archetype || *file.archetype != key.archetype) return false;
  const std::uint8_t g = file.gift.value_or(0xFF);
  return g == key.gift || g == 0xFF || key.gift == 0xFF;
}

bool archive_file(const SaveFolder& folder, const fs::path& live, std::int64_t wall) {
  std::error_code ec;
  fs::create_directories(folder.archive(), ec);
  settle(live);
  const std::string stem = live.stem().string();
  fs::path to = folder.archive() / (stem + "-" + utc_stamp(wall) + ".sqlite");
  for (int n = 2; fs::exists(to, ec) && n < 100; ++n) {
    to = folder.archive() / (stem + "-" + utc_stamp(wall) + "-" + std::to_string(n) + ".sqlite");
  }
  return rename_with_sidecars(live, to);
}

Result<Located> locate(const SaveFolder& folder, const proto::PlaythroughKey& key, std::int64_t wall) {
  std::error_code ec;
  fs::create_directories(folder.dir, ec);
  if (ec) return Unexpected{Fault::IoErr};
  fs::create_directories(folder.backups(), ec);
  fs::create_directories(folder.archive(), ec);
  Located out;
  out.path = folder.live(key.slot, key.fp8);
  // 1. The live file.
  if (fs::exists(out.path, ec)) return out;
  // The slot's other live files (never a set-aside one).
  std::vector<fs::path> others;
  for (const fs::path& p : files_like(folder.dir, "pt-s" + std::to_string(key.slot) + "-", ".sqlite")) {
    if (p.filename().string().find(".corrupt-") == std::string::npos) others.push_back(p);
  }
  // 2. A live file of the same character under another fingerprint (the starting gift read or not).
  for (const fs::path& p : others) {
    std::optional<FileIdentity> id;
    {
      auto db = Db::open(p, FileKind::Playthrough, false);
      if (!db) continue;
      auto got = identity_of(*db);
      if (got) id = *got;
      (void)checkpoint(*db, true);
    }
    if (id && soft_match(*id, key)) {
      if (!rename_with_sidecars(p, out.path)) return Unexpected{Fault::IoErr};
      out.renamed = true;
      return out;
    }
  }
  // 3. An archived file of this fingerprint comes back; 4. else a new file. Either way the slot's others are archived.
  const auto archived = files_like(folder.archive(), file_stem(key.slot, key.fp8) + "-", ".sqlite");
  for (const fs::path& p : others) {
    if (archive_file(folder, p, wall)) out.archived_other = true;
  }
  if (!archived.empty()) {
    if (!rename_with_sidecars(archived.back(), out.path)) return Unexpected{Fault::IoErr};
    out.restored_archive = true;
    return out;
  }
  out.create = true;
  return out;
}

Result<void> rotate_backup(const Db& db, const SaveFolder& folder, std::uint8_t slot, std::string_view fp8) {
  std::error_code ec;
  fs::create_directories(folder.backups(), ec);
  const std::string stem = file_stem(slot, fp8);
  const auto nth = [&](int n) { return folder.backups() / (stem + "." + std::to_string(n) + ".sqlite"); };
  remove_quietly(nth(3));
  fs::rename(nth(2), nth(3), ec);
  fs::rename(nth(1), nth(2), ec);
  return backup_into(db, nth(1));
}

fs::path pre_migration_backup(const SaveFolder& folder, std::uint8_t slot, std::string_view fp8, std::uint32_t from,
                              std::uint32_t to, std::size_t keep) {
  std::error_code ec;
  fs::create_directories(folder.backups(), ec);
  const std::string stem = file_stem(slot, fp8);
  const fs::path target =
      folder.backups() / (stem + ".pre-" + std::to_string(from) + "-" + std::to_string(to) + ".sqlite");
  remove_quietly(target);
  auto olds = files_like(folder.backups(), stem + ".pre-", ".sqlite");
  std::sort(olds.begin(), olds.end(), [](const fs::path& a, const fs::path& b) { return wall_of(a) > wall_of(b); });
  for (std::size_t i = keep > 0 ? keep - 1 : 0; i < olds.size(); ++i) remove_quietly(olds[i]);
  return target;
}

std::optional<fs::path> newest_backup(const SaveFolder& folder, std::uint8_t slot, std::string_view fp8) {
  std::error_code ec;
  for (int n = 1; n <= 3; ++n) {
    const fs::path p = folder.backups() / (file_stem(slot, fp8) + "." + std::to_string(n) + ".sqlite");
    if (fs::exists(p, ec)) return p;
  }
  return std::nullopt;
}

BackupInfo backups_of(const SaveFolder& folder, std::uint8_t slot, std::string_view fp8) {
  BackupInfo info;
  std::error_code ec;
  for (int n = 1; n <= 3; ++n) {
    const fs::path p = folder.backups() / (file_stem(slot, fp8) + "." + std::to_string(n) + ".sqlite");
    if (!fs::exists(p, ec)) continue;
    ++info.count;
    info.walls.push_back(wall_of(p));
  }
  return info;
}

bool set_aside(const fs::path& live, std::int64_t wall) {
  return rename_with_sidecars(live, with_suffix(live, ".corrupt-" + utc_stamp(wall)));
}

bool restore_newest(const SaveFolder& folder, std::uint8_t slot, std::string_view fp8) {
  const auto b = newest_backup(folder, slot, fp8);
  if (!b) return false;
  std::error_code ec;
  const fs::path live = folder.live(slot, fp8);
  fs::copy_file(*b, live, fs::copy_options::overwrite_existing, ec);
  return !ec;
}

}  // namespace en::store
