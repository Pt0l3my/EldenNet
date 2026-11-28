// The store in the DLL (dll.hpp; ARCH §1.2's T9, §1.3's C8; MEM §8.2, §8.6): T9 started by T6 on
// %APPDATA%\EldenNet\saves, the account looked for, the core's requests sent through the spill ring, and T9's answers
// handed back to the core in order, each logged as words and numbers.
//
// - The account (MEM §8.2, en_shell's steamid.hpp): the 17-digit folders of the game's own %APPDATA%\EldenRing\ (the
//   real one: ELDENNET_APPDATA stands in only where EldenNet's files go), listed for metadata alone, looked at when T6
//   starts and again at each PlaythroughReady, so that a save written since the game started names its folder. The
//   tests' switch, ELDENNET_TEST_SAVES in the environment, names a folder in its place; a probe build alone takes it,
//   and the release DLL logs that it was asked and refuses. The SteamID is never logged: the log says only why the
//   folder was chosen and how many there were.
// - The spill ring (en_shell's spill.hpp) holds what C8 has no room for; the core hears when events stop being saved
//   and when the chat must hold its pages (CoreInput::StoreBacklog).
// - Nothing here logs a path, the key, chat or model text: an answer is logged by its kind and its numbers (PV rules).
#include <ctime>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "dll.hpp"
#include "en/proto/ffi_guard.hpp"
#include "en/shell/spill.hpp"
#include "en/shell/steamid.hpp"
#include "en/store/loop.hpp"

namespace eldennet {

using en::shell::Line;
using en::shell::LogLevel;
namespace db = en::proto::db;

struct StoreLink {
  std::unique_ptr<en::store::StoreHandle> handle;  // T9: kept for the process (R14), never stopped
  en::shell::SpillRing ring;
  en::core::CoreInput::StoreBacklog told;  // what the core last heard of the ring
  std::uint64_t sent = 0, failed = 0;
  std::optional<std::uint64_t> account;  // the last account told to the core
  bool account_told = false;
};

namespace {

constinit StoreLink* g_store = nullptr;  // T6's alone

std::int64_t unix_of(const FILETIME& ft) {
  ULARGE_INTEGER u;
  u.LowPart = ft.dwLowDateTime;
  u.HighPart = ft.dwHighDateTime;
  return static_cast<std::int64_t>(u.QuadPart / 10000000ull) - 11644473600ll;
}

std::wstring env(const wchar_t* name) {
  wchar_t buf[1024];
  const DWORD n = GetEnvironmentVariableW(name, buf, 1024);
  if (n == 0 || n >= 1024) return {};
  return std::wstring(buf, n);
}

// The folder whose 17-digit folders are the accounts: the game's own, or the tests' in a probe build.
std::wstring accounts_root() {
  const std::wstring asked = env(L"ELDENNET_TEST_SAVES");
  if (!asked.empty()) {
    if (kProbeBuild) return asked;
    log(Line("account").word("test_saves", "refused"), LogLevel::Warn);
  }
  const std::wstring appdata = env(L"APPDATA");
  return appdata.empty() ? std::wstring() : appdata + L"\\EldenRing";
}

std::string ascii_of(std::wstring_view w) {
  std::string out;
  for (const wchar_t c : w) out.push_back(c < 0x80 ? static_cast<char>(c) : '?');
  return out;
}

// MEM §8.2's listing: each folder's name and its files' sizes and times, no more (never a save's contents).
std::vector<en::shell::AccountFolder> list_accounts(const std::wstring& root) {
  std::vector<en::shell::AccountFolder> out;
  if (root.empty()) return out;
  WIN32_FIND_DATAW fd;
  HANDLE h = FindFirstFileExW((root + L"\\*").c_str(), FindExInfoBasic, &fd, FindExSearchLimitToDirectories, nullptr,
                              0);
  if (h == INVALID_HANDLE_VALUE) return out;
  do {
    if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0 || (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) {
      continue;
    }
    const std::string name = ascii_of(fd.cFileName);
    if (!en::shell::steamid_folder(name)) continue;
    en::shell::AccountFolder folder{name, {}};
    WIN32_FIND_DATAW f;
    HANDLE g = FindFirstFileExW((root + L"\\" + fd.cFileName + L"\\*").c_str(), FindExInfoBasic, &f,
                                FindExSearchNameMatch, nullptr, 0);
    if (g != INVALID_HANDLE_VALUE) {
      do {
        en::shell::SaveFileMeta m;
        m.regular = (f.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT |
                                           FILE_ATTRIBUTE_DEVICE)) == 0;
        m.bytes = (static_cast<std::uint64_t>(f.nFileSizeHigh) << 32) | f.nFileSizeLow;
        m.modified = unix_of(f.ftLastWriteTime);
        if (folder.files.size() < 64) folder.files.push_back(m);
      } while (FindNextFileW(g, &f));
      FindClose(g);
    }
    out.push_back(std::move(folder));
  } while (FindNextFileW(h, &fd) && out.size() < 64);
  FindClose(h);
  return out;
}

// When the game's process started, for MEM §8.2's "a save changed since the game started".
std::optional<std::int64_t> process_start() {
  FILETIME created, exited, kernel, user;
  if (!GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user)) return std::nullopt;
  return unix_of(created);
}

std::optional<std::uint64_t> free_bytes(const std::filesystem::path& folder) {
  ULARGE_INTEGER avail;
  if (!GetDiskFreeSpaceExW(folder.c_str(), &avail, nullptr, nullptr)) return std::nullopt;
  return avail.QuadPart;
}

const char* rollback_word(db::Rollback::Kind k) {
  switch (k) {
    case db::Rollback::Kind::Clean: return "clean";
    case db::Rollback::Kind::Trimmed: return "trimmed";
    case db::Rollback::Kind::Choice: return "choice";
  }
  return "clean";
}

