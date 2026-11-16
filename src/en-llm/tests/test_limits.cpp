// ARCH §10.2's caps on what a server sends, each just under and just over: a stream's line at most 64 KB, a whole
// response at most 1 MB, a stream cancelled once it passes 16 KB of display text, and each JSON payload at most 256 KB
// and 32 deep before it is parsed. On the parser and the assembler, then through the client on the fake transport.
#include <doctest.h>

#include <string>
#include <utility>
#include <vector>

#include "en/llm/sse.hpp"
#include "en/llm/stream.hpp"
#include "en/proto/json.hpp"
#include "rig.hpp"

using namespace en::llm;
using namespace en::llm::test;
using nlohmann::json;
using en::proto::kMaxJsonBytes;
using en::proto::kMaxJsonDepth;

namespace {

std::string chunk_text(const std::string& content) { return delta_chunk(content).dump(); }

// A content chunk padded with spaces to exactly `size` bytes: whitespace after the object leaves the same JSON.
std::string padded_chunk(const std::string& content, std::size_t size) {
  std::string chunk = chunk_text(content);
  REQUIRE(chunk.size() <= size);
  chunk.append(size - chunk.size(), ' ');
  return chunk;
}

// A content chunk whose deepest point is `depth`: the object itself, then nested arrays in a field the reader skips.
std::string nested_chunk(const std::string& content, std::size_t depth) {
  std::string chunk = chunk_text(content);
  chunk.pop_back();  // the closing brace
  return chunk + R"(,"pad":)" + std::string(depth - 1, '[') + std::string(depth - 1, ']') + "}";
}

// One SSE event whose data is exactly `payload` bytes: the chunk on its first data line, then lines of spaces, each
// well under the line cap; the newlines that join them count, and are whitespace to the JSON.
std::string big_event(const std::string& content, std::size_t payload) {
  const std::string first = chunk_text(content);
  std::vector<std::string> lines{first};
  std::size_t size = first.size();
  while (size < payload) {
    const std::size_t room = payload - size - 1;  // a newline joins the next line on
    const std::size_t n = std::min<std::size_t>(room, 60000);
    lines.emplace_back(n, ' ');
    size += 1 + n;
  }
  REQUIRE(size == payload);
  std::string event;
  for (const std::string& line : lines) event += "data: " + line + "\n";
  return event + "\n";
}

// SSE comment lines (a server's keep-alives) of exactly `total` bytes, none longer than the line cap.
std::string comments(std::size_t total) {
  std::string out;
  while (total > 0) {
    const std::size_t n = std::min<std::size_t>(total, 60000);
    REQUIRE(n >= 2);
    out += ":" + std::string(n - 2, '.') + "\n";
    total -= n;
  }
  return out;
}

std::string finish_events() {
  const json last = {{"choices", json::array({{{"index", 0}, {"delta", json::object()}, {"finish_reason", "stop"}}})}};
  return sse_data(last) + "data: [DONE]\n\n";
}

// A 200 stream of these pieces, all at 100 ms, and its end at 1 s: a cancel in between is seen as one.
FakeScript stream_of(const std::vector<std::string>& pieces) {
  std::vector<std::pair<Millis, std::string>> timed;
  for (const std::string& piece : pieces) timed.emplace_back(Millis(100), piece);
  return sse_raw(timed, Millis(1000));
}

FakeScript whole_body(const std::string& body) {
  return {FakeStep{Millis(0), TransportEventKind::Head, 200, {{"content-type", "application/json"}}, "", ""},
          FakeStep{Millis(100), TransportEventKind::Data, 200, {}, body, ""},
          FakeStep{Millis(100), TransportEventKind::Done, 200, {}, "", ""}};
}

std::string whole_reply(const std::string& content) {
  return json{{"choices", json::array({{{"message", {{"content", content}}}, {"finish_reason", "stop"}}})}}.dump();
}

}  // namespace

TEST_CASE("limits: a stream line of 64 KB is read, and a byte more stops the parse") {
  const std::string line = "data: " + std::string(kMaxSseLine - 6, 'x');
  REQUIRE(line.size() == 64u * 1024u);
  SseParser at_cap;
  std::vector<SseEvent> events;
  CHECK(at_cap.feed(line + "\n\n", events));
  REQUIRE(events.size() == 1);
  CHECK(events[0].data.size() == kMaxSseLine - 6);
  SseParser past;
  events.clear();
  CHECK_FALSE(past.feed(line + "x\n\n", events));
  CHECK(past.failed());
  CHECK(events.empty());
}

