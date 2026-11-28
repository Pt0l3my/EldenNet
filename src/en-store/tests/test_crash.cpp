// C11's hard kill (MEM §8.6): a child process of the test program's own appends through T9 and writes each
// acknowledged id to a file, and is killed without warning (SIGKILL on Linux, TerminateProcess on Windows and under
// Wine). Every acknowledged id is in the file afterwards, and the file is whole and opens again.
#include <doctest.h>

#include <algorithm>
#include <charconv>
#include <chrono>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "en/store/loop.hpp"
#include "support.hpp"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
extern char** environ;
#endif

using namespace en::store;
using namespace en::store::test;
namespace proto = en::proto;
namespace db = en::proto::db;
namespace fs = std::filesystem;

namespace {

constexpr int kBatch = 24;      // more than 16: each batch shares a transaction
constexpr int kBatches = 3000;  // the child's end if no one kills it
constexpr std::size_t kAcksBeforeKill = 400;

bool next_result(StoreHandle& h, proto::DbResult& out) {
  const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(30);
  while (!h.poll(out)) {
    if (std::chrono::steady_clock::now() >= until) return false;
    std::this_thread::sleep_for(std::chrono::microseconds(200));
  }
  return true;
}

// The complete lines of the acknowledgements file: a line the kill cut short is no acknowledgement.
std::vector<proto::EventId> acked(const fs::path& p) {
  std::ifstream in(p, std::ios::binary);
  std::stringstream all;
  all << in.rdbuf();
  const std::string text = all.str();
  std::vector<proto::EventId> out;
  std::size_t start = 0;
  for (std::size_t nl = text.find('\n'); nl != std::string::npos; nl = text.find('\n', start)) {
    out.push_back(std::stoll(text.substr(start, nl - start)));
    start = nl + 1;
  }
  return out;
}

// The test program itself, run as the crash child, and its hard kill.
class Child {
 public:
  Child(const fs::path& saves, const fs::path& acks) {
#ifdef _WIN32
    std::wstring exe(32768, L'\0');
    exe.resize(GetModuleFileNameW(nullptr, exe.data(), static_cast<DWORD>(exe.size())));
    std::wstring cmd = L"\"" + exe + L"\" --crash-child \"" + saves.wstring() + L"\" \"" + acks.wstring() + L"\"";
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    started_ = CreateProcessW(exe.c_str(), cmd.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi_) != 0;
#else
    const std::string exe = fs::read_symlink("/proc/self/exe").string();
    std::string a0 = exe, a1 = "--crash-child", a2 = saves.string(), a3 = acks.string();
    char* argv[] = {a0.data(), a1.data(), a2.data(), a3.data(), nullptr};
    started_ = posix_spawn(&pid_, exe.c_str(), nullptr, nullptr, argv, environ) == 0;
#endif
  }
  ~Child() { kill(); }
  Child(const Child&) = delete;
  Child& operator=(const Child&) = delete;
  [[nodiscard]] bool started() const { return started_; }
  void kill() {
    if (!started_) return;
    started_ = false;
#ifdef _WIN32
    TerminateProcess(pi_.hProcess, 9);
    WaitForSingleObject(pi_.hProcess, 30000);
    CloseHandle(pi_.hThread);
    CloseHandle(pi_.hProcess);
#else
    ::kill(pid_, SIGKILL);
    int status = 0;
    waitpid(pid_, &status, 0);
#endif
  }

 private:
  bool started_ = false;
#ifdef _WIN32
  PROCESS_INFORMATION pi_{};
#else
  pid_t pid_ = 0;
#endif
};

}  // namespace

namespace en::store::test {

// argv: the program, --crash-child, the saves folder, the acknowledgements file.
int crash_child(int argc, char** argv) {
  if (argc < 4) return 2;
  StoreCfg cfg;
  cfg.saves = fs::path(argv[2]);
  cfg.build = "crash-child";
  auto h = Store::spawn(cfg, {});
  proto::DbRequest open;
  open.seq = 1;
  open.op = db::Open{invented_key(), 60000, 1};
  if (!h->send(std::move(open))) return 3;
  proto::DbResult r;
  if (!next_result(*h, r)) return 4;
  const auto* o = std::get_if<db::Opened>(&r.result);
  if (o == nullptr) return 5;
  const proto::EventId first = o->next_event_id;
  std::ofstream acks(fs::path(argv[3]), std::ios::binary | std::ios::app);
  if (!acks) return 6;
  std::uint64_t seq = 1;
  for (int b = 0; b < kBatches; ++b) {
    for (int i = 0; i < kBatch; ++i) {
      db::Append a;
      ++seq;
      const proto::EventId id = first + static_cast<proto::EventId>(seq - 2);
      a.event = event(id, "PlaceEntered", 60000 + static_cast<proto::Pt>(seq), {{"kale", proto::How::Direct}});
      proto::DbRequest req;
      req.seq = seq;
      req.op = std::move(a);
      if (!h->send(std::move(req))) return 7;
    }
    for (int i = 0; i < kBatch; ++i) {
      if (!next_result(*h, r)) return 8;
      if (!std::holds_alternative<db::Done>(r.result)) continue;
      // Written and flushed to the system at once: a hard kill keeps what the process handed over.
      char line[24];
      auto [end, ec] = std::to_chars(line, line + sizeof line - 1, first + static_cast<proto::EventId>(r.seq - 2));
      if (ec != std::errc{}) return 9;
      *end++ = '\n';
      acks.write(line, end - line);
      acks.flush();
    }
  }
  return 0;
}

}  // namespace en::store::test

TEST_CASE("store: a hard kill loses no acknowledged append, and the file is whole and opens again") {
  TempDir dir;
  const fs::path saves = dir.path() / "saves";
  const fs::path acks = dir.path() / "acks.txt";
  Child child(saves, acks);
  REQUIRE(child.started());
  const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(120);
  while (acked(acks).size() < kAcksBeforeKill && std::chrono::steady_clock::now() < until) {
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  child.kill();
  const std::vector<proto::EventId> ids = acked(acks);
  REQUIRE(ids.size() >= kAcksBeforeKill);

  const proto::PlaythroughKey k = invented_key();
  const fs::path live = save_folder(saves, k.steamid64).live(k.slot, k.fp8);
  proto::EventId highest = 0;
  {
    auto d = Db::open(live, FileKind::Playthrough, false);
    REQUIRE(d.has_value());
    auto ok = healthy(*d);
    REQUIRE(ok.has_value());
    CHECK(*ok);
    auto s = Stmt::prepare(*d, "SELECT count(*) FROM events WHERE id = ?1");
    REQUIRE(s.has_value());
    std::size_t missing = 0;
    for (const proto::EventId id : ids) {
      s->reset();
      s->bind(1, id);
      auto row = s->step();
      if (!row || !*row || s->i64(0) != 1) ++missing;
      highest = std::max(highest, id);
    }
    CHECK(missing == 0);
  }
  StoreCore store(store_config(saves));
  proto::DbRequest open;
  open.seq = 1;
  open.op = db::Open{k, 60000 + 10 * kBatch * kBatches, 1};
  const proto::DbResult r = store.handle(open);
  const auto* o = std::get_if<db::Opened>(&r.result);
  REQUIRE(o != nullptr);
  CHECK(o->next_event_id > highest);
}
