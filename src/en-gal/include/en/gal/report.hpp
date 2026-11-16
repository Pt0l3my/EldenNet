#pragma once
// en-gal's numbers-only reports (the derivation round, O89; the devkit's safe limits, O75): the writer every report
// that leaves the owner's PC is written through, and the lint that checks one (`eldennet-engine lint --report`).
//
// A report is one JSON object. Its first two keys name its kind and schema; every other key comes from the kind's
// closed vocabulary, and each key holds one type of value: a whole number (unsigned or signed), a boolean, a word from
// a fixed list, a label (lower-case letters, digits and _, of the caller's own), a byte pattern of at most 64 bytes in
// canonical form, an object or an array of one type. There is no free string, so nothing read from a file (a name, a
// path, text) can be written, and no byte run longer than a pattern; that is the "numbers only by construction" of the
// plan, and the lint refuses anything else in a file, wherever it came from. Problems name where they are, never what
// was there. Nothing here knows the game.
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "en/gal/patterns.hpp"
#include "en/proto/expected.hpp"

namespace en::gal::report {

enum class Type : std::uint8_t { Object, Array, Unsigned, Signed, Boolean, Word, Label, Pattern };

// One key of a vocabulary: its value's type, an array's element type, and the words a Word (or an array of Words) may
// take.
struct Key {
  std::string_view name;
  Type type = Type::Unsigned;
  Type element = Type::Object;
  std::span<const std::string_view> words{};
};

struct Vocabulary {
  std::string_view kind;  // the value of the report's "report" key
  std::uint32_t schema = 1;
  std::span<const Key> keys;

  [[nodiscard]] const Key* find(std::string_view name) const noexcept;
};

inline constexpr std::size_t kMaxPatternBytes = 64;  // a pattern longer than this is a byte run, refused
inline constexpr std::size_t kMaxPatterns = 64;      // patterns in one report
inline constexpr std::size_t kMaxLabelBytes = 48;
inline constexpr std::size_t kMaxArrayItems = 4096;
inline constexpr std::size_t kMaxDepth = 16;  // objects and arrays nested
inline constexpr std::size_t kMaxReportBytes = 256u << 10;

// A label: [a-z][a-z0-9_]* of at most 48 bytes.
[[nodiscard]] bool is_label(std::string_view s) noexcept;

struct Problem {
  std::string where;    // "fd4.singletons[2].label"
  std::string problem;  // "is not a label"

  friend bool operator==(const Problem&, const Problem&) = default;
};

// "where: problem".
[[nodiscard]] std::string describe(const Problem& problem);

// Writes one report, pretty-printed, checking each key against the vocabulary and each value against its key's type.
// In an object every value takes its key; in an array, none (the array's key gives the element type). The first refusal
// is kept and every later call is ignored; finish() then answers it, and no text.
class Writer {
 public:
  explicit Writer(const Vocabulary& vocabulary);

  void begin_object(std::string_view key = {});
  void end_object();
  void begin_array(std::string_view key = {});
  void end_array();
  void number(std::string_view key, std::uint64_t value);
  void number(std::uint64_t value) { number({}, value); }
  void signed_number(std::string_view key, std::int64_t value);
  void boolean(std::string_view key, bool value);
  void word(std::string_view key, std::string_view word);
  void word(std::string_view w) { word({}, w); }
  void label(std::string_view key, std::string_view label);
  void label(std::string_view l) { label({}, l); }
  void pattern(std::string_view key, const BytePattern& pattern);

  // The text, once every object and array is closed; else the first refusal.
  [[nodiscard]] proto::Expected<std::string, Problem> finish();

 private:
  struct Level {
    bool array = false;
    const Key* key = nullptr;  // an array's key
    std::size_t items = 0;
    std::string path;
  };
  bool value_start(std::string_view key, Type type, const Key** found);
  void refuse(std::string where, std::string problem);
  [[nodiscard]] std::string where(std::string_view key) const;

  const Vocabulary& vocabulary_;
  std::string out_;
  std::vector<Level> levels_;
  std::size_t patterns_ = 0;
  bool done_ = false;
  bool refused_ = false;
  Problem refusal_;
};

// Checks a report's text against the vocabulary its "report" key names: its size, that it is JSON, its depth, every
// key, every value's type, and the counts. Every problem is listed, up to 64.
[[nodiscard]] proto::Expected<void, std::vector<Problem>> lint(std::string_view text,
                                                               std::span<const Vocabulary* const> vocabularies);

}  // namespace en::gal::report
