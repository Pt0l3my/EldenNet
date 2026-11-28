// W5's and W7's cue clauses (news_lint.hpp; MEM §12.10, §12.13).
#include "en/core/news_lint.hpp"

#include <cctype>
#include <cmath>

#include "../content/wording.hpp"  // kImperatives, W7's list

namespace en::core {

namespace {

std::size_t tokens(std::size_t bytes) { return static_cast<std::size_t>(std::ceil(bytes / 3.6 * 1.15)); }

std::vector<std::string> words_of(std::string_view s) {
  std::vector<std::string> out;
  std::string w;
  for (const char c : s) {
    if (std::isalpha(static_cast<unsigned char>(c)) != 0 || c == '\'') {
      w.push_back(c);
    } else if (!w.empty()) {
      out.push_back(std::move(w));
      w.clear();
    }
  }
  if (!w.empty()) out.push_back(std::move(w));
  return out;
}

}  // namespace

std::vector<std::string> lint_cue_strings(std::span<const std::string_view, 4> strings,
                                          std::span<const std::string> shorts) {
  std::vector<std::string> out;
  for (std::size_t i = 0; i < strings.size(); ++i) {
    const std::string_view s = strings[i];
    const std::string at = "cue string " + std::to_string(i + 1);
    const std::size_t mark = s.find("{short}");
    if (mark == std::string_view::npos) {
      out.push_back("W5: " + at + " names no {short}");
      continue;
    }
    for (const std::string& sh : shorts) {
      const std::size_t bytes = s.size() - 7 + sh.size();
      if (tokens(bytes) > 35) {
        out.push_back("W5: " + at + " passes 35 tokens with a short");
        break;
      }
      if (tokens(bytes) + 55 > 90) {
        out.push_back("W5: " + at + " and the since-line pass 90 tokens with a short");
        break;
      }
    }
    std::string fixed(s);
    fixed.erase(mark, 7);
    const std::vector<std::string> words = words_of(fixed);
    for (std::size_t w = 0; w < words.size(); ++w) {
      std::string lower = words[w];
      for (char& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
      for (const std::string_view imp : wording::kImperatives) {
        if (lower == imp) out.push_back("W7: " + at + " holds an imperative to speak");
      }
      if (w > 0 && std::isupper(static_cast<unsigned char>(words[w][0])) != 0 && words[w] != "Tarnished") {
        out.push_back("W7: " + at + " holds a capitalised word after its first");
      }
    }
    if (i != 1 && fixed.find("the Tarnished") != std::string::npos) {
      out.push_back("W7: " + at + " names the Tarnished, who is the teller only in the Tarnished's part");
    }
  }
  return out;
}

}  // namespace en::core
