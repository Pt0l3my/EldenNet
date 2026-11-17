#pragma once
// The recorder's lines (ARCH §7.5, in the numbers-only form the first in-game build writes; plan §1.1, §1.3): plain JSON
// Lines, one record a line, each line a report of the kind "recording" written through en-gal's closed-vocabulary
// writer (report.hpp) on one line, so that `eldennet-engine lint --recording` and DataPiler's guard check a file key by
// key (O73). Numbers only by construction: there is no free string, so no name, SteamID, path, game text or param
// value can be written. What ARCH §7.5 keeps as text waits for its keyed hashes and redaction (S10.2.d):
// - a text look-up's answer keeps its table, id and whether it was found, never the text;
// - a param row keeps its param, id and how many fields it had, never their values;
// - PlaythroughReady keeps its slot, class, keepsake and play time, and the name's length only.
// Each line is `{"report":"recording","schema":1,"t":<mono ms>,"k":"<kind>",...}`; the kinds are header (the first
// line), snap, life, banner, change, cmd and mark. Floats (positions, distances, bearings) are written with 7
// significant digits; a bearing without the camera (NaN) is left out.
#include <array>
#include <cstdint>
#include <string>
#include <string_view>

#include "en/core/core.hpp"
#include "en/gal/report.hpp"
#include "en/proto/command.hpp"
#include "en/proto/lifecycle.hpp"
#include "en/proto/snapshot.hpp"

namespace en::shell {

[[nodiscard]] const gal::report::Vocabulary& recording_vocabulary() noexcept;

struct RecordingHeader {
  std::array<std::uint16_t, 4> game_version{};  // the gated build's file version; zeros when the gate refused it
  std::string_view region = "none";             // ww, jp, other, none
  std::array<std::uint16_t, 3> en_version{};    // EldenNet's
  std::string_view spike = "none";              // the launcher's (watch.hpp's words), or none
  bool probe_build = false;                     // EN_PROBE
  std::uint64_t file = 0;                       // the file's number in this session (rotation), from 1
};

// Each answers the line with its newline, or an empty string if the writer refused it (a defect of ours: counted).
[[nodiscard]] std::string header_line(const RecordingHeader& h, std::uint64_t t);
[[nodiscard]] std::string snapshot_line(const proto::WorldSnapshot& s, std::uint64_t t);
[[nodiscard]] std::string lifecycle_line(const proto::Lifecycle& l, std::uint64_t t);
[[nodiscard]] std::string banner_line(core::BannerReason reason, std::uint64_t t);
[[nodiscard]] std::string change_line(const core::Change& c, std::uint64_t t);
[[nodiscard]] std::string command_line(const proto::GameCommand& c, std::uint64_t t);
[[nodiscard]] std::string mark_line(std::uint32_t n, std::uint64_t t);

// The recorder rotates its file past this many bytes (ARCH §7.5, impl).
inline constexpr std::uint64_t kRecordingRotateBytes = 256ull << 20;

}  // namespace en::shell
