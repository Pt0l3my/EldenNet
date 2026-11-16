#pragma once
// en-gal's byte patterns (ARCH §6.2, §6.3): the grammar of patterns.toml's `bytes`, the multi-pattern matcher that
// finds every pattern in one pass over a section, patterns.toml's schema, and the rules a pattern's hits must pass
// before an item counts as found:
// - a pattern matches exactly `expect_matches` times (default 1; an idiom takes a range);
// - a function's hit maps to its containing function through the exception directory, sits at entry + anchor_min or
//   later (anchor_min ≥ 5: other mods hook prologues), in a function whose length lies within fn_len and whose entry
//   holds the prologue's bytes, if one is given, and not in a fragment of a function;
// - a static's hit holds a RIP-relative displacement at capture.at whose target lies in target_section and is aligned
//   to 8;
// - every hit of one pattern resolves to the same function or static.
// Nothing here knows the game: the patterns, their sections and their ids come from patterns.toml, and real patterns
// come only from our own derivation (O27, O48, O89). Matching runs over a ByteSource in chunks, offline over the file
// and in game over the mapped image through guarded copies, with the hits reported as RVAs either way.
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "en/gal/pe.hpp"
#include "en/gal/schema.hpp"
#include "en/proto/expected.hpp"

namespace en::gal {

// ---- The grammar: hex byte pairs, ?? for a wildcard byte, whitespace between them ignored ----

struct BytePattern {
  std::vector<std::uint8_t> value;  // the byte to match, 0 under a wildcard
  std::vector<std::uint8_t> mask;   // 0xFF for a fixed byte, 0x00 for a wildcard

  [[nodiscard]] std::size_t size() const noexcept { return value.size(); }
  [[nodiscard]] std::size_t fixed() const noexcept;  // how many bytes are not wildcards
  // Whether the pattern matches the first size() bytes of `at` (false when `at` is shorter).
  [[nodiscard]] bool matches(std::span<const std::byte> at) const noexcept;
  // The canonical text: upper-case pairs and ??, one space between.
  [[nodiscard]] std::string text() const;

  friend bool operator==(const BytePattern&, const BytePattern&) = default;
};

enum class GrammarProblem : std::uint8_t {
  Empty,         // no byte at all
  BadCharacter,  // a character that is neither a hex digit, '?' nor whitespace
  HalfByte,      // a hex digit without its pair
  HalfWildcard,  // a '?' without its pair, or mixed with a digit ("4?")
  Wildcard,      // a wildcard where only exact bytes are allowed (a prologue)
  TooLong,       // more bytes than the limit
  TooFewFixed,   // fewer fixed bytes than the limit: it would match too much
};

struct GrammarError {
  GrammarProblem problem = GrammarProblem::Empty;
  std::size_t at = 0;  // the offset of the character where it was found

  friend bool operator==(const GrammarError&, const GrammarError&) = default;
};

struct GrammarLimits {
  std::size_t max_bytes = 256;
  std::size_t min_fixed = 4;
  bool wildcards = true;
};

[[nodiscard]] const char* to_string(GrammarProblem problem) noexcept;
[[nodiscard]] std::string describe(const GrammarError& error);
[[nodiscard]] proto::Expected<BytePattern, GrammarError> parse_pattern(std::string_view text,
                                                                       const GrammarLimits& limits = {});

// ---- The matcher ----

// Finds every pattern in one pass over a stream of bytes (a section), delivered in chunks. Positions are the stream's
// own (RVAs, for a section). Chunks come in order; consecutive chunks of one run share at least overlap() bytes, so a
// match across their seam is found once. A chunk that starts past the last examined position leaves a gap whose
// positions are never examined and are counted in unscanned(). It keeps at most `max_hits` positions a pattern;
// counts go on past that. It allocates only when it is made.
class MultiMatcher {
 public:
  MultiMatcher(std::span<const BytePattern> patterns, std::size_t max_hits = 4096);

  [[nodiscard]] std::size_t longest() const noexcept { return longest_; }
  [[nodiscard]] std::size_t overlap() const noexcept { return longest_ > 0 ? longest_ - 1 : 0; }

  // Starts a stream whose first byte is at `start`.
  void begin(std::uint64_t start) noexcept;
  // Scans `chunk`, which starts at `at`. With `ends_run`, every position in it is examined, and what follows it is a
  // gap or the stream's end; otherwise its last overlap() positions wait for the next chunk.
  void feed(std::uint64_t at, std::span<const std::byte> chunk, bool ends_run) noexcept;
  // Ends the stream at `end`: positions never examined are counted in unscanned().
  void finish(std::uint64_t end) noexcept;

  [[nodiscard]] std::size_t patterns() const noexcept { return patterns_.size(); }
  [[nodiscard]] std::uint64_t count(std::size_t pattern) const noexcept;
  [[nodiscard]] std::span<const std::uint64_t> hits(std::size_t pattern) const noexcept;
  [[nodiscard]] std::uint64_t unscanned() const noexcept { return unscanned_; }

 private:
  struct Anchor {
    std::uint16_t key = 0;  // the two bytes at the anchor, the first in the low byte
    std::uint32_t pattern = 0;
    std::uint32_t offset = 0;  // the anchor's offset in the pattern
  };
  void check(std::uint32_t pattern, std::uint64_t start, std::uint64_t at, std::span<const std::byte> chunk) noexcept;

