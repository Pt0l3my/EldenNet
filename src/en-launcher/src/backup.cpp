// Save backups with a rotation of 10 (backup.hpp; ARCH §4.4).
#include "en/launcher/backup.hpp"

#include <algorithm>
#include <fstream>
#include <optional>
#include <system_error>
#include <utility>

#include "en/launcher/blake3.hpp"
#include "en/launcher/steam.hpp"

namespace en::launcher {

namespace fs = std::filesystem;

std::vector<std::string> elden_ring_save_files() { return {"ER0000.sl2", "ER0000.sl2.bak"}; }

namespace {

bool digits(std::string_view s) noexcept {
  return !s.empty() && std::all_of(s.begin(), s.end(), [](char c) { return c >= '0' && c <= '9'; });
}

// The order of backups: the stamp, then the same-second number (1 for none).
std::pair<std::string, int> backup_key(const std::string& name) {
  if (name.size() > 15) return {name.substr(0, 15), std::stoi(name.substr(16))};
  return {name, 1};
}

std::vector<fs::path> backups_of(const fs::path& id_dir) {
  std::vector<fs::path> out;
  std::error_code ec;
  if (!fs::is_directory(id_dir, ec)) return out;
  for (fs::directory_iterator it(id_dir, ec), end; !ec && it != end; it.increment(ec)) {
    const std::string name = path_to_utf8(it->path().filename());
    if (it->is_directory(ec) && is_backup_name(name)) out.push_back(it->path());
  }
  std::sort(out.begin(), out.end(), [](const fs::path& a, const fs::path& b) {
    return backup_key(path_to_utf8(a.filename())) < backup_key(path_to_utf8(b.filename()));
  });
  return out;
}

struct Present {
  std::string name;
  FileDigest digest;
};

bool same_as(const fs::path& backup, const std::vector<Present>& files) {
  std::error_code ec;
  std::size_t count = 0;
  for (fs::directory_iterator it(backup, ec), end; !ec && it != end; it.increment(ec)) {
    if (it->is_regular_file(ec) && path_to_utf8(it->path().filename()) != "manifest.toml") ++count;
  }
  if (ec || count != files.size()) return false;
  for (const Present& p : files) {
    const auto d = digest_file(backup / path_from_utf8(p.name));
    if (!d || d->size != p.digest.size || d->blake3 != p.digest.blake3) return false;
  }
  return true;
}

std::string toml_quoted(std::string_view s) {
  std::string out = "\"";
  for (const char c : s) {
    if (c == '"' || c == '\\') out.push_back('\\');
    out.push_back(c);
  }
  out.push_back('"');
  return out;
}

bool write_manifest(const fs::path& dir, const std::string& stamp, const std::vector<Present>& files) {
  const fs::path tmp = dir / "manifest.toml.tmp";
  {
    std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    out << "# EldenNet's save backup (ARCH 4.4): the files copied, with their sizes and BLAKE3 hashes.\n"
        << "schema = 1\nmade = " << toml_quoted(stamp) << "\n";
    for (const Present& p : files) {
      out << "\n[[file]]\nname = " << toml_quoted(p.name) << "\nsize = " << p.digest.size
          << "\nblake3 = " << toml_quoted(to_hex(p.digest.blake3)) << "\n";
    }
    out.flush();
    if (!out) return false;
  }
  std::error_code ec;
  fs::rename(tmp, dir / "manifest.toml", ec);
  return !ec;
}

// One copy: to `.tmp`, checked against the source's size and hash, then renamed. A fixed problem, or nothing.
std::optional<std::string> copy_checked(const fs::path& from, const fs::path& dir, const Present& p) {
  std::error_code ec;
  const fs::path tmp = dir / path_from_utf8(p.name + ".tmp");
  if (!fs::copy_file(from, tmp, fs::copy_options::overwrite_existing, ec) || ec) return "a copy cannot be written";
  const auto d = digest_file(tmp);
  if (!d || d->size != p.digest.size || d->blake3 != p.digest.blake3) return "a copy differs from its save file";
  fs::rename(tmp, dir / path_from_utf8(p.name), ec);
  if (ec) return "a copy cannot be renamed";
  return std::nullopt;
}

}  // namespace

bool is_backup_name(std::string_view name) noexcept {
  if (name.size() != 15 && (name.size() < 17 || name.size() > 18)) return false;
  if (!digits(name.substr(0, 8)) || name[8] != '-' || !digits(name.substr(9, 6))) return false;
  if (name.size() == 15) return true;
  if (name[15] != '-' || !digits(name.substr(16))) return false;
  const int n = name.size() == 17 ? name[16] - '0' : (name[16] - '0') * 10 + (name[17] - '0');
  return n >= 2 && n <= 99 && name[16] != '0';
}

bool is_steam_id_folder(std::string_view name) noexcept { return name.size() == 17 && digits(name); }

BackupReport back_up_saves(const BackupSpec& spec) {
  BackupReport r;
  std::error_code ec;
  if (!fs::is_directory(spec.saves_root, ec)) return r;  // no saves yet: nothing to back up
  std::vector<fs::path> folders;
  for (fs::directory_iterator it(spec.saves_root, ec), end; !ec && it != end; it.increment(ec)) {
    if (it->is_directory(ec) && is_steam_id_folder(path_to_utf8(it->path().filename()))) folders.push_back(it->path());
  }
  if (ec) {
    r.problems.emplace_back("the save folder cannot be listed");
    return r;
  }
  std::sort(folders.begin(), folders.end());
  for (const fs::path& folder : folders) {
    ++r.folders;
    std::vector<Present> present;
    bool readable = true;
    for (const std::string& name : spec.files) {
      const fs::path file = folder / path_from_utf8(name);
      if (!fs::is_regular_file(file, ec)) continue;
      const auto d = digest_file(file);
      if (!d) {
        readable = false;
        break;
      }
      present.push_back(Present{name, *d});
    }
    if (!readable) {
      r.problems.emplace_back("a save file cannot be read");
      continue;
    }
    if (present.empty()) {
      ++r.empty;
      continue;
    }
    const fs::path id_dir = spec.backups_root / folder.filename();
    if (const auto existing = backups_of(id_dir); !existing.empty() && same_as(existing.back(), present)) {
      ++r.unchanged;
      continue;
    }
    fs::path dest = id_dir / path_from_utf8(spec.stamp);
    for (int n = 2; fs::exists(dest, ec) && n <= 99; ++n)
      dest = id_dir / path_from_utf8(spec.stamp + "-" + std::to_string(n));
    if (fs::exists(dest, ec) || !fs::create_directories(dest, ec) || ec) {
      r.problems.emplace_back("a backup folder cannot be made");
      continue;
    }
    std::optional<std::string> failed;
    for (const Present& p : present) {
      failed = copy_checked(folder / path_from_utf8(p.name), dest, p);
      if (failed) break;
    }
    if (!failed && !write_manifest(dest, spec.stamp, present)) failed = "the manifest cannot be written";
    if (failed) {
      fs::remove_all(dest, ec);  // a half-made backup never stays
      r.problems.push_back(*failed);
      continue;
    }
    ++r.made;
    // The rotation, only now that a new backup exists.
    auto all = backups_of(id_dir);
    while (all.size() > spec.keep && spec.keep > 0) {
      fs::remove_all(all.front(), ec);
      if (ec) break;
      ++r.removed;
      all.erase(all.begin());
    }
  }
  return r;
}

}  // namespace en::launcher
