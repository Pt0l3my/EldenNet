#pragma once
// A small reader of Steam's KeyValues text (`libraryfolders.vdf`, `appmanifest_<id>.acf`; ARCH §4.2), our own: quoted
// or bare tokens, `{ }` objects, `//` comments, the escapes `\\ \" \n \t`, and a `[$…]` condition after a value, which
// it skips. Keys compare without regard to ASCII case, as Steam's do. Bounded: at most 1 MiB of text, objects nested
// at most 16 deep and 65,536 nodes; anything else is refused with its line and a fixed problem, never a quote of the
// text.
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "en/proto/expected.hpp"

namespace en::launcher::vdf {

struct Node {
  std::string key;
  std::string value;  // a value's text (escapes resolved); empty for an object
  bool object = false;
  std::vector<Node> children;  // an object's pairs, in the text's order
};

struct Error {
  std::size_t line = 0;
  const char* problem = "";
};

inline constexpr std::size_t kMaxText = 1u << 20;
inline constexpr std::size_t kMaxDepth = 16;
inline constexpr std::size_t kMaxNodes = 65536;

// The text's top-level pairs, as the children of an unnamed object.
[[nodiscard]] proto::Expected<Node, Error> parse(std::string_view text);

// The first child of `obj` named `key` (ASCII case ignored), or null.
[[nodiscard]] const Node* find(const Node& obj, std::string_view key) noexcept;

}  // namespace en::launcher::vdf
