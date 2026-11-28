// MEM §8.2's SteamID discovery (steamid.hpp).
#include "en/shell/steamid.hpp"

#include <algorithm>

namespace en::shell {

const char* to_string(AccountWhy w) noexcept {
  switch (w) {
    case AccountWhy::One: return "one";
    case AccountWhy::Newest: return "newest";
    case AccountWhy::Changed: return "changed";
    case AccountWhy::None: return "none";
  }
  return "none";
}

std::optional<std::uint64_t> steamid_folder(std::string_view name) noexcept {
  if (name.size() != 17) return std::nullopt;
  std::uint64_t v = 0;
  for (const char c : name) {
    if (c < '0' || c > '9') return std::nullopt;
    v = v * 10 + static_cast<std::uint64_t>(c - '0');  // 17 digits stay below 2^64
  }
  if (v == 0) return std::nullopt;
  return v;
}

bool save_file(const SaveFileMeta& f) noexcept { return f.regular && f.bytes >= kSaveFileMinBytes; }

AccountPick pick_account(const std::vector<AccountFolder>& listing, std::optional<std::int64_t> since) {
  struct Seen {
    std::uint64_t id = 0;
    std::optional<std::int64_t> newest;  // its newest save file
  };
  std::vector<Seen> seen;
  for (const AccountFolder& f : listing) {
    const std::optional<std::uint64_t> id = steamid_folder(f.name);
    if (!id) continue;
    Seen s{*id, std::nullopt};
    for (const SaveFileMeta& m : f.files) {
      if (save_file(m) && (!s.newest || m.modified > *s.newest)) s.newest = m.modified;
    }
    seen.push_back(s);
  }
  AccountPick pick;
  pick.folders = seen.size();
  if (seen.empty()) return pick;
  if (seen.size() == 1) {
    pick.steamid64 = seen.front().id;
    pick.why = AccountWhy::One;
    return pick;
  }
  std::sort(seen.begin(), seen.end(), [](const Seen& a, const Seen& b) { return a.id < b.id; });
  const Seen* best = nullptr;
  for (const Seen& s : seen) {
    if (s.newest && (best == nullptr || *s.newest > *best->newest)) best = &s;
  }
  if (best == nullptr) return pick;  // several, and no save file in any: no guess
  pick.steamid64 = best->id;
  pick.why = since && *best->newest >= *since ? AccountWhy::Changed : AccountWhy::Newest;
  return pick;
}

}  // namespace en::shell
