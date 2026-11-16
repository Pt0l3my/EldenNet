#pragma once
// The caps on reading JSON (ARCH §10.2): a JSON text is at most 256 KB and nested at most 32 deep, and both are checked
// before nlohmann/json parses it, so that no text from a server, a file or a model can make the parser, or the code
// that walks its result, run out of memory or stack. Also the error every message's from_json throws (llm.hpp).
#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

namespace en::proto {

// A JSON form that is not the message's. The text names the path to the field and what is wrong with it
// ("LlmEvent.chat.attempts[1].error: must be one of None, ..."), and never quotes a value.
class ProtoError : public std::invalid_argument {
 public:
  using std::invalid_argument::invalid_argument;
};

inline constexpr std::size_t kMaxJsonBytes = 256u << 10;
inline constexpr std::size_t kMaxJsonDepth = 32;

struct JsonLimits {
  std::size_t max_bytes = kMaxJsonBytes;
  std::size_t max_depth = kMaxJsonDepth;
};

enum class JsonRefusal { None, TooLarge, TooDeep, Malformed };

// "larger than 256 KB", "nested deeper than 32", "not valid JSON"; empty for None.
std::string describe(JsonRefusal refusal, const JsonLimits& limits = {});

// Whether no array or object in `text` lies more than `max_depth` deep (a scalar is at depth 0, `[]` at 1). It counts
// without parsing, so brackets inside strings do not count, and it stops at the first bracket that goes too deep.
bool within_depth(std::string_view text, std::size_t max_depth) noexcept;

struct ParsedJson {
  JsonRefusal refusal = JsonRefusal::None;
  nlohmann::json value;  // the document when nothing was refused; discarded otherwise

  bool ok() const { return refusal == JsonRefusal::None; }
};

// Parses `text` once it has passed the limits, its size first and then its depth. What the text holds never throws.
ParsedJson parse_json(std::string_view text, const JsonLimits& limits = {});

// The message a JSON text holds: parsed within the limits, then read by the message's from_json. Throws ProtoError.
template <class T>
T read_json(std::string_view text, const JsonLimits& limits = {}) {
  ParsedJson parsed = parse_json(text, limits);
  if (!parsed.ok()) throw ProtoError("the text is " + describe(parsed.refusal, limits));
  return parsed.value.template get<T>();
}

}  // namespace en::proto
