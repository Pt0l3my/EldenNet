// The scrubbed logger (dll.hpp; ARCH §1.2's T11): any thread pushes a line onto a ring of fixed lines and never waits;
// T11 writes them to logs\eldennet.log as `ELDENNET <event> key=value ... t=<ms>`, rotating it past 1 MiB (one older
// file kept). Lines made before the logger starts (T1's Pin and Gate) wait in the ring, so a second copy, which never
// starts it, never writes to the shared log (ARCH §3.2's Safety). When the ring is full a line is dropped and counted.
#include <algorithm>
#include <cinttypes>
#include <cstdio>

#include "dll.hpp"
#include "en/proto/ffi_guard.hpp"

namespace eldennet {

namespace {

constexpr std::uint64_t kRotateBytes = 1u << 20;

// Made on first use by a constant-initialised pointer and never freed (R14).
constinit std::atomic<en::shell::LogRing*> g_ring{nullptr};
// The lines T11 wrote, for T6's logs stream (EldenNet's pages, System › Logs): T11 the producer, T6 the consumer; a
// line that finds it full is not offered (the page's ring is 500 rows; T6 drains this one every wake).
using TailRing = en::shell::SpscRing<en::shell::LogEntry, 256>;
constinit std::atomic<TailRing*> g_tail{nullptr};
// settings.toml's edits from the pages, posted by T6 and written here, off the core thread (D14): the newest batch
// only, each holding every edit of the session so far.
constinit std::atomic<std::vector<en::shell::SettingsEdit>*> g_edits{nullptr};
constinit wchar_t g_settings[MAX_PATH] = {};
// The AI pages' changes to providers.toml and secrets.toml, posted by T6 (a list pushed without a lock) and made here
// in the order posted.
struct AiWriteNode {
  AiWriteNode* next = nullptr;
  AiWrite write;
};
constinit std::atomic<AiWriteNode*> g_ai_writes{nullptr};
// The profile editor's writes (profiles.cpp): T6 the producer, T11 the consumer, eight at most waiting (C17); and what T11
// made of each, back to T6.
using ProfileRing = en::shell::SpscRing<ProfileWrite, 8>;
using ProfileDoneRing = en::shell::SpscRing<ProfileWritten, 8>;
constinit std::atomic<ProfileRing*> g_profile_writes{nullptr};
constinit std::atomic<ProfileDoneRing*> g_profile_done{nullptr};
constinit wchar_t g_ai_folder[MAX_PATH] = {};
constinit bool g_probe_build = false;
constinit std::atomic<int> g_level{static_cast<int>(en::shell::LogLevel::Info)};
constinit std::atomic<bool> g_started{false};
constinit wchar_t g_file[MAX_PATH] = {};
constinit wchar_t g_older[MAX_PATH] = {};

en::shell::LogRing* ring() noexcept {
  en::shell::LogRing* r = g_ring.load(std::memory_order_acquire);
  if (r != nullptr) return r;
  auto* made = new (std::nothrow) en::shell::LogRing();
  if (made == nullptr) return nullptr;
  if (!g_ring.compare_exchange_strong(r, made, std::memory_order_acq_rel)) {
    delete made;  // another thread made it first
    return r;
  }
  return made;
}

TailRing* tail() noexcept {
  TailRing* t = g_tail.load(std::memory_order_acquire);
  if (t != nullptr) return t;
  auto* made = new (std::nothrow) TailRing();
  if (made == nullptr) return nullptr;
  if (!g_tail.compare_exchange_strong(t, made, std::memory_order_acq_rel)) {
    delete made;
    return t;
  }
  return made;
}

// One batch of the pages' edits written to settings.toml: read again, edited (en_shell's edit_settings, which never
// writes over a file the parse refuses), written beside it and renamed over it.
void write_settings(const std::vector<en::shell::SettingsEdit>& edits) {
  const std::wstring file(g_settings);
  if (file.empty()) return;
  const std::optional<std::string> text = read_text(file, en::shell::kMaxSettingsBytes);
  const en::shell::EditedSettings edited = en::shell::edit_settings(text.value_or(""), edits, g_probe_build);
  bool ok = edited.ok;
  if (ok) {
    const std::size_t cut = file.find_last_of(L'\\');
    if (cut != std::wstring::npos) (void)make_dirs(file.substr(0, cut));
    ok = write_text(file, edited.text);
  }
  log(en::shell::Line("settings_write").flag("ok", ok).num("keys", edits.size())
          .word("why", edited.ok ? (ok ? "none" : "write") : edited.why),
      ok ? en::shell::LogLevel::Info : en::shell::LogLevel::Warn);
}

// One of the AI pages' changes, made on the file as it is now (read again, en-llm's edit, written beside it and renamed
// over it); a file whose text fails its checks is never written over. secrets.toml gets this user's access alone.
void write_ai(const AiWrite& w) {
  const std::wstring folder(g_ai_folder);
  if (folder.empty()) return;
  (void)make_dirs(folder);
  constexpr std::size_t kMax = 64u << 10;
  if (w.file == AiWrite::File::Providers) {
    const std::wstring file = folder + L"\\providers.toml";
    const std::optional<std::string> text = read_text(file, kMax);
    const en::llm::EditedText edited = en::llm::edit_providers(text.value_or(""), w.edit);
    const bool ok = edited.ok && write_text(file, edited.text);
    log(en::shell::Line("ai_write").word("file", "providers").flag("ok", ok)
            .word("why", edited.ok ? (ok ? "none" : "write") : edited.why),
        ok ? en::shell::LogLevel::Info : en::shell::LogLevel::Warn);
    return;
  }
  const std::wstring file = folder + L"\\secrets.toml";
  std::optional<std::string> read = read_text(file, kMax);
  const en::llm::Secret held(read ? std::move(*read) : std::string());
  const en::llm::EditedSecrets edited = en::llm::edit_secrets(held.reveal(), w.name, w.key ? &*w.key : nullptr);
  const bool ok = edited.ok && write_private(file, edited.text.reveal());
  log(en::shell::Line("ai_write").word("file", "secrets").flag("ok", ok)
          .word("why", edited.ok ? (ok ? "none" : "write") : edited.why),
      ok ? en::shell::LogLevel::Info : en::shell::LogLevel::Warn);
}

template <class R>
R* ring_of(std::atomic<R*>& slot) noexcept {
  R* r = slot.load(std::memory_order_acquire);
  if (r != nullptr) return r;
  auto* made = new (std::nothrow) R();
  if (made == nullptr) return nullptr;
  if (!slot.compare_exchange_strong(r, made, std::memory_order_acq_rel)) {
    delete made;
    return r;
  }
  return made;
}

// A layer's path, checked again here: layers/persona-<slug>.toml under the overrides folder, nothing else.
bool layer_rel(std::string_view rel) {
  constexpr std::string_view prefix = "layers/persona-";
  constexpr std::string_view suffix = ".toml";
  if (rel.size() <= prefix.size() + suffix.size() || rel.size() > 64 || rel.substr(0, prefix.size()) != prefix ||
      rel.substr(rel.size() - suffix.size()) != suffix) {
    return false;
  }
  const std::string_view id = rel.substr(prefix.size(), rel.size() - prefix.size() - suffix.size());
  return std::all_of(id.begin(), id.end(), [](char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_'; });
}

// One of the profile editor's changes, made on the file as it is now (NPC §4.7's save path). The previous version stays
// as <file>.bak; a removal leaves the file as the .bak; a read of the .bak gives its text, at most 8 KB.
ProfileWritten write_profile(const ProfileWrite& w) {
  ProfileWritten out;
  out.token = w.token;
  if (w.folder.empty() || !layer_rel(w.rel)) return out;
  std::wstring rel(w.rel.begin(), w.rel.end());  // ASCII: checked above
  for (wchar_t& c : rel) c = c == L'/' ? L'\\' : c;
  const std::wstring file = w.folder + L"\\" + rel;
  const std::wstring bak = file + L".bak";
  switch (w.op) {
    case ProfileWrite::Op::Write: {
      const std::size_t cut = file.find_last_of(L'\\');
      (void)make_dirs(file.substr(0, cut));
      // The previous version, for restore: read and written again (no import of the DLL's own for a copy).
      if (std::optional<std::string> previous = read_text(file, 8u << 10)) (void)write_text(bak, *previous);
      out.ok = write_text(file, w.text);
      break;
    }
    case ProfileWrite::Op::Remove:
      out.ok = MoveFileExW(file.c_str(), bak.c_str(), MOVEFILE_REPLACE_EXISTING) != 0;
      break;
    case ProfileWrite::Op::ReadBak:
      if (std::optional<std::string> text = read_text(bak, 8u << 10)) {
        out.text = std::move(*text);
        out.ok = true;
      }
      break;
  }
  log(en::shell::Line("profile_file").word("op", w.op == ProfileWrite::Op::Write    ? "write"
                                                 : w.op == ProfileWrite::Op::Remove ? "remove"
                                                                                    : "read_bak")
          .flag("ok", out.ok).num("bytes", w.op == ProfileWrite::Op::ReadBak ? out.text.size() : w.text.size()),
      out.ok ? en::shell::LogLevel::Info : en::shell::LogLevel::Warn);
  return out;
}

HANDLE open_log() noexcept {
  HANDLE f = CreateFileW(g_file, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr, OPEN_ALWAYS,
                         FILE_ATTRIBUTE_NORMAL, nullptr);
  return f;
}

DWORD WINAPI writer_main(void*) noexcept {
  return en::proto::ffi_guard(en::proto::Component::DllMain, DWORD{1}, []() -> DWORD {
    HANDLE f = open_log();
    LARGE_INTEGER size{};
    std::uint64_t written = f != INVALID_HANDLE_VALUE && GetFileSizeEx(f, &size) ? static_cast<std::uint64_t>(size.QuadPart) : 0;
    en::shell::LogEntry e;
    char out[en::shell::kLineBytes + 64];
    for (;;) {
      bool any = false;
      while (en::shell::LogRing* r = ring()) {
        if (!r->try_pop(e)) break;
        any = true;
        const int n = std::snprintf(out, sizeof out, "ELDENNET %.*s t=%" PRIu64 "\r\n", static_cast<int>(e.len),
                                    e.text.data(), e.at_ms);
        if (n <= 0) continue;
        if (TailRing* t = tail()) (void)t->try_push(e);  // the pages' copy, written or not
        if (f == INVALID_HANDLE_VALUE) continue;
        DWORD put = 0;
        WriteFile(f, out, static_cast<DWORD>(n), &put, nullptr);
        written += put;
        if (written > kRotateBytes) {
          CloseHandle(f);
          MoveFileExW(g_file, g_older, MOVEFILE_REPLACE_EXISTING);
          f = open_log();
          written = 0;
        }
      }
      if (any && f != INVALID_HANDLE_VALUE) FlushFileBuffers(f);
      if (std::vector<en::shell::SettingsEdit>* edits = g_edits.exchange(nullptr, std::memory_order_acq_rel)) {
        write_settings(*edits);
        delete edits;
      }
      if (AiWriteNode* list = g_ai_writes.exchange(nullptr, std::memory_order_acq_rel)) {
        AiWriteNode* ordered = nullptr;  // pushed newest first: turned round, oldest first
        while (list != nullptr) {
          AiWriteNode* next = list->next;
          list->next = ordered;
          ordered = list;
          list = next;
        }
        while (ordered != nullptr) {
          AiWriteNode* next = ordered->next;
          write_ai(ordered->write);
          delete ordered;
          ordered = next;
        }
      }
      if (ProfileRing* q = g_profile_writes.load(std::memory_order_acquire)) {
        ProfileWrite w;
        bool made = false;
        while (q->try_pop(w)) {
          ProfileWritten done = write_profile(w);
          if (ProfileDoneRing* back = ring_of(g_profile_done)) made = back->try_push(done) || made;
        }
        if (made) {
          if (Shell* sh = shell(); sh != nullptr && sh->wake != nullptr) SetEvent(sh->wake);  // T6 answers at once
        }
      }
      Sleep(50);
    }
  });
}

}  // namespace

void log(const en::shell::Line& line, en::shell::LogLevel level) noexcept {
  if (static_cast<int>(level) > g_level.load(std::memory_order_relaxed)) return;
  if (en::shell::LogRing* r = ring()) (void)r->try_push(line.text(), since_ms());
}

void set_log_level(en::shell::LogLevel level) noexcept { g_level.store(static_cast<int>(level), std::memory_order_relaxed); }

bool take_log_tail(en::shell::LogEntry& out) noexcept {
  TailRing* t = g_tail.load(std::memory_order_acquire);
  return t != nullptr && t->try_pop(out);
}

void set_settings_file(const std::wstring& path, bool probe_build) noexcept {
  if (path.size() >= MAX_PATH) return;
  path.copy(g_settings, path.size());
  g_probe_build = probe_build;
}

void post_settings_write(std::vector<en::shell::SettingsEdit> edits) noexcept {
  en::proto::ffi_guard(en::proto::Component::DllMain, [&] {
    auto* batch = new std::vector<en::shell::SettingsEdit>(std::move(edits));
    delete g_edits.exchange(batch, std::memory_order_acq_rel);  // an older batch not yet written: this one holds it all
  });
}

void set_ai_folder(const std::wstring& config) noexcept {
  if (config.size() >= MAX_PATH) return;
  config.copy(g_ai_folder, config.size());
}

void post_ai_write(AiWrite write) noexcept {
  en::proto::ffi_guard(en::proto::Component::DllMain, [&] {
    auto* node = new AiWriteNode();
    node->write = std::move(write);
    node->next = g_ai_writes.load(std::memory_order_relaxed);
    while (!g_ai_writes.compare_exchange_weak(node->next, node, std::memory_order_release, std::memory_order_relaxed)) {
    }
  });
}

bool post_profile_write(ProfileWrite& write) noexcept {
  return en::proto::ffi_guard(en::proto::Component::DllMain, false, [&] {
    ProfileRing* q = ring_of(g_profile_writes);
    return q != nullptr && q->try_push(write);
  });
}

bool take_profile_written(ProfileWritten& out) noexcept {
  ProfileDoneRing* back = g_profile_done.load(std::memory_order_acquire);
  return back != nullptr && back->try_pop(out);
}

std::uint64_t log_dropped() noexcept {
  en::shell::LogRing* r = g_ring.load(std::memory_order_acquire);
  return r == nullptr ? 0 : r->dropped();
}

bool start_logger(const std::wstring& folder) noexcept {
  return en::proto::ffi_guard(en::proto::Component::DllMain, false, [&] {
    if (g_started.exchange(true)) return true;
    const std::wstring file = folder + L"\\eldennet.log";
    const std::wstring older = folder + L"\\eldennet.1.log";
    if (folder.empty() || file.size() >= MAX_PATH || older.size() >= MAX_PATH || !make_dirs(folder)) return false;
    file.copy(g_file, file.size());
    older.copy(g_older, older.size());
    HANDLE t = CreateThread(nullptr, 0, &writer_main, nullptr, 0, nullptr);
    if (t == nullptr) return false;
    CloseHandle(t);
    return true;
  });
}

}  // namespace eldennet
