// The store's messages' words (store.hpp).
#include "en/proto/store.hpp"

namespace en::proto {

const char* to_string(How h) noexcept {
  switch (h) {
    case How::Direct:
      return "direct";
    case How::Overheard:
      return "overheard";
    case How::Witnessed:
      return "witnessed";
  }
  return "?";
}

std::optional<How> how_from(std::string_view word) noexcept {
  if (word == "direct") return How::Direct;
  if (word == "overheard") return How::Overheard;
  if (word == "witnessed") return How::Witnessed;
  return std::nullopt;
}

const char* to_string(JobKind k) noexcept { return k == JobKind::Lived ? "lived" : "journal"; }

const char* to_string(JobCause c) noexcept {
  switch (c) {
    case JobCause::None:
      return "";
    case JobCause::Conversation:
      return "conversation";
    case JobCause::Exchange:
      return "exchange";
    case JobCause::Witness:
      return "witness";
  }
  return "";
}

const char* to_string(JobState s) noexcept {
  switch (s) {
    case JobState::Queued:
      return "queued";
    case JobState::Running:
      return "running";
    case JobState::Deferred:
      return "deferred";
    case JobState::Parked:
      return "parked";
    case JobState::Done:
      return "done";
  }
  return "?";
}

const char* db::to_string(Fault f) noexcept {
  switch (f) {
    case Fault::Busy:
      return "busy";
    case Fault::Full:
      return "full";
    case Fault::IoErr:
      return "ioerr";
    case Fault::Corrupt:
      return "corrupt";
    case Fault::NotADb:
      return "notadb";
    case Fault::Constraint:
      return "constraint";
    case Fault::TooNew:
      return "too_new";
    case Fault::NotOpen:
      return "not_open";
    case Fault::Invalid:
      return "invalid";
    case Fault::Other:
      return "other";
  }
  return "?";
}

bool DbRequest::speech() const noexcept {
  if (std::holds_alternative<db::AppendPage>(op)) return true;
  if (const auto* a = std::get_if<db::Append>(&op)) return a->event.kind == "PlayerSaid" || a->event.kind == "NpcSaid";
  return false;
}

}  // namespace en::proto
