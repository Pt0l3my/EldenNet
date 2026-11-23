// The first conversation's glue in the DLL (dll.hpp; WHY§364, part C): the content and its realm, read on T1 and
// loaded into the core on T6; and what each step of the core puts out for the conversation, on T6: its requests to
// en-llm's handle (ai.cpp), its cancels, its commits and its endings, in the log as words and numbers only.
//
// - The content: the shipped index and records are built into en-core (builtin_content), and the player's overrides
//   folder, %APPDATA%\EldenNet\overrides\ (DESIGN §11.4: the same paths, each file there replacing the shipped one
//   whole), is read by T1 on the main path, every .toml, .md and .j2 in it (records, indexes, prompts), each within
//   its cap. T6 loads them at its next wake. The content's warnings (en-core's, once it keeps them: a player's file past
//   a cap, and others) are logged by count and shown once as a notice that names the file, the key and the line, never
//   the text (NPC §4.7).
// - The realm (part B, identity.hpp): the game's. The tests' own switch, ELDENNET_TEST_REALM=eval in the environment,
//   is taken by a probe build alone (the tests' DLL; CI refuses EN_PROBE in a release build) and refused by the
//   release DLL, which logs that it was asked: a test persona never resolves in a player's game, whatever their
//   overrides hold (NPC §2.1).
// - Nothing here writes chat or model text anywhere: a commit is logged by its kind and its numbers (PV rules).
#include <string>
#include <string_view>
#include <vector>

#include "dll.hpp"
#include "en/core/content.hpp"
#include "en/proto/ffi_guard.hpp"

namespace eldennet {

using en::shell::Line;
using en::shell::LogLevel;

struct ContentRead {
  std::vector<en::core::ContentFile> overrides;
  bool folder = false;         // the overrides folder exists
  std::uint32_t refused = 0;   // its files left out: too large, too deep, unreadable, or past the count
  en::core::Realm realm = en::core::Realm::Game;
};

namespace {

constexpr std::size_t kMaxOverrideFiles = 256;
constexpr int kMaxDepth = 4;

std::string utf8_of(std::wstring_view w) {
  if (w.empty()) return {};
  const int n = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, w.data(), static_cast<int>(w.size()), nullptr, 0,
                                    nullptr, nullptr);
  if (n <= 0) return {};
  std::string out(static_cast<std::size_t>(n), '\0');
  WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, w.data(), static_cast<int>(w.size()), out.data(), n, nullptr,
                      nullptr);
  return out;
}

// The tests' switch: ELDENNET_TEST_REALM=eval in the environment (only a probe build takes it).
bool eval_asked() {
  wchar_t buf[16];
  const DWORD n = GetEnvironmentVariableW(L"ELDENNET_TEST_REALM", buf, 16);
  return n == 4 && std::wstring_view(buf, n) == L"eval";
}

const char* kind_word(en::core::Commit::Kind k) noexcept {
  using K = en::core::Commit::Kind;
  switch (k) {
    case K::Started: return "started";
    case K::PlayerSaid: return "player_said";
    case K::NpcSaid: return "npc_said";
    case K::NpcPage: return "npc_page";
    case K::CutOff: return "cut_off";
    case K::Ended: return "ended";
  }
  return "none";
}

// The pack's kinds of file: records and indexes (.toml), prompts and their parts (.md, .j2).
bool content_file(const std::wstring& name) {
  for (const std::wstring_view ext : {std::wstring_view(L".toml"), std::wstring_view(L".md"), std::wstring_view(L".j2")}) {
    if (name.size() > ext.size() && std::wstring_view(name).substr(name.size() - ext.size()) == ext) return true;
  }
  return false;
}

// The core's content warnings, where its version keeps them (en-core's content_warnings(), from the first
// conversation's joining of parts A and B); none where it does not.
template <class C>
std::vector<en::core::ContentError> warnings_of(const C& core) {
  if constexpr (requires { core.content_warnings(); }) {
    return std::vector<en::core::ContentError>(core.content_warnings().begin(), core.content_warnings().end());
  } else {
    return {};
  }
}

