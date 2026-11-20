#pragma once
// The shipped rule files (content/eldenring/augur/*.toml), built into en-augur's tests and benchmark
// (cmake/embed_files.cmake), in file-name order.
#include <span>
#include <string_view>

namespace en::augur::test {

struct ShippedFile {
  std::string_view name;
  std::string_view text;
};

[[nodiscard]] std::span<const ShippedFile> shipped_texts() noexcept;

}  // namespace en::augur::test
