#pragma once
// The Content packs page's projection (PAGES §4.6.9; roadmap S11.4.a): what content EldenNet runs, as en-core's content
// layer knows it (content.hpp; content_view.hpp there builds it), in plain data that en-overlay's space publishes (it
// links no en-core, ARCH §2.3). The DLL keeps one on T6 and records each load into it.
//
// Projections only (PAGES §3, SP1): counts, the ids the player's own disable.toml turned off, paths relative to the
// overrides folder (PV5: never a drive, a user's folder or a folder above it), lines, and the content layer's own fixed
// words, which name a key's path and never a value of the file. Never a file's text. Knowledge and event rules are
// told as included, never counted (SP5).
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace en::proto {

inline constexpr std::size_t kContentReloadsMax = 20;    // the newest loads kept, a session's
inline constexpr std::size_t kContentProblemsMax = 64;   // a file's problems kept (the content layer lists 64 at most)
inline constexpr std::size_t kContentPathMaxBytes = 240;  // a path shown

// A file's state at the last load (Refused: it holds an error; a player's layer is refused alone, any other file
// refuses the whole load, so the content loaded before stays in use; Waiting: not loaded, since another file refused
// the load; LeftOut: never read, by the shell's own limits: too large, a link, too deep, or past the count).
enum class OverrideState : std::uint8_t { Loaded, Refused, Waiting, LeftOut };

struct ContentProblem {
  std::uint32_t line = 0;  // 1-based; 0 for the whole file
  bool error = false;      // false: a warning, and the file still loads
  std::string words;       // the content layer's fixed words and a key's path in them; never a value of the file

  friend bool operator==(const ContentProblem&, const ContentProblem&) = default;
};

struct OverrideFile {
  std::string path;  // relative to the overrides folder, '/'-separated; empty: the content as a whole
  OverrideState state = OverrideState::Loaded;
  std::vector<ContentProblem> problems;  // at most kContentProblemsMax, in the load's order

  friend bool operator==(const OverrideFile&, const OverrideFile&) = default;
};

// What a load read: everything at the start (the shipped content and the overrides folder), or the overrides again
// after a file of the player's own was written (the profile editor's).
enum class ReloadLayer : std::uint8_t { All, Overrides };

struct ContentReload {
  std::uint64_t at_ms = 0;  // since T1's start
  ReloadLayer layer = ReloadLayer::All;
  bool ok = false;
  bool earlier = false;      // refused, with the content of an earlier load still in use
  std::uint32_t errors = 0;  // a refused load's problems
  std::uint32_t warnings = 0;

  friend bool operator==(const ContentReload&, const ContentReload&) = default;
};

struct ContentView {
  std::uint64_t revision = 0;  // changes whenever anything below does
  bool loaded = false;         // a load has succeeded this session: the content below is in use
  // The shipped content: the cast EldenNet voices in this realm (the shipped index's personas, test personas apart, and
  // its cards: v0.2 ships none), and whether it holds knowledge and event rules (told as included, never counted).
  std::uint32_t personas = 0;
  std::uint32_t cards = 0;
  bool knowledge = false;
  bool rules = false;
  // The overrides folder's files at the last load, and those the shell left out when it read the folder, by path.
  std::vector<OverrideFile> overrides;
  std::vector<OverrideFile> left_out;
  // What disable.toml turns off in the content in use: personas and cards by id, in its order; knowledge entries,
  // triggers and rules by count. v0.2's content layer turns off personas alone (the rest is warned "not read").
  std::vector<std::string> off_personas;
  std::vector<std::string> off_cards;
  std::uint32_t off_knowledge = 0;
  std::uint32_t off_triggers = 0;
  std::uint32_t off_rules = 0;
  std::vector<ContentReload> reloads;  // newest first, at most kContentReloadsMax

  friend bool operator==(const ContentView&, const ContentView&) = default;
};

// Whether a path may be shown on the page (PV5): relative and '/'-separated, within kContentPathMaxBytes, printable,
// with no drive (a colon), no backslash, no leading '/' or '~', and no "." or ".." part, so never above the folder.
[[nodiscard]] inline bool shown_path(std::string_view p) noexcept {
  if (p.empty() || p.size() > kContentPathMaxBytes || p.front() == '/' || p.front() == '~' || p.back() == '/') {
    return false;
  }
  if (std::any_of(p.begin(), p.end(), [](char c) {
        const auto u = static_cast<unsigned char>(c);
        return u < 0x20 || u == 0x7F || c == '\\' || c == ':';
      })) {
    return false;
  }
  std::size_t start = 0;
  while (start <= p.size()) {
    const std::size_t end = std::min(p.find('/', start), p.size());
    const std::string_view part = p.substr(start, end - start);
    if (part.empty() || part == "." || part == "..") return false;
    start = end + 1;
  }
  return true;
}

}  // namespace en::proto
