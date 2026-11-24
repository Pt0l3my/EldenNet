// System › Traces' data (traces.hpp): which calls are lines, the verdict's rule, the percentiles the tiles give, and
// the page's words.
#include "en/proto/traces.hpp"

#include <algorithm>

namespace en::proto {

std::optional<LineKind> line_of(CallPurpose p) noexcept {
  switch (p) {
    case CallPurpose::Reply: return LineKind::Reply;
    case CallPurpose::OwnInitiative: return LineKind::OwnInitiative;
    case CallPurpose::CallOut: return LineKind::CallOut;
    case CallPurpose::PreparedBatch:
    case CallPurpose::Memory:
    case CallPurpose::SelfTest:
    case CallPurpose::ModelList:
    case CallPurpose::ProfileDraft: break;
  }
  return std::nullopt;
}

LineVerdict verdict_of(LineKind kind, std::int64_t first_page_ms) noexcept {
  if (first_page_ms < 0 || kind == LineKind::OwnInitiative) return LineVerdict::None;
  const bool call_out = kind == LineKind::CallOut;
  if (first_page_ms <= (call_out ? kQuickCallOutMs : kQuickPageMs)) return LineVerdict::Quick;
  if (first_page_ms <= (call_out ? kAcceptableCallOutMs : kAcceptablePageMs)) return LineVerdict::Acceptable;
  return LineVerdict::Slow;
}

std::int64_t percentile(std::vector<std::uint32_t> samples, std::uint32_t permille) {
  if (samples.empty()) return -1;
  const std::uint64_t n = samples.size();
  const std::uint64_t rank = std::max<std::uint64_t>((n * std::min<std::uint32_t>(permille, 1000) + 999) / 1000, 1);
  const auto at = samples.begin() + static_cast<std::ptrdiff_t>(rank - 1);
  std::nth_element(samples.begin(), at, samples.end());
  return static_cast<std::int64_t>(*at);
}

const char* words(LineKind k) noexcept {
  switch (k) {
    case LineKind::Reply: return "reply";
    case LineKind::OwnInitiative: return "own-initiative";
    case LineKind::CallOut: return "call-out";
  }
  return "reply";
}

const char* words(LineVerdict v) noexcept {
  switch (v) {
    case LineVerdict::None: return "";
    case LineVerdict::Quick: return "quick";
    case LineVerdict::Acceptable: return "acceptable";
    case LineVerdict::Slow: return "slow";
  }
  return "";
}

const char* to_string(LineKind k) noexcept {
  switch (k) {
    case LineKind::Reply: return "reply";
    case LineKind::OwnInitiative: return "own_initiative";
    case LineKind::CallOut: return "call_out";
  }
  return "reply";
}

const char* to_string(LineVerdict v) noexcept {
  switch (v) {
    case LineVerdict::None: return "none";
    case LineVerdict::Quick: return "quick";
    case LineVerdict::Acceptable: return "acceptable";
    case LineVerdict::Slow: return "slow";
  }
  return "none";
}

}  // namespace en::proto
