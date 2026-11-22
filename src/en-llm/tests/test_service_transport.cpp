// The handle over the real transports (libcurl where EN_WITH_CURL is set, WinHTTP on Windows and under Wine), against
// a server on 127.0.0.1: the path the core takes in the game, from submit to the last event (S8.1.g).
#include <doctest.h>

#include <chrono>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "en/llm/service.hpp"
#include "fake_transport.hpp"
#include "local_server.hpp"
#include "rig.hpp"

using namespace en::llm;
using namespace en::llm::test;
using nlohmann::json;
using namespace std::chrono_literals;

namespace {

struct RealTransport {
  const char* name;
  std::shared_ptr<TransportFactory> (*make)();
};

std::vector<RealTransport> real_transports() {
  std::vector<RealTransport> list;
#ifdef EN_WITH_CURL
  list.push_back({"libcurl", &make_curl_transport_factory});
#endif
#ifdef _WIN32
  list.push_back({"WinHTTP", &make_winhttp_transport_factory});
#endif
  return list;
}

std::string ollama_providers(int port) {
  return "schema = 1\n[[provider]]\nid = \"ollama\"\nflavor = \"ollama\"\nkind = \"local\"\n"
         "base_url = \"http://127.0.0.1:" + std::to_string(port) + "/v1\"\n"
         "[slots.dialogue]\nmodel = \"local-talker\"\n";
}

// The events up to and including `id`'s terminal one, read as the core reads them, within a generous real-time bound
// that only a broken handle comes near.
std::vector<LlmEvent> events_until_end(Service& service, RequestId id) {
  std::vector<LlmEvent> events;
  const auto deadline = std::chrono::steady_clock::now() + 30s;
  while (std::chrono::steady_clock::now() < deadline) {
    std::optional<LlmEvent> event = service.wait(Millis(500));
    if (!event) continue;
    events.push_back(*event);
    if (event->id == id && event->terminal()) break;
  }
  return events;
}

}  // namespace

TEST_CASE("service over a real transport: a reply streams from submit to its one Ended event") {
  for (const RealTransport& transport : real_transports()) {
    INFO("transport: " << transport.name);
    LocalServer server([](Connection& c, const SeenRequest&) {
      c.send("HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\nConnection: close\r\n\r\n");
      c.send(sse_data(delta_chunk("Lamps for ")));
      std::this_thread::sleep_for(20ms);
      c.send(sse_data(delta_chunk("the road.")));
      const json stop = {{"index", 0}, {"delta", json::object()}, {"finish_reason", "stop"}};
      c.send(sse_data(json{{"choices", json::array({stop})}}) + "data: [DONE]\n\n");
      return false;
    });
    Service service(make_config(ollama_providers(server.port())), transport.make());
    ChatRequest request;
    request.messages = {Message::system("An invented frame.", true), Message::user("Hello.")};
    REQUIRE(service.submit(LlmRequest::chat(7, request)) == SubmitResult::Accepted);

    const std::vector<LlmEvent> events = events_until_end(service, 7);
    REQUIRE_FALSE(events.empty());
    CHECK(events.front().kind == EventKind::Started);
    CHECK(events.front().provider == "ollama");
    CHECK(events.front().model == "local-talker");
    std::string streamed;
    int ended = 0;
    for (const LlmEvent& e : events) {
      CHECK(e.id == 7);
      if (e.kind == EventKind::Delta) streamed += e.text;
      if (e.kind != EventKind::Ended) continue;
      ++ended;
      REQUIRE(e.chat.has_value());
      CHECK(e.chat->ok());
      CHECK(e.chat->text == "Lamps for the road.");
    }
    CHECK(ended == 1);
    CHECK(streamed == "Lamps for the road.");
    CHECK_FALSE(service.poll().has_value());  // nothing after the end
  }
}

TEST_CASE("service over a real transport: a dialogue request's stop sequences reach the server as it sent them") {
  for (const RealTransport& transport : real_transports()) {
    INFO("transport: " << transport.name);
    LocalServer server([](Connection& c, const SeenRequest&) {
      c.send("HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\nConnection: close\r\n\r\n");
      c.send(sse_data(delta_chunk("Down, mostly.")));
      const json stop = {{"index", 0}, {"delta", json::object()}, {"finish_reason", "stop"}};
      c.send(sse_data(json{{"choices", json::array({stop})}}) + "data: [DONE]\n\n");
      return false;
    });
    Service service(make_config(ollama_providers(server.port())), transport.make());
    ChatRequest request;
    request.messages = {Message::system("An invented frame.", true), Message::user("Where does the road go?")};
    request.stop = {"\nThe Wanderer:", "\nWanderer:"};
    REQUIRE(service.submit(LlmRequest::chat(8, request)) == SubmitResult::Accepted);
    const std::vector<LlmEvent> events = events_until_end(service, 8);
    REQUIRE_FALSE(events.empty());
    CHECK(events.back().kind == EventKind::Ended);

    // The server's own view: the body as it arrived, read as JSON.
    const std::vector<SeenRequest> seen = server.seen();
    REQUIRE(seen.size() == 1);
    CHECK(seen[0].target == "/v1/chat/completions");
    const json body = json::parse(seen[0].body);
    CHECK(body["stop"] == json({"\nThe Wanderer:", "\nWanderer:"}));
    CHECK(body["model"] == "local-talker");
  }
}

TEST_CASE("service over a real transport: Ollama's warm-up reaches its native endpoint, with no prompt") {
  for (const RealTransport& transport : real_transports()) {
    INFO("transport: " << transport.name);
    LocalServer server([](Connection& c, const SeenRequest&) {
      const std::string body = R"({"model": "local-talker", "done": true})";
      c.send("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: " + std::to_string(body.size()) +
             "\r\nConnection: close\r\n\r\n" + body);
      return false;
    });
    Service service(make_config(ollama_providers(server.port())), transport.make());
    const std::optional<RequestId> id = service.warm_up(kDialogue);
    REQUIRE(id.has_value());

    const std::vector<LlmEvent> events = events_until_end(service, *id);
    REQUIRE(events.size() == 1);
    CHECK(events[0].kind == EventKind::WarmUpEnded);
    CHECK(events[0].warm_up.ok);
    CHECK(events[0].warm_up.status == 200);
    const std::vector<SeenRequest> seen = server.seen();
    REQUIRE(seen.size() == 1);
    CHECK(seen[0].head.rfind("POST /api/generate HTTP/1.1", 0) == 0);
    CHECK(json::parse(seen[0].body) == json{{"model", "local-talker"}, {"keep_alive", "10m"}});
    CHECK_FALSE(seen[0].has_header("authorization"));
  }
}