  std::vector<BytePattern> patterns_;
  std::vector<Anchor> pairs_;               // patterns anchored on two fixed bytes in a row, sorted by key
  std::vector<Anchor> singles_;             // patterns with no two fixed bytes in a row, on their first fixed byte
  std::vector<std::uint64_t> pair_bits_;    // 65,536 bits: a pair anchor has this key
  std::vector<std::uint64_t> single_bits_;  // 256 bits
  std::vector<std::uint64_t> counts_;
  std::vector<std::vector<std::uint64_t>> hits_;
  std::size_t max_hits_;
  std::size_t longest_ = 0;
  std::uint64_t next_ = 0;  // the first position not yet examined
  std::uint64_t unscanned_ = 0;
};

// ---- patterns.toml ----

enum class PatternKind : std::uint8_t { Function, Static, Idiom };

// A 32-bit displacement at `at` in the pattern, relative to the end of its instruction: target = hit + insn_end + disp.
struct Capture {
  std::uint32_t at = 0;
  std::uint32_t insn_end = 0;

  friend bool operator==(const Capture&, const Capture&) = default;
};

// One of an idiom's captures, with where its target must lie.
struct IdiomCapture {
  std::string name;
  Capture capture;
  std::string section;      // the section its target lies in
  std::uint32_t align = 1;  // its target's alignment
  bool function = false;    // its target starts a function of the exception directory

  friend bool operator==(const IdiomCapture&, const IdiomCapture&) = default;
};

struct Provenance {
  std::string method;  // always "own-derivation"
  std::vector<std::string> leads;
  std::string evidence;
  std::string date;

  friend bool operator==(const Provenance&, const Provenance&) = default;
};

struct PatternSpec {
  std::string id;
  PatternKind kind = PatternKind::Function;
  std::string section;  // where it is matched
  BytePattern bytes;
  std::uint32_t expect_min = 1;  // expect_matches: a count, or [lowest, highest]
  std::uint32_t expect_max = 1;
  // A function's:
  std::uint32_t anchor_min = 5;
  std::uint32_t fn_len_min = 0;
  std::uint32_t fn_len_max = 0;
  std::vector<std::uint8_t> prologue;  // exact bytes at the entry; empty for none
  // A static's:
  Capture capture;
  std::string target_section;
  std::string rtti;  // the class its instance must have, checked on the first tick; empty for none
  // An idiom's:
  std::vector<IdiomCapture> captures;
  std::string path_suffix;  // the FD4 idiom's: the path every site loads ends with this (ARCH §6.2 step 2)
  // Every pattern's:
  std::vector<std::string> builds;  // where it was confirmed
  Provenance provenance;
};

struct PatternSet {
  std::vector<PatternSpec> patterns;

  [[nodiscard]] const PatternSpec* find(std::string_view id) const noexcept;
};

inline constexpr std::uint32_t kMaxPatternHits = 4096;  // the most hits a pattern may expect, and the most kept
// An idiom's: every inlined accessor of every singleton is a hit (some thousands in a large game), so an idiom may
// expect, and the scan keeps, up to 65536.
inline constexpr std::uint32_t kMaxIdiomHits = 1u << 16;

// Reads patterns.toml (schema 1): every field of its kind and none of another's, the grammar, the captures within
// their patterns, ids unique, provenance "own-derivation", and each of `required_ids` present. `file` names the file
// in the errors.
[[nodiscard]] proto::Expected<PatternSet, SchemaErrors> parse_patterns(std::string_view text,
                                                                       std::span<const std::string_view> required_ids,
                                                                       std::string_view file = "patterns.toml");

// ---- The rules over an image ----

enum class HitProblem : std::uint8_t {
  NoSection,          // the image has no section of the pattern's name
  Count,              // it matched other than expect_matches times
  Unscanned,          // too few matches, where part of its section could not be read
  NoFunction,         // a hit lies in no function of the exception directory
  Fragment,           // a hit lies in a fragment of a function
  NearEntry,          // a hit starts before entry + anchor_min
  PastEnd,            // a hit runs past its function's end
  Length,             // the function's length is outside fn_len
  Prologue,           // the entry's bytes differ from the prologue
  Unreadable,         // bytes a check needs could not be read
  TargetSection,      // a capture's target lies outside its section
  TargetAlignment,    // a capture's target is not aligned
  TargetNotFunction,  // a capture's target does not start a function
  Ambiguous,          // the hits resolve to different functions or statics
};

[[nodiscard]] const char* to_string(HitProblem problem) noexcept;

// One idiom hit whose captures all passed, with their targets in the captures' order.
struct IdiomHit {
  std::uint32_t hit = 0;
  std::vector<std::uint32_t> targets;
};

struct PatternResult {
  std::string id;
  PatternKind kind = PatternKind::Function;
  std::uint64_t count = 0;
  std::vector<std::uint32_t> hits;    // RVAs, the first kMaxPatternHits (an idiom's, kMaxIdiomHits)
  std::optional<HitProblem> problem;  // why it did not resolve; empty when it did
  std::uint32_t problem_at = 0;       // the hit, or the RVA, where the problem was found
  std::uint32_t entry = 0;            // a function's entry RVA
  std::uint32_t length = 0;           // and its length
  std::uint32_t target = 0;           // a static's RVA
  std::vector<IdiomHit> idiom_hits;   // an idiom's hits that passed
  std::uint64_t idiom_rejected = 0;   // and how many did not

  [[nodiscard]] bool resolved() const noexcept { return !problem.has_value(); }
};

struct ScanOptions {
  std::uint32_t chunk_bytes = 1u << 20;  // each copy from the source (ARCH §6.2: 1 MiB)
};

struct ScanReport {
  std::vector<PatternResult> results;  // in the set's order
  std::uint64_t scanned = 0;           // bytes examined
  std::uint64_t unscanned = 0;         // bytes that could not be read, or were never examined
};

// Matches every pattern of the set over its section of the image, each section once, and applies the rules above.
[[nodiscard]] ScanReport scan_patterns(const ImageInfo& image, const ByteSource& source, const PatternSet& set,
                                       const ScanOptions& options = {});

}  // namespace en::gal
