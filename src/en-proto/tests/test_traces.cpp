// System › Traces' plain data and words (traces.hpp): which calls are lines, the verdict's rule at its bounds for each
// kind (C5's cloud targets for a reply, DESIGN §1.4's for a call-out, none for an own-initiative line), the
// nearest-rank percentiles the tiles give, C5's targets, and PAGES' words. All data is invented.
#include <doctest.h>

#include <cstdint>
#include <set>
#include <string>
#include <vector>

#include "en/proto/traces.hpp"

using namespace en::proto;

TEST_CASE("a line is a reply, an own-initiative line or a call-out reply; nothing else is") {
  CHECK(line_of(CallPurpose::Reply) == LineKind::Reply);
  CHECK(line_of(CallPurpose::OwnInitiative) == LineKind::OwnInitiative);
  CHECK(line_of(CallPurpose::CallOut) == LineKind::CallOut);
  for (const CallPurpose p : {CallPurpose::PreparedBatch, CallPurpose::Memory, CallPurpose::SelfTest,
                              CallPurpose::ModelList, CallPurpose::ProfileDraft}) {
    CHECK_FALSE(line_of(p).has_value());
  }
}

TEST_CASE("the verdict: quick within 1.6 s, acceptable within 3.0 s, slow beyond; a call-out by its own 1.5 and 2.5") {
  CHECK(verdict_of(LineKind::Reply, -1) == LineVerdict::None);  // no page showed
  CHECK(verdict_of(LineKind::Reply, 0) == LineVerdict::Quick);
  CHECK(verdict_of(LineKind::Reply, 1600) == LineVerdict::Quick);
  CHECK(verdict_of(LineKind::Reply, 1601) == LineVerdict::Acceptable);
  CHECK(verdict_of(LineKind::Reply, 3000) == LineVerdict::Acceptable);
  CHECK(verdict_of(LineKind::Reply, 3001) == LineVerdict::Slow);
  CHECK(verdict_of(LineKind::CallOut, 1500) == LineVerdict::Quick);
  CHECK(verdict_of(LineKind::CallOut, 1501) == LineVerdict::Acceptable);
  CHECK(verdict_of(LineKind::CallOut, 2500) == LineVerdict::Acceptable);
  CHECK(verdict_of(LineKind::CallOut, 2501) == LineVerdict::Slow);
  // Prepared, and shown once the Tarnished comes near: its first page is no measure of the model's.
  CHECK(verdict_of(LineKind::OwnInitiative, 900) == LineVerdict::None);
  CHECK(verdict_of(LineKind::OwnInitiative, 9000) == LineVerdict::None);
  // The rule is the self-test's (ARCH §11.7), C5's cloud targets.
  CHECK(kQuickPageMs == kCloudTargets.median_ms);
  CHECK(kAcceptablePageMs == kCloudTargets.p95_ms);
  CHECK(kLocalTargets.median_ms == 2500);
  CHECK(kLocalTargets.p95_ms == 4500);
}

TEST_CASE("the percentiles are nearest-rank: the median of one is itself, of two the lower, a 95th of 20 the 19th") {
  CHECK(percentile({}, 500) == -1);
  CHECK(percentile({1400}, 500) == 1400);
  CHECK(percentile({1400}, 950) == 1400);
  CHECK(percentile({2000, 1000}, 500) == 1000);
  CHECK(percentile({3000, 1000, 2000}, 500) == 2000);
  std::vector<std::uint32_t> twenty;
  for (std::uint32_t i = 20; i >= 1; --i) twenty.push_back(i * 100);  // 2000 down to 100, unsorted for it
  CHECK(percentile(twenty, 500) == 1000);  // rank 10
  CHECK(percentile(twenty, 950) == 1900);  // rank 19: the slowest one in twenty above it
  CHECK(percentile(twenty, 1000) == 2000);
  CHECK(percentile(twenty, 0) == 100);     // rank 1 at the least
  CHECK(percentile(twenty, 2000) == 2000); // past 1000 counts as 1000
  // The samples are its own copy: the caller's order is left as it was.
  CHECK(twenty.front() == 2000);
}

TEST_CASE("every kind and verdict has PAGES' words, and each its own name") {
  std::set<std::string> words_seen;
  std::set<std::string> names;
  for (std::size_t i = 0; i < kLineKinds; ++i) {
    words_seen.insert(words(static_cast<LineKind>(i)));
    names.insert(to_string(static_cast<LineKind>(i)));
  }
  CHECK(words_seen == std::set<std::string>{"reply", "own-initiative", "call-out"});
  CHECK(names.size() == kLineKinds);
  CHECK(std::string(words(LineVerdict::None)).empty());
  CHECK(std::string(words(LineVerdict::Quick)) == "quick");
  CHECK(std::string(words(LineVerdict::Acceptable)) == "acceptable");
  CHECK(std::string(words(LineVerdict::Slow)) == "slow");
  CHECK(std::string(to_string(LineVerdict::None)) == "none");
}
