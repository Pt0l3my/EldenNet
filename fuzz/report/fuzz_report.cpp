// libFuzzer target: en-gal's numbers-only writer and its lint (report.hpp), with the derivation's vocabulary and one of
// the target's own. The first byte picks the mode.
// - Even: the rest is a text for the lint. Besides any sanitizer report, a broken property is a finding: a text the
// lint
//   accepts is one JSON object whose every string is a word, a label or a canonical pattern of at most 64 bytes, with
//   no character a path needs, and whose every number is whole; a refusal lists problems.
// - Odd: the rest drives the writer, one operation per few bytes (a key from the vocabulary or bytes of the input as
//   a key, a value from the input); a broken property is a finding: what finish() gives lints clean, parses as JSON,
//   and holds no string the input offered that is not a word of the vocabulary or a label.
// An exception that escapes is a finding too, so this entry has no ffi_guard (R3 is for the engine's entries).
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <nlohmann/json.hpp>
#include <string>
#include <string_view>
#include <vector>

#include "en/gal/derive.hpp"
#include "en/gal/patterns.hpp"
#include "en/gal/report.hpp"

namespace {

using namespace en::gal;
using report::Key;
using report::Type;

void require(bool holds, const char* property) {
  if (holds) return;
  std::fprintf(stderr, "property broken: %s\n", property);
  std::abort();
}

constexpr std::array<std::string_view, 3> kWords = {"one", "two", ".three"};
constexpr std::array<Key, 9> kKeys = {{
    {"n", Type::Unsigned},
    {"s", Type::Signed},
    {"b", Type::Boolean},
    {"w", Type::Word, Type::Object, kWords},
    {"l", Type::Label},
    {"p", Type::Pattern},
    {"o", Type::Object},
    {"a", Type::Array, Type::Object},
    {"u", Type::Array, Type::Unsigned},
}};
constexpr report::Vocabulary kOwn{"fuzz", 3, kKeys};

// Whether a string a report holds could be a word, a label or a pattern: printable, short, nothing a path needs.
bool plausible(const std::string& s) {
  if (s.empty() || s.size() > 3 * report::kMaxPatternBytes) return false;
  for (const char c : s) {
    if (c < 0x20 || c >= 0x7F || c == '/' || c == '\\' || c == ':' || c == '"') return false;
  }
  return true;
}

void walk(const nlohmann::json& j) {
  if (j.is_string()) {
    require(plausible(j.get_ref<const std::string&>()), "every string a word, a label or a pattern");
  } else if (j.is_number()) {
    require(j.is_number_integer(), "every number whole");
  } else if (j.is_object() || j.is_array()) {
    for (const auto& item : j) walk(item);
  } else {
    require(j.is_boolean(), "no null and no binary");
  }
}

void lint_mode(std::string_view text) {
  const report::Vocabulary* known[] = {&derive::vocabulary(), &kOwn};
  const auto r = report::lint(text, known);
  if (!r) {
    require(!r.error().empty(), "a refusal lists its problems");
    return;
  }
  const auto doc = nlohmann::json::parse(text, nullptr, false);
  require(!doc.is_discarded() && doc.is_object(), "what the lint accepts is one JSON object");
  walk(doc);
}

class Input {
 public:
  Input(const std::uint8_t* data, std::size_t size) : data_(data), size_(size) {}
  bool more() const { return at_ < size_; }
  std::uint8_t byte() { return at_ < size_ ? data_[at_++] : 0; }
  std::string text(std::size_t max) {
    const std::size_t n = byte() % (max + 1);
    std::string s;
    for (std::size_t i = 0; i < n && more(); ++i) s += static_cast<char>(byte());
    return s;
  }
  std::uint64_t number() {
    std::uint64_t v = 0;
    const std::size_t n = byte() % 9;
    for (std::size_t i = 0; i < n; ++i) v = (v << 8) | byte();
    return v;
  }

 private:
  const std::uint8_t* data_;
  std::size_t size_;
  std::size_t at_ = 0;
};

void writer_mode(Input& in) {
  const bool own = (in.byte() & 1) != 0;
  const report::Vocabulary& vocab = own ? kOwn : derive::vocabulary();
  report::Writer w(vocab);
  std::vector<std::string> offered;  // strings from the input given as values
  int ops = 0;
  while (in.more() && ops++ < 256) {
    const std::uint8_t op = in.byte();
    std::string key;
    if ((op & 0x80) != 0) {
      key = in.text(16);
    } else if (!vocab.keys.empty() && (op & 0x40) == 0) {
      key = std::string(vocab.keys[in.byte() % vocab.keys.size()].name);
    }
    switch (op & 0x0F) {
      case 0: w.begin_object(key); break;
      case 1: w.end_object(); break;
      case 2: w.begin_array(key); break;
      case 3: w.end_array(); break;
      case 4: w.number(key, in.number()); break;
      case 5: w.signed_number(key, static_cast<std::int64_t>(in.number())); break;
      case 6: w.boolean(key, (in.byte() & 1) != 0); break;
      case 7: {
        std::string word = in.text(12);
        if ((in.byte() & 1) != 0 && !vocab.keys.empty()) {
          const Key& k = vocab.keys[in.byte() % vocab.keys.size()];
          if (!k.words.empty()) word = std::string(k.words[in.byte() % k.words.size()]);
        }
        offered.push_back(word);
        w.word(key, word);
        break;
      }
      case 8: {
        const std::string label = in.text(20);
        offered.push_back(label);
        w.label(key, label);
        break;
      }
      case 9: {
        BytePattern p;
        const std::size_t n = in.byte() % 80;
        for (std::size_t i = 0; i < n; ++i) {
          p.value.push_back(in.byte());
          p.mask.push_back((in.byte() & 3) == 0 ? 0 : 0xFF);
        }
        for (std::size_t i = 0; i < p.value.size(); ++i) {
          if (p.mask[i] == 0) p.value[i] = 0;
        }
        w.pattern(key, p);
        break;
      }
      default: break;
    }
  }
  const auto text = w.finish();
  if (!text) return;
  const report::Vocabulary* known[] = {&vocab};
  const auto linted = report::lint(*text, known);
  require(linted.has_value(), "what the writer gives lints clean");
  const auto doc = nlohmann::json::parse(*text, nullptr, false);
  require(!doc.is_discarded() && doc.is_object(), "what the writer gives is one JSON object");
  walk(doc);
  for (const std::string& s : offered) {
    if (s.empty() || report::is_label(s)) continue;
    bool word = false;
    for (const Key& k : vocab.keys) {
      for (const std::string_view w2 : k.words) word = word || w2 == s;
    }
    if (word) continue;
    require(text->find("\"" + s + "\"") == std::string::npos, "no string offered that is not a word or a label");
  }
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  if (size == 0) return 0;
  if ((data[0] & 1) == 0) {
    lint_mode(std::string_view(reinterpret_cast<const char*>(data + 1), size - 1));  // SAFETY: libFuzzer's input
  } else {
    Input in(data + 1, size - 1);
    writer_mode(in);
  }
  return 0;
}