TEST_CASE("limits: a stream event of 256 KB of JSON is parsed; a byte more, or a level deeper than 32, is refused") {
  ReplyAssembler fits;
  fits.on_event_data(padded_chunk("fits", kMaxJsonBytes));
  CHECK(fits.text() == "fits");
  CHECK(fits.refused().empty());

  ReplyAssembler past;
  CHECK(past.on_event_data(padded_chunk("never read", kMaxJsonBytes + 1)).delta.empty());
  CHECK(past.text().empty());
  CHECK(past.refused() == "a stream event is larger than 256 KB");
  past.on_event_data(chunk_text("after"));  // a refused stream reads nothing more
  CHECK(past.text().empty());

  ReplyAssembler deep;
  deep.on_event_data(nested_chunk("deep enough", kMaxJsonDepth));
  CHECK(deep.text() == "deep enough");
  ReplyAssembler too_deep;
  too_deep.on_event_data(nested_chunk("never read", kMaxJsonDepth + 1));
  CHECK(too_deep.text().empty());
  CHECK(too_deep.refused() == "a stream event is nested deeper than 32");

  // A whole body (a server that ignored stream: true) is held to the same caps; a malformed one only counts.
  ReplyAssembler body;
  body.on_whole_body(whole_reply("Whole.") + std::string(kMaxJsonBytes - whole_reply("Whole.").size(), ' '));
  CHECK(body.text() == "Whole.");
  ReplyAssembler big_body;
  big_body.on_whole_body(whole_reply("Whole.") + std::string(kMaxJsonBytes + 1 - whole_reply("Whole.").size(), ' '));
  CHECK(big_body.refused() == "the reply is larger than 256 KB");
  ReplyAssembler broken;
  broken.on_event_data("{\"choices\": [");
  CHECK(broken.bad_chunks() == 1);
  CHECK(broken.refused().empty());
}

TEST_CASE("limits: 16 KB of display text is a reply; past it the text stops there, at a character, and it is over") {
  ReplyAssembler at_cap;
  at_cap.on_event_data(chunk_text(std::string(kMaxReplyText - 1, 'a')));
  CHECK(at_cap.on_event_data(chunk_text("b")).delta == "b");
  CHECK(at_cap.text().size() == 16u * 1024u);
  CHECK_FALSE(at_cap.overflowed());

  ReplyAssembler past;
  past.on_event_data(chunk_text(std::string(kMaxReplyText - 1, 'a')));
  const ReplyAssembler::Step step = past.on_event_data(chunk_text("bc"));
  CHECK(step.delta == "b");
  CHECK(past.overflowed());
  CHECK(past.text().size() == kMaxReplyText);
  CHECK(past.on_event_data(chunk_text("more")).delta.empty());  // nothing after it is read
  CHECK(past.finish().delta.empty());
  CHECK(past.text().size() == kMaxReplyText);

  // A character that would straddle the cap is left out whole.
  ReplyAssembler utf8;
  utf8.on_event_data(chunk_text(std::string(kMaxReplyText - 1, 'a')));
  utf8.on_event_data(chunk_text("\xC3\xA9"));  // é, two bytes
  CHECK(utf8.overflowed());
  CHECK(utf8.text() == std::string(kMaxReplyText - 1, 'a'));

  // Reasoning is not display text: only what is shown counts.
  ReplyAssembler thinking;
  thinking.on_event_data(chunk_text("<think>" + std::string(3 * kMaxReplyText, 'r') + "</think>Shown."));
  CHECK_FALSE(thinking.overflowed());
  CHECK(thinking.text() == "Shown.");
}

TEST_CASE("limits through the client: a reply past 16 KB of text is cut there and its stream cancelled") {
  {
    Rig rig;
    rig.server->script("model-a", stream_of({sse_data(delta_chunk(std::string(kMaxReplyText / 2, 'a'))),
                                             sse_data(delta_chunk(std::string(kMaxReplyText / 2, 'b'))),
                                             finish_events()}));
    CollectSink sink;
    const ChatResult r = rig.chat("dialogue", Purpose::Reply, &sink);
    CHECK(r.ok());
    CHECK(r.text.size() == kMaxReplyText);
  }
  {
    Rig rig;
    rig.server->script("model-a",
                       sse_raw({{Millis(100), sse_data(delta_chunk(std::string(kMaxReplyText / 2, 'a')))},
                                {Millis(120), sse_data(delta_chunk(std::string(kMaxReplyText / 2 + 1, 'b')))},
                                {Millis(140), sse_data(delta_chunk("never read"))}},
                               Millis(20000)));
    CollectSink sink;
    const ChatResult r = rig.chat("dialogue", Purpose::Reply, &sink);
    CHECK(r.error == ErrorClass::Truncated);
    CHECK(r.message == "the reply passed 16 KB of text, so the stream was cancelled");
    CHECK(r.text.size() == kMaxReplyText);
    CHECK(sink.text() == r.text);  // what the caller was shown is the text kept
    const auto requests = rig.server->requests();
    REQUIRE(requests.size() == 1);  // text was shown, so no second reply follows
    CHECK(requests[0].cancelled);
    CHECK(requests[0].ended - requests[0].started == Millis(120));  // cancelled at once, not at the stream's end
  }
}