void log_opened(const char* event, const db::Opened& o) {
  Line l(event);
  l.word("rollback", rollback_word(o.rollback.kind)).num("journey", o.journey).flag("created", o.created);
  l.flag("restored_backup", o.restored_backup).flag("restored_archive", o.restored_archive);
  l.flag("archived_other", o.archived_other).flag("migrated", o.migrated).flag("low_disk", o.low_disk);
  if (o.rollback.kind == db::Rollback::Kind::Trimmed) l.num("rows", o.rollback.rows);
  if (o.rollback.kind != db::Rollback::Kind::Clean) l.flag("lower_journey", o.rollback.lower_journey);
  l.num("closed", o.closed_conversations.size()).num("jobs", o.open_jobs.size());
  log(l);
}

// One of T9's answers, in the log: the open and its answer to the choice, a failure and a trip; the rest counted.
void log_result(StoreLink& link, const en::proto::DbResult& r) {
  if (const auto* o = std::get_if<db::Opened>(&r.result)) {
    log_opened("store_open", *o);
  } else if (const auto* v = std::get_if<db::Resolved>(&r.result)) {
    log_opened("store_resolved", v->opened);
  } else if (const auto* f = std::get_if<db::Failed>(&r.result)) {
    ++link.failed;
    log(Line("store_failed").word("fault", db::to_string(f->fault)).num("failed", link.failed), LogLevel::Warn);
  } else if (const auto* t = std::get_if<db::Tripped>(&r.result)) {
    log(Line("store_tripped").word("cause", db::to_string(t->cause)), LogLevel::Warn);
  }
}

}  // namespace

void store_start(Shell& s, std::vector<en::core::CoreInput>& to_core) noexcept {
  en::proto::ffi_guard(en::proto::Component::DllMain, [&] {
    if (g_store != nullptr) return;
    auto* link = new StoreLink();  // lives for the process (R14)
    const std::filesystem::path saves = std::filesystem::path(s.paths.data) / L"saves";
    en::store::StoreCfg cfg;
    cfg.saves = saves;
    cfg.build = std::string("eldennet-") + std::string(kVersionLabel);
    cfg.wall = [] { return static_cast<std::int64_t>(std::time(nullptr)); };
    cfg.mono_ms = [] { return mono_ms(); };
    cfg.free_bytes = [](const std::filesystem::path& p) { return free_bytes(p); };
    HANDLE wake = s.wake;
    link->handle = en::store::Store::spawn(std::move(cfg), [wake] { SetEvent(wake); });
    g_store = link;
    log(Line("store").word("state", "started"));
    store_account(s, to_core);
  });
}

void store_account(Shell&, std::vector<en::core::CoreInput>& to_core) noexcept {
  en::proto::ffi_guard(en::proto::Component::DllMain, [&] {
    StoreLink* link = g_store;
    if (link == nullptr) return;
    const en::shell::AccountPick pick = en::shell::pick_account(list_accounts(accounts_root()), process_start());
    if (link->account_told && link->account == pick.steamid64) return;
    log(Line("account").word("why", en::shell::to_string(pick.why)).num("folders", pick.folders),
        pick.steamid64 == 0 ? LogLevel::Warn : LogLevel::Info);
    link->account = pick.steamid64;
    link->account_told = true;
    to_core.push_back(en::core::CoreInput{en::core::CoreInput::Account{pick.steamid64}});
  });
}

void store_poll(Shell&, std::vector<en::proto::DbResult>& out) noexcept {
  en::proto::ffi_guard(en::proto::Component::DllMain, [&] {
    StoreLink* link = g_store;
    if (link == nullptr) return;
    en::proto::DbResult r;
    while (link->handle->poll(r)) {
      log_result(*link, r);
      out.push_back(std::move(r));
    }
  });
}

void store_send(Shell&, std::vector<en::proto::DbRequest>& requests) noexcept {
  en::proto::ffi_guard(en::proto::Component::DllMain, [&] {
    StoreLink* link = g_store;
    if (link == nullptr || requests.empty()) return;
    const auto send = [link](en::proto::DbRequest& r) { return link->handle->try_send(r); };
    for (en::proto::DbRequest& r : requests) {
      if (link->ring.push(std::move(r), send) == en::shell::SpillRing::Push::Sent) ++link->sent;
    }
    requests.clear();
  });
}

std::optional<en::core::CoreInput::StoreBacklog> store_backlog(Shell&) noexcept {
  std::optional<en::core::CoreInput::StoreBacklog> changed;
  en::proto::ffi_guard(en::proto::Component::DllMain, [&] {
    StoreLink* link = g_store;
    if (link == nullptr) return;
    const auto send = [link](en::proto::DbRequest& r) { return link->handle->try_send(r); };
    link->ring.drain(send, link->handle->waiting());
    const en::core::CoreInput::StoreBacklog now{link->ring.behind(), link->ring.pages_held()};
    if (now.behind == link->told.behind && now.pages_held == link->told.pages_held) return;
    log(Line("store_backlog").flag("behind", now.behind).flag("pages_held", now.pages_held)
            .num("spilled", link->ring.spilled()).num("not_saved", link->ring.not_saved()),
        now.behind ? LogLevel::Warn : LogLevel::Info);
    link->told = now;
    changed = now;
  });
  return changed;
}

void store_status() noexcept {
  const StoreLink* link = g_store;
  if (link == nullptr) return;
  log(Line("store_status").num("sent", link->sent).num("waiting", link->handle->waiting())
          .num("spilled", link->ring.spilled()).num("not_saved", link->ring.not_saved()).num("failed", link->failed));
}

}  // namespace eldennet