void read_folder(const std::wstring& root, const std::wstring& rel, int depth, ContentRead& out) {
  WIN32_FIND_DATAW f{};
  const std::wstring pattern = root + (rel.empty() ? L"" : L"\\" + rel) + L"\\*";
  HANDLE h = FindFirstFileExW(pattern.c_str(), FindExInfoBasic, &f, FindExSearchNameMatch, nullptr, 0);
  if (h == INVALID_HANDLE_VALUE) return;
  do {
    const std::wstring name = f.cFileName;
    if (name == L"." || name == L"..") continue;
    const std::wstring path = rel.empty() ? name : rel + L"\\" + name;
    if ((f.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
      ++out.refused;  // a link: never followed out of the folder
    } else if ((f.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
      if (depth < kMaxDepth) {
        read_folder(root, path, depth + 1, out);
      } else {
        ++out.refused;
      }
    } else if (content_file(name)) {
      if (out.overrides.size() >= kMaxOverrideFiles) {
        ++out.refused;
        continue;
      }
      std::optional<std::string> text = read_text(root + L"\\" + path, en::core::kContentFileMaxBytes);
      std::string key = utf8_of(path);
      for (char& c : key) c = c == '\\' ? '/' : c;
      if (!text || key.empty()) {
        ++out.refused;
        continue;
      }
      out.overrides.push_back(
          en::core::ContentFile{std::move(key), std::move(*text), en::core::ContentOrigin::Override});
    }
  } while (FindNextFileW(h, &f));
  FindClose(h);
}

}  // namespace

void read_content(Shell& s) noexcept {
  en::proto::ffi_guard(en::proto::Component::DllMain, [&] {
    auto* c = new ContentRead();  // handed to T6, which keeps it for the process (R14)
    if (!s.paths.data.empty()) {
      const std::wstring root = s.paths.data + L"\\overrides";
      const DWORD attributes = GetFileAttributesW(root.c_str());
      c->folder = attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
      if (c->folder) read_folder(root, L"", 0, *c);
    }
    const bool asked = eval_asked();
    if (asked && kProbeBuild) c->realm = en::core::Realm::Eval;
    if (asked) {
      log(Line("realm").word("asked", "eval").flag("accepted", kProbeBuild).flag("probe_build", kProbeBuild),
          kProbeBuild ? LogLevel::Info : LogLevel::Warn);
    }
    log(Line("content").flag("overrides", c->folder).num("files", c->overrides.size()).num("refused", c->refused)
            .word("realm", c->realm == en::core::Realm::Eval ? "eval" : "game"));
    s.content.store(c, std::memory_order_release);
    SetEvent(s.wake);
  });
}

bool content_tick(Shell& s, en::core::Core& core) noexcept {
  return en::proto::ffi_guard(en::proto::Component::DllMain, false, [&] {
    ContentRead* c = s.content.exchange(nullptr, std::memory_order_acq_rel);
    if (c == nullptr) return false;
    std::vector<en::core::ContentFile> files = en::core::builtin_content();
    for (const en::core::ContentFile& f : c->overrides) files.push_back(f);
    const std::vector<en::core::ContentError> errors = core.load_content(files, c->realm);
    const std::vector<en::core::ContentError> warnings = warnings_of(core);
    // The errors by count alone: their words name the file's keys, and the log takes words of its own shape only.
    log(Line("content_load").flag("ok", errors.empty()).num("files", files.size()).num("errors", errors.size())
            .num("personas", core.directory().size()).num("bound", core.directory().index().rows())
            .num("warnings", warnings.size()).num("disabled", core.directory().pack().disabled_personas.size()),
        errors.empty() && warnings.empty() ? LogLevel::Info : LogLevel::Warn);
    // Shown to the player once (NPC §4.7): the first warning's file, key and line in fixed words, and how many more.
    // Draft words, for review.
    const std::vector<en::core::ContentError>& shown = !errors.empty() ? errors : warnings;
    if (!shown.empty()) {
      std::string text = std::string(!errors.empty() ? "EldenNet could not load a content file: "
                                                     : "EldenNet content warning: ") +
                         en::core::describe(shown.front());
      if (shown.size() > 1) text += " (and " + std::to_string(shown.size() - 1) + " more)";
      s.content_notice = std::move(text);
    }
    c->overrides.clear();
    return true;
  });
}

void conversation_out(Shell& s, en::core::Outputs& out) noexcept {
  en::proto::ffi_guard(en::proto::Component::DllMain, [&] {
    for (const en::proto::RequestId id : out.cancels) ai_cancel(s, id);
    for (en::proto::LlmRequest& r : out.requests) (void)ai_submit(s, std::move(r));
    for (const en::core::Commit& c : out.commits) {
      // A commit by its kind and numbers: its text is chat, never for the log (PV rules).
      Line l("commit");
      l.word("kind", kind_word(c.kind)).num("conv", c.conv);
      if (c.kind == en::core::Commit::Kind::NpcSaid || c.kind == en::core::Commit::Kind::NpcPage ||
          c.kind == en::core::Commit::Kind::CutOff) {
        l.num("reply", c.reply).num("page", c.page);
      }
      if (c.kind == en::core::Commit::Kind::PlayerSaid || c.kind == en::core::Commit::Kind::NpcSaid ||
          c.kind == en::core::Commit::Kind::NpcPage) {
        l.num("chars", c.text.size());
      }
      if (c.kind == en::core::Commit::Kind::Ended) {
        l.word("reason", en::core::to_string(c.reason)).num("player_lines", c.player_lines);
        l.num("committed", c.committed);
      }
      log(l);
    }
    if (out.ended) log(Line("conversation").word("ended", en::core::to_string(*out.ended)));
  });
}

}  // namespace eldennet
