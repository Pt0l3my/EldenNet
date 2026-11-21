// libFuzzer target: en-proto's JSON forms of the game messages (snapshot.hpp, lifecycle.hpp, command.hpp,
// render.hpp), as a replay reads a recording's records (ARCH §7.5). The first byte picks the message (its value modulo
// 4: WorldSnapshot, Lifecycle, GameCommand, RenderFeed); the rest is the JSON text, read within en-proto's caps.
// Besides any sanitizer report, a broken property is a finding:
// - read_json throws ProtoError and nothing else;
// - what it reads, written back, is valid JSON whose text writes without an error (no invalid UTF-8 reaches it), and
//   reads again to the same JSON form (the round trip; a NaN is null in both);
// - a container read never holds more than its capacity.
// An exception that escapes is a finding too, so this entry has no ffi_guard (R3 is for the engine's entries).
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

#include "en/proto/command.hpp"
#include "en/proto/json.hpp"
#include "en/proto/lifecycle.hpp"
#include "en/proto/render.hpp"
#include "en/proto/snapshot.hpp"

namespace {

using nlohmann::json;

void require(bool holds, const char* property) {
  if (holds) return;
  std::fprintf(stderr, "property broken: %s\n", property);
  std::abort();
}

void check_capacities(const en::proto::WorldSnapshot& s) {
  require(s.chars.size() <= s.chars.capacity(), "chars within their capacity");
  require(s.new_text.size() <= s.new_text.capacity(), "new_text within its capacity");
  require(s.new_params.size() <= s.new_params.capacity(), "new_params within their capacity");
  require(s.boss.bars.size() <= s.boss.bars.capacity(), "boss bars within their capacity");
  for (const auto& row : s.new_params) require(row.fields.size() <= row.fields.capacity(), "param fields");
  for (const auto& t : s.new_text) {
    if (t.text) require(en::proto::valid_utf8(t.text->view()), "text is UTF-8");
  }
}

void check_capacities(const en::proto::Lifecycle&) {}
void check_capacities(const en::proto::GameCommand& c) {
  if (const auto* watch = std::get_if<en::proto::GameCommand::SetWatchFlags>(&c.payload)) {
    if (watch->ids) require(watch->ids->size() <= en::proto::kMaxWatchFlags, "a watch list within its cap");
  }
}
void check_capacities(const en::proto::RenderFeed& f) {
  require(f.heads.size() <= f.heads.capacity(), "heads within their capacity");
}

template <class T>
void round_trip(std::string_view text) {
  T first{};
  try {
    first = en::proto::read_json<T>(text);
  } catch (const en::proto::ProtoError&) {
    return;
  }
  check_capacities(first);
  const json written = first;
  const std::string dumped = written.dump();  // throws on invalid UTF-8: a finding
  const T second = en::proto::read_json<T>(dumped);
  require(json(second) == written, "what read_json reads, written back, reads again to the same");
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  if (size < 1) return 0;
  // SAFETY: libFuzzer hands over `size` readable bytes, which live until this call returns; the view reads them as
  // the characters they are, and ends where they end.
  const std::string_view input(reinterpret_cast<const char*>(data), size);
  const std::string_view text = input.substr(1);
  switch (static_cast<std::uint8_t>(input[0]) % 4) {
    case 0: round_trip<en::proto::WorldSnapshot>(text); break;
    case 1: round_trip<en::proto::Lifecycle>(text); break;
    case 2: round_trip<en::proto::GameCommand>(text); break;
    default: round_trip<en::proto::RenderFeed>(text); break;
  }
  return 0;
}
