// libFuzzer target: en-proto's parse_json with its caps (ARCH §10.2), and read_json for the two messages that cross
// the boundary between the core and the net, LlmRequest and LlmEvent (ARCH §2.2). The first byte picks the caps: below
// 0x80 the built ones (256 KB, 32 deep), else a depth of (byte & 0x3F) and a size of 16 times the next byte, to reach
// both edges with small inputs; the rest is the JSON text. Besides any sanitizer report, a broken property is a
// finding:
// - a text past the size cap is refused as TooLarge, else one with a bracket past the depth cap as TooDeep, else it is
//   refused, as Malformed, exactly when nlohmann/json does not accept it;
// - what parses is never nested deeper than the cap, and what is refused holds no value;
// - read_json throws ProtoError and nothing else, and what it reads, written back as text, reads again to the same
//   JSON form (en-proto's round trip).
// An exception that escapes is a finding too, so this entry has no ffi_guard (R3 is for the engine's entries).
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string_view>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "en/proto/json.hpp"
#include "en/proto/llm.hpp"

namespace {

using en::proto::JsonLimits;
using en::proto::JsonRefusal;
using nlohmann::json;

// A broken property ends the run as a crash does, so that libFuzzer keeps the input.
void require(bool holds, const char* property) {
  if (holds) return;
  std::fprintf(stderr, "property broken: %s\n", property);
  std::abort();
}

// How deeply arrays and objects nest in a value, counted as within_depth counts brackets (a scalar 0, [] 1), without
// recursion.
std::size_t depth_of(const json& root) {
  std::size_t deepest = 0;
  std::vector<std::pair<const json*, std::size_t>> pending{{&root, 0}};
  while (!pending.empty()) {
    const auto [node, depth] = pending.back();
    pending.pop_back();
    if (!node->is_structured()) continue;
    deepest = std::max(deepest, depth + 1);
    for (const json& child : *node) pending.emplace_back(&child, depth + 1);
  }
  return deepest;
}

// read_json's round trip for one message type: a ProtoError means the text is not one, and anything else escapes.
template <class T>
void round_trip(std::string_view text) {
  T first{};
  try {
    first = en::proto::read_json<T>(text);
  } catch (const en::proto::ProtoError&) {
    return;
  }
  const json written = first;
  const T second = en::proto::read_json<T>(written.dump());
  require(json(second) == written, "what read_json reads, written back, reads again to the same");
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  if (size < 1) return 0;
  // SAFETY: libFuzzer hands over `size` readable bytes, which live until this call returns; the view reads them as
  // the characters they are, and ends where they end, so a read past the text's end is a sanitizer report.
  const std::string_view input(reinterpret_cast<const char*>(data), size);
  const auto caps = static_cast<std::uint8_t>(input[0]);
  JsonLimits limits;
  std::string_view text = input.substr(1);
  if (caps >= 0x80) {
    if (size < 2) return 0;
    limits.max_depth = caps & 0x3Fu;
    limits.max_bytes = std::size_t{16} * static_cast<std::uint8_t>(input[1]);
    text = input.substr(2);
  }

  const en::proto::ParsedJson parsed = en::proto::parse_json(text, limits);
  if (text.size() > limits.max_bytes) {
    require(parsed.refusal == JsonRefusal::TooLarge, "a text past the size cap is refused as TooLarge");
  } else if (!en::proto::within_depth(text, limits.max_depth)) {
    require(parsed.refusal == JsonRefusal::TooDeep, "a text past the depth cap is refused as TooDeep");
  } else {
    const bool accepted = json::accept(text);
    require(parsed.ok() == accepted, "a text within the caps parses exactly when nlohmann/json accepts it");
    require(accepted || parsed.refusal == JsonRefusal::Malformed, "a text it does not accept is Malformed");
  }
  if (parsed.ok()) {
    require(depth_of(parsed.value) <= limits.max_depth, "what parses is never nested deeper than the cap");
  } else {
    require(parsed.value.is_discarded(), "what is refused holds no value");
  }

  if (caps < 0x80) {  // the built caps, as every reader in the engine has them
    round_trip<en::proto::LlmRequest>(text);
    round_trip<en::proto::LlmEvent>(text);
  }
  return 0;
}
