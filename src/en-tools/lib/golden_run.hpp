#pragma once
// The golden runner (ARCH §8.12): a recording's or a scenario's inputs stepped through en-core (and so Augur), the
// events in the golden files' form, and our own line diff. Replays are deterministic, so the times match exactly too
// (C12). A recording from the owner's game slots in as a scenario does: only its inputs differ.
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "en/augur/events.hpp"
#include "en/augur/rules.hpp"
#include "en/shell/record.hpp"
#include "scn.hpp"

namespace en::tools {

struct Run {
  std::vector<augur::Event> events;
  std::vector<core::Change> changes;
  std::uint64_t rules_hash = 0;
  std::uint64_t inputs = 0;
};

// Every step through a fresh Core. With `rules`, that rule set instead of the built-in one (an error in it is answered
// as RuleErrors and nothing is run).
[[nodiscard]] proto::Expected<Run, std::vector<augur::RuleError>> run_core(std::vector<Step>& steps,
                                                                          const std::vector<augur::RuleFile>* rules = nullptr);

// A recording (`*.ensnap.jsonl`) read as the tools read it: linted first (lint --recording's check), then each line
// read back with en_shell's reader; each problem on standard error, starting "<cmd>: ", never quoting the file.
[[nodiscard]] std::optional<std::vector<shell::RecordedLine>> load_recording(const std::string& path, const char* cmd);
// A file of at most max_bytes, or nothing.
[[nodiscard]] std::optional<std::string> read_file(const std::string& path, std::uintmax_t max_bytes);
inline constexpr std::uintmax_t kMaxRecordingBytes = 512ull << 20;

// A recording's inputs (its lifecycle messages, banners and snapshots, in their order, each at the latest time seen
// so far), as replay steps them.
[[nodiscard]] std::vector<Step> steps_of(const std::vector<shell::RecordedLine>& lines);

// The golden file: a header line, then one line an event, handles normalised h1, h2, ... (augur::GoldenNames).
[[nodiscard]] std::string golden_text(const std::string& route, const std::string& source, const Run& run);

// Our own diff of two texts by lines: "-" for the golden's, "+" for the run's, with a line of context each side,
// at most `max_lines` lines; empty when they agree.
[[nodiscard]] std::string line_diff(const std::string& want, const std::string& got, std::size_t max_lines = 40);

}  // namespace en::tools
