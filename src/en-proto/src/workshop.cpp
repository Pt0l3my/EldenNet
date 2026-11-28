// Tools › Workshop's words for the recorder (workshop.hpp): drafts for the owner's review.
#include "en/proto/workshop.hpp"

namespace en::proto {

const char* words(RecordingWhy w) noexcept {
  switch (w) {
    case RecordingWhy::Off: return "Off: Record sessions was off at the start";
    case RecordingWhy::AtStart: return "On: Record sessions was on at the start";
    case RecordingWhy::TestRun: return "On: a test run records every session";
    case RecordingWhy::StartedHere: return "On: started here";
    case RecordingWhy::StoppedHere: return "Off: stopped here";
    case RecordingWhy::Failed: return "Off: EldenNet could not open a recording file";
  }
  return "Off";
}

const char* to_string(RecordingWhy w) noexcept {
  switch (w) {
    case RecordingWhy::Off: return "off";
    case RecordingWhy::AtStart: return "at_start";
    case RecordingWhy::TestRun: return "test_run";
    case RecordingWhy::StartedHere: return "started_here";
    case RecordingWhy::StoppedHere: return "stopped_here";
    case RecordingWhy::Failed: return "failed";
  }
  return "off";
}

}  // namespace en::proto
