#pragma once
// The model calls' record (PAGES §4.6.2's AI › Requests and §4.6.1's Model calls card; roadmap S11.4.a): en-proto's
// CallsView, which en-overlay's space publishes, built from what EldenNet sent and what the model client answered.
// Pure (ARCH §2.3): the DLL keeps one on T6 and tells it, with the time since T1's start, of each request it hands the
// model client, each of the client's events, the core's notes and the pages it committed on display (record_step),
// and the client's count of requests in the hour.
//
// - **One row per call.** A request handed over makes its row, waiting; the self-test's two requests and each model
//   list make theirs at their first event, from the CallStart the DLL gives (it does not submit them itself). A reply
//   is one row whatever it took: en-llm's retries, fallback and backups are one request's attempts, and the
//   conversation's regeneration after a lint hit joins its reply's row (CallNote), its attempts and tokens summed; the
//   times, the provider and the model are the last request's, the one whose answer counts.
// - **The outcome** (calls.hpp in en-proto): a page shown first (fallback model when the answer came from another model
//   than the request's first attempt), then waiting, then the end's class: timed out, refused, failed, or not shown
//   (stopped, or answered with nothing reaching the screen); a purpose with no screen (the self-test, a model list, a
//   draft) reads answered. A page committed after the request's end (pacing) turns not shown to shown.
// - **The usage** counts every call of the session, not only the rows kept: calls, the tokens and costs reported,
//   failures (rows whose outcome is a failure now), and own-initiative calls in the last hour; the client's own count
//   of the hour's requests and the bound are the DLL's to give.
// - **PV4.** The only text kept is a reply's displayed pages (the core's commits on display), never a page of an answer
//   a check withheld; nothing is ever taken from a request (its prompt) or a result (its text, its message), so no
//   prompt, unshown answer, key, base URL or server's words can reach a row. A model id that looks like a path is
//   left out (PV5).
#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "en/core/conversation.hpp"
#include "en/proto/calls.hpp"
#include "en/proto/llm.hpp"

namespace en::core {

struct Outputs;

// What a call is, where its request does not say it.
struct CallStart {
  std::string slot;
  proto::CallPurpose purpose = proto::CallPurpose::Reply;
  std::string speaker;  // as shown (SP3), or a type label; empty when nobody speaks
};

class CallRecord {
 public:
  explicit CallRecord(std::size_t kept = proto::kCallsKept);

  // The rows kept, kCallsKept or kCallsKeptDeveloper (debug.panel); the oldest past it leave the view.
  void set_kept(std::size_t kept);

  // A request handed to the model client at `at_ms` (the DLL's: a profile draft): its row, waiting.
  void sent(proto::RequestId id, CallStart start, std::uint64_t at_ms);
  // One of the conversation's notes (core.hpp's Outputs::calls): a reply's request, whose first makes the reply's row
  // and a regeneration's joins it, or a reply whose answer a check withheld.
  void note(const CallNote& n, std::uint64_t at_ms);
  // A page of a reply shown (a commit NpcSaid or NpcPage): its row reads shown, and its text grows by the page's, cut
  // at kCallTextMaxBytes. Nothing for a reply whose answer was withheld, or one the record does not know.
  void shown(proto::ConvId conv, proto::ReplyId reply, std::string_view text);
  // A request the model client did not take (the handle missing, its queue full): the row ends failed, `why` its class.
  void not_sent(proto::RequestId id, proto::ErrorClass why, std::uint64_t at_ms);
  // A request refused before it left (Pause AI): it was no call, so its row goes; a regeneration's, its reply's row
  // keeps the earlier request's end.
  void withdraw(proto::RequestId id);
  // One of the client's events: Started (its provider and model), Ended (the request's result) and ModelsEnded (a
  // model list's). `start` says what a call the record was not told of is (the self-test's requests and every model
  // list): its row is made at its first event. An event of a call the record does not know, given no start, is left
  // alone (a warm-up's, or a finished run's).
  void event(const proto::LlmEvent& e, std::uint64_t at_ms, const CallStart* start = nullptr);
  // The client's count of the requests it sent in the last hour and the bound they count against
  // (budget.requests_per_hour), and the time, by which the own-initiative calls of the last hour are counted.
  void hour(std::uint32_t requests, std::uint32_t bound, std::uint64_t at_ms);

  [[nodiscard]] const proto::CallsView& view() const noexcept { return view_; }

 private:
  struct Entry {
    proto::CallRow row;
    std::vector<proto::RequestId> requests;  // every request of the row, the one whose answer counts last
    std::optional<std::uint64_t> list;       // a model list's own id
    std::optional<std::pair<proto::ConvId, proto::ReplyId>> reply;  // a conversation's reply
    bool ended = false;     // the last request (or the list) ended
    proto::ErrorClass end = proto::ErrorClass::None;  // how
    bool shown = false;     // a page of it reached the screen
    bool withheld = false;  // a check withheld its answer: no page of it counts
    // What it adds to the usage now.
    std::uint64_t tokens_in = 0;
    std::uint64_t tokens_out = 0;
    double cost = 0;
    bool costed = false;
    bool failure = false;
  };

  Entry* by_request(proto::RequestId id);
  Entry* by_reply(proto::ConvId conv, proto::ReplyId reply);
  Entry& make(CallStart start, std::uint64_t at_ms);
  void end_chat(Entry& e, proto::RequestId id, const proto::ChatResult& r, std::uint64_t at_ms);
  void end_list(Entry& e, const proto::ModelsOutcome& m);
  // The row's outcome worked out again and the usage moved by what the entry adds now, then the view rebuilt.
  void settle(Entry& e);
  void rebuild();
  void prune_hour(std::uint64_t at_ms);

  std::size_t kept_ = proto::kCallsKept;
  std::vector<Entry> entries_;  // newest first (at_ms, then id), at most kept_ and 64 more
  std::uint64_t next_id_ = 1;
  std::deque<std::uint64_t> own_initiative_;  // when each own-initiative call of the last hour was sent
  proto::CallsView view_;
};

// The core's step for the record: its notes first, then the pages it committed on display (NpcSaid, NpcPage).
void record_step(CallRecord& calls, const Outputs& out, std::uint64_t at_ms);

}  // namespace en::core
