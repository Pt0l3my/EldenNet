#pragma once
// The reaction cue's four strings (MEM §12.13; block 7's template holds them word for word, `{short}` there being
// `{{ cue.short }}`) and W5's and W7's cue clauses (MEM §12.10): each string with every short at most 35 tokens, and
// with the since-line's 55 at most 90; no imperative to speak ("tell", "mention", "say", "ask", "remind", "must",
// "should"), no capitalised word after the first but "Tarnished", and "the Tarnished" only as the teller in the
// Tarnished's-part string.
#include <array>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace en::core {

// In CueWhy's order: Own, Told, LocalHere, LocalHome.
inline constexpr std::array<std::string_view, 4> kCueStrings = {
    "On your mind: {short}, for it touches your own affairs.",
    "On your mind: {short}, because of what the Tarnished told you of it.",
    "On your mind: {short}, for it is the talk of these parts.",
    "On your mind: {short}, for it is the talk of your home country.",
};

// Each finding, naming its clause ("W5: ...", "W7: ..."), never a short's text.
[[nodiscard]] std::vector<std::string> lint_cue_strings(std::span<const std::string_view, 4> strings,
                                                        std::span<const std::string> shorts);

}  // namespace en::core