TEST_CASE("limits through the client: a response of 1 MB is read, and a byte more fails it") {
  const std::string head = sse_data(delta_chunk("Hello."));
  const std::string tail = finish_events();
  const std::size_t room = kMaxResponse - head.size() - tail.size();
  {
    Rig rig;
    rig.server->script("model-a", stream_of({head, comments(room), tail}));
    CollectSink sink;
    const ChatResult r = rig.chat("dialogue", Purpose::Reply, &sink);
    CHECK(r.ok());
    CHECK(r.text == "Hello.");
  }
  {
    Rig rig;
    rig.server->script("model-a", stream_of({head, comments(room + 1), tail}));
    CollectSink sink;
    const ChatResult r = rig.chat("dialogue", Purpose::Reply, &sink);
    CHECK(r.error == ErrorClass::Server);
    CHECK(r.message == "the reply passed 1 MB");
    CHECK(r.text == "Hello.");
    CHECK(rig.server->requests()[0].cancelled);
  }
  {
    // An error's body past the cap goes unread: the status already names the error.
    Rig rig(kTwoClouds);
    rig.server->script("model-a", {FakeStep{Millis(50), TransportEventKind::Head, 429, {{"retry-after", "7"}}, "", ""},
                                   FakeStep{Millis(60), TransportEventKind::Data, 429, {}, std::string(kMaxResponse, ' '), ""},
                                   FakeStep{Millis(70), TransportEventKind::Data, 429, {}, " ", ""},
                                   FakeStep{Millis(5000), TransportEventKind::Done, 429, {}, "", ""}});
    rig.server->script("model-b", sse_reply({"From the fallback."}));
    const ChatResult r = rig.chat("dialogue", Purpose::Reply);
    REQUIRE(r.attempts.size() == 2);
    CHECK(r.attempts[0].error == ErrorClass::RateLimited);
    CHECK(rig.server->requests()[0].cancelled);
    CHECK(r.text == "From the fallback.");
  }
  {
    // The same 1 MB holds for a server that ignored stream: true, and its JSON is held to 256 KB first.
    const std::string at_cap = whole_reply("Whole.") + std::string(kMaxJsonBytes - whole_reply("Whole.").size(), ' ');
    Rig rig;
    rig.server->script("model-a", whole_body(at_cap));
    CHECK(rig.chat("dialogue", Purpose::Reply).text == "Whole.");
    Rig past;
    past.server->script("model-a", whole_body(at_cap + " "));
    past.server->script("model-b", whole_body(whole_reply("Fallback.")));
    const ChatResult r = past.chat("dialogue", Purpose::Reply);
    CHECK(r.attempts[0].error == ErrorClass::Server);
    CHECK(r.attempts[0].message == "the reply is larger than 256 KB");
    CHECK(r.text == "Fallback.");
    Rig huge;
    huge.server->script("model-a", whole_body(std::string(kMaxResponse + 1, ' ')));
    huge.server->script("model-b", whole_body(whole_reply("Fallback.")));
    CHECK(huge.chat("dialogue", Purpose::Reply).attempts[0].message == "the reply passed 1 MB");
  }
}

TEST_CASE("limits through the client: a stream event past 256 KB or 32 levels fails the attempt unparsed") {
  {
    Rig rig;
    rig.server->script("model-a", stream_of({big_event("Big but fine.", kMaxJsonBytes), finish_events()}));
    const ChatResult r = rig.chat("dialogue", Purpose::Reply);
    CHECK(r.ok());
    CHECK(r.text == "Big but fine.");
  }
  {
    Rig rig;
    rig.server->script("model-a", stream_of({big_event("Never read.", kMaxJsonBytes + 1), finish_events()}));
    rig.server->script("model-b", sse_reply({"From the fallback."}));
    const ChatResult r = rig.chat("dialogue", Purpose::Reply);
    CHECK(r.attempts[0].error == ErrorClass::Server);
    CHECK(r.attempts[0].message == "a stream event is larger than 256 KB");
    CHECK(r.text == "From the fallback.");
  }
  {
    Rig rig;
    rig.server->script("model-a", stream_of({sse_data(json::parse(nested_chunk("Deep.", kMaxJsonDepth))), finish_events()}));
    CHECK(rig.chat("dialogue", Purpose::Reply).text == "Deep.");
    Rig too_deep;
    too_deep.server->script("model-a", stream_of({"data: " + nested_chunk("Never read.", kMaxJsonDepth + 1) + "\n\n"}));
    too_deep.server->script("model-b", sse_reply({"From the fallback."}));
    const ChatResult r = too_deep.chat("dialogue", Purpose::Reply);
    CHECK(r.attempts[0].message == "a stream event is nested deeper than 32");
    CHECK(r.text == "From the fallback.");
  }
}
