// The real transports against small servers on 127.0.0.1, on real time: one conformance suite, run against libcurl
// where EN_WITH_CURL is set and against WinHTTP on Windows (under Wine on the build machine). Through the client: a
// stream with the key only in its header, a 429 with Retry-After, cancelling mid-stream, a refused connection. On the
// transport itself: a GET with no body, two exchanges at once, a chunked stream piece by piece, a stall cancelled,
// UTF-8 both ways, keep-alive across transports, redirects not followed, loopback past a proxy, cancelling at any
// moment, a reply cut short, TLS refused, and the connect timeout.
#include <doctest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "en/proto/ffi_guard.hpp"
#include "fault_alloc.hpp"
#include "local_server.hpp"
#include "rig.hpp"

using namespace en::llm;
using namespace en::llm::test;
using nlohmann::json;
using namespace std::chrono_literals;
using SteadyClock = std::chrono::steady_clock;

namespace {

struct Candidate {
  const char* name;
  std::shared_ptr<TransportFactory> (*make)();
};

// The real transports this build has.
std::vector<Candidate> transports() {
  std::vector<Candidate> list;
#ifdef EN_WITH_CURL
  list.push_back({"libcurl", &make_curl_transport_factory});
#endif
#ifdef _WIN32
  list.push_back({"WinHTTP", &make_winhttp_transport_factory});
#endif
  return list;
}

// Runs `body` once for each real transport, naming it in any failure.
void for_each_transport(const std::function<void(const Candidate&)>& body) {
  for (const Candidate& candidate : transports()) {
    const std::string name = candidate.name;
    INFO("transport: " << name);
    body(candidate);
  }
}

// WinHTTP under Wine, whose implementation differs from Windows' in two ways the transport cannot change: a test meets
// them by checking what Wine does, and says so.
bool wine_winhttp(const Candidate& candidate) {
  return std::string(candidate.name) == "WinHTTP" && running_under_wine();
}

constexpr const char* kStreamHead =
    "HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\nCache-Control: no-cache\r\nConnection: close\r\n\r\n";
constexpr const char* kChunkedStreamHead =
    "HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\nCache-Control: no-cache\r\n"
    "Transfer-Encoding: chunked\r\n\r\n";

std::string sse_content(const std::string& content) { return sse_data(delta_chunk(content)); }

std::string lower(std::string s) {
  for (char& c : s) {
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
  }
  return s;
}

bool contains(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }

std::string local_providers(int port) {
  return "schema = 1\n[[provider]]\nid = \"local\"\nflavor = \"llamacpp\"\nkind = \"local\"\n"
         "base_url = \"http://127.0.0.1:" + std::to_string(port) + "/v1\"\nkey_ref = \"secrets:cloud\"\n"
         "[slots.dialogue]\nmodel = \"local-model\"\n";
}

ChatRequest hello() {
  ChatRequest request;
  request.messages = {Message::system("An invented frame.", true), Message::user("Hello.")};
  return request;
}

std::string url(const std::string& host, int port, const std::string& path) {
  return "http://" + host + ":" + std::to_string(port) + path;
}

HttpRequest get_request(int port, const std::string& path, const std::string& host = "127.0.0.1") {
  HttpRequest request;
  request.method = "GET";
  request.url = url(host, port, path);
  request.headers.push_back(HttpHeader{"Accept", "application/json"});
  return request;
}

HttpRequest post_request(int port, const std::string& path, std::string body) {
  HttpRequest request;
  request.url = url("127.0.0.1", port, path);
  request.headers.push_back(HttpHeader{"Content-Type", "application/json"});
  request.headers.push_back(HttpHeader{"Accept", "text/event-stream"});
  request.body = std::move(body);
  return request;
}

// A whole reply with its Content-Length; `close` ends the connection after it.
std::string reply(const std::string& status, const std::string& type, const std::string& body, bool close = false) {
  return "HTTP/1.1 " + status + "\r\nContent-Type: " + type + "\r\nContent-Length: " + std::to_string(body.size()) +
         "\r\n" + (close ? "Connection: close\r\n" : "") + "\r\n" + body;
}

// What one exchange produced, its events checked for order as they come: a Head first, Data after it, then one end.
struct Outcome {
  int status = 0;
  std::vector<HttpHeader> headers;
  std::string data;
  int pieces = 0;
  bool done = false;
  std::string failed;  // why it failed, when it did

  bool ended() const { return done || !failed.empty(); }

  std::string header(const std::string& name) const {
    for (const HttpHeader& h : headers) {
      if (h.name == name) return h.value;
    }
    return "";
  }

  void record(const TransportEvent& event) {
    CHECK_FALSE(ended());  // nothing after the end
    switch (event.kind) {
      case TransportEventKind::Head:
        CHECK(status == 0);
        status = event.status;
        headers = event.headers;
        break;
      case TransportEventKind::Data:
        CHECK(status != 0);
        data += event.data;
        ++pieces;
        break;
      case TransportEventKind::Done:
        CHECK(status != 0);
        done = true;
        break;
      case TransportEventKind::Failed:
        failed = event.error.empty() ? "(no reason given)" : event.error;
        break;
    }
  }
};

// Runs the transport until each of `ids` has ended or `limit` has passed. An event for any other exchange fails.
std::map<int, Outcome> run(Transport& transport, const std::vector<int>& ids, std::chrono::milliseconds limit = 5s) {
  std::map<int, Outcome> outcomes;
  for (int id : ids) outcomes[id];
  const auto deadline = SteadyClock::now() + limit;
  const auto all_ended = [&] {
    for (const auto& entry : outcomes) {
      if (!entry.second.ended()) return false;
    }
    return true;
  };
  std::vector<TransportEvent> events;
  while (!all_ended() && SteadyClock::now() < deadline) {
    events.clear();
    transport.wait(50ms, events);
    for (const TransportEvent& event : events) {
      const auto it = outcomes.find(event.exchange);
      CHECK_MESSAGE(it != outcomes.end(), "an event for exchange " << event.exchange);
      if (it != outcomes.end()) it->second.record(event);
    }
  }
  return outcomes;
}

Outcome run_one(Transport& transport, const HttpRequest& request, std::chrono::milliseconds limit = 5s) {
  const int id = transport.start(request);
  CHECK(id > 0);
  return run(transport, {id}, limit)[id];
}

}  // namespace

TEST_CASE("transport: a reply streams from a local server, and the key travels only in its header") {
  for_each_transport([](const Candidate& candidate) {
    LocalServer server([](Connection& c, const SeenRequest&) {
      c.send(kStreamHead);
      c.send(": keep-alive\n\n");
      c.send(sse_content("Hello from"));
      std::this_thread::sleep_for(20ms);
      c.send(sse_content(" the local server."));
      const json stop = {{"index", 0}, {"delta", json::object()}, {"finish_reason", "stop"}};
      const json usage = {{"prompt_tokens", 12}, {"completion_tokens", 5}};
      const json last = {{"choices", json::array({stop})}, {"usage", usage}};
      c.send(sse_data(last) + "data: [DONE]\n\n");
      return false;
    });
    Client client(make_config(local_providers(server.port())), candidate.make());
    CollectSink sink;
    const ChatResult r = client.chat(hello(), &sink);
    REQUIRE(r.ok());
    CHECK(r.text == "Hello from the local server.");
    CHECK(sink.text() == r.text);
    CHECK(r.usage.prompt_tokens == 12);
    CHECK(r.first_token.count() >= 0);

    const auto seen = server.seen();
    REQUIRE(seen.size() == 1);
    CHECK(seen[0].head.rfind("POST /v1/chat/completions HTTP/1.1", 0) == 0);
    CHECK(contains(seen[0].head, "Authorization: Bearer invented-test-key"));
    CHECK(contains(seen[0].head, "Accept: text/event-stream"));
    CHECK_FALSE(seen[0].has_header("expect"));
    CHECK_FALSE(seen[0].has_header("cookie"));
    const json body = json::parse(seen[0].body);
    CHECK(body["model"] == "local-model");
    CHECK(body["cache_prompt"] == true);
    CHECK(body["messages"][1]["content"] == "Hello.");
    CHECK_FALSE(contains(seen[0].body, "invented-test-key"));
  });
}

TEST_CASE("transport: a 429 with Retry-After cools the provider for that long") {
  for_each_transport([](const Candidate& candidate) {
    const std::string body = R"({"error": {"message": "slow down now"}})";
    LocalServer server([&body](Connection& c, const SeenRequest&) {
      c.send("HTTP/1.1 429 Too Many Requests\r\nRetry-After: 7\r\nContent-Type: application/json\r\nContent-Length: " +
             std::to_string(body.size()) + "\r\nConnection: close\r\n\r\n" + body);
      return false;
    });
    Client client(make_config(local_providers(server.port())), candidate.make());
    const ChatResult r = client.chat(hello());
    CHECK(r.error == ErrorClass::RateLimited);
    CHECK(contains(r.message, "slow down now"));
    const HealthReport report = client.health();
    REQUIRE(report.providers.size() == 1);
    CHECK(report.providers[0].cooling_left > Millis(6000));
    CHECK(report.providers[0].cooling_left <= Millis(7000));
  });
}

TEST_CASE("transport: cancelling mid-stream stops at once and keeps what came") {
  for_each_transport([](const Candidate& candidate) {
    LocalServer server([](Connection& c, const SeenRequest&) {
      c.send(kStreamHead);
      c.send(sse_content("First"));
      std::this_thread::sleep_for(700ms);
      c.send(sse_content(" late"));
      return false;
    });
    Client client(make_config(local_providers(server.port())), candidate.make());
    struct StopSink final : StreamSink {
      std::stop_source* stop = nullptr;
      std::string text;
      void on_delta(std::string_view t) override {
        text += t;
        stop->request_stop();
      }
    };
    std::stop_source stop;
    StopSink sink;
    sink.stop = &stop;
    const auto started = SteadyClock::now();
    const ChatResult r = client.chat(hello(), &sink, stop.get_token());
    CHECK(r.error == ErrorClass::Cancelled);
    CHECK(r.text == "First");
    CHECK(SteadyClock::now() - started < 500ms);  // well before the server's next piece
  });
}

TEST_CASE("transport: a refused connection is a Server error, and its message names no address") {
  for_each_transport([](const Candidate& candidate) {
    const int port = unused_port();
    Client client(make_config(local_providers(port)), candidate.make());
    const ChatResult r = client.chat(hello());
    CHECK(r.error == ErrorClass::Server);
    CHECK(contains(r.message, "connection failed"));
    CHECK_FALSE(contains(r.message, "127.0.0.1"));
    CHECK_FALSE(contains(r.message, std::to_string(port)));
  });
}

TEST_CASE("transport: a GET carries no body, and its reply comes whole with lower-case header names") {
  for_each_transport([](const Candidate& candidate) {
    const std::string models = R"({"object":"list","data":[{"id":"invented-model","object":"model"}]})";
    LocalServer server([&models](Connection& c, const SeenRequest&) {
      return c.send(reply("200 OK", "application/json", models));
    });
    auto transport = candidate.make()->open();
    const Outcome o = run_one(*transport, get_request(server.port(), "/v1/models"));
    CHECK(o.done);
    CHECK(o.status == 200);
    CHECK(o.header("content-type") == "application/json");
    for (const HttpHeader& h : o.headers) CHECK(h.name == lower(h.name));
    CHECK(o.data == models);

    const auto seen = server.seen();
    REQUIRE(seen.size() == 1);
    CHECK(seen[0].method == "GET");
    CHECK(seen[0].target == "/v1/models");
    CHECK(seen[0].body.empty());
    CHECK_FALSE(seen[0].has_header("transfer-encoding"));
    if (seen[0].has_header("content-length")) CHECK(seen[0].header("content-length") == "0");
  });
}

TEST_CASE("transport: two exchanges run at once on one transport") {
  for_each_transport([](const Candidate& candidate) {
    Signal both_in;
    std::atomic<int> arrived{0};
    std::atomic<int> together{0};
    LocalServer server([&](Connection& c, const SeenRequest& request) {
      if (arrived.fetch_add(1) + 1 == 2) both_in.notify();
      if (both_in.wait_for(3s)) together.fetch_add(1);  // each answers only once both are in
      c.send(reply("200 OK", "text/plain", "reply to " + request.target, true));
      return false;
    });
    auto transport = candidate.make()->open();
    const int a = transport->start(post_request(server.port(), "/a", R"({"n": 1})"));
    const int b = transport->start(post_request(server.port(), "/b", R"({"n": 2})"));
    CHECK(a != b);
    auto outcomes = run(*transport, {a, b}, 6s);
    CHECK(together.load() == 2);
    CHECK(outcomes[a].done);
    CHECK(outcomes[b].done);
    CHECK(outcomes[a].data == "reply to /a");
    CHECK(outcomes[b].data == "reply to /b");
  });
}

TEST_CASE("transport: a chunked event stream reaches the client piece by piece") {
  for_each_transport([](const Candidate& candidate) {
    Signal first_seen;
    std::atomic<bool> more_sent{false};
    std::atomic<bool> end_sent{false};
    LocalServer server([&](Connection& c, const SeenRequest&) {
      c.send(kChunkedStreamHead);
      c.send(chunk(sse_content("one")));
      first_seen.wait_for(2s);  // the client should have the first event before anything more is sent
      more_sent.store(true);
      c.send(chunk(sse_content("two")));
      first_seen.wait_for(2s);  // Wine's WinHTTP hands a chunk over only once the next one begins
      end_sent.store(true);
      c.send(kLastChunk);
      return false;
    });
    auto transport = candidate.make()->open();
    const int id = transport->start(post_request(server.port(), "/v1/chat/completions", "{}"));
    Outcome o;
    bool first_before_more = false;
    bool first_before_end = false;
    std::vector<TransportEvent> events;
    const auto deadline = SteadyClock::now() + 8s;
    while (!o.ended() && SteadyClock::now() < deadline) {
      events.clear();
      transport->wait(50ms, events);
      for (const TransportEvent& event : events) {
        REQUIRE(event.exchange == id);
        if (event.kind == TransportEventKind::Data && o.pieces == 0) {
          first_before_more = !more_sent.load();
          first_before_end = !end_sent.load();
          first_seen.notify();
        }
        o.record(event);
      }
    }
    first_seen.notify();  // never leave the server waiting
    if (wine_winhttp(candidate)) {
      // Wine's read_data reads the next chunk's size line before it completes a read that ended a chunk, so a chunk
      // arrives once the next one begins: piece by piece still, one piece late.
      CHECK(first_before_end);
      if (!first_before_more) MESSAGE("Wine's WinHTTP: the first chunk came only when the second began");
    } else {
      CHECK(first_before_more);
    }
    CHECK(o.done);
    CHECK(o.status == 200);
    CHECK(o.header("content-type") == "text/event-stream");
    CHECK(o.data == sse_content("one") + sse_content("two"));  // the chunks' framing taken off
  });
}

TEST_CASE("transport: wait() keeps to its bound while a stream pauses mid-way, and cancel() is at once") {
  for_each_transport([](const Candidate& candidate) {
    Signal release;
    LocalServer server([&](Connection& c, const SeenRequest&) {
      c.send(kChunkedStreamHead);
      c.send(chunk(sse_content("before the pause")));
      release.wait_for(1500ms);  // the pause, mid-stream
      c.send(chunk(sse_content("after the pause")));
      c.send(kLastChunk);
      return false;
    });
    auto transport = candidate.make()->open();
    const int id = transport->start(post_request(server.port(), "/v1/chat/completions", "{}"));
    SteadyClock::duration longest{0};
    std::vector<TransportEvent> events;
    const auto until = SteadyClock::now() + 700ms;  // well inside the pause
    while (SteadyClock::now() < until) {
      const auto before = SteadyClock::now();
      events.clear();
      transport->wait(50ms, events);
      longest = std::max(longest, SteadyClock::now() - before);
    }
    CHECK(longest < 250ms);
    const auto cancelled_at = SteadyClock::now();
    transport->cancel(id);
    CHECK(SteadyClock::now() - cancelled_at < 100ms);
    release.notify();  // the rest comes now, and none of it may come through
    events.clear();
    transport->wait(300ms, events);
    CHECK(events.empty());
  });
}

TEST_CASE("transport: a server that stalls after its headers is cancelled at once, and the connection closes") {
  for_each_transport([](const Candidate& candidate) {
    std::atomic<bool> saw_close{false};
    std::atomic<SteadyClock::rep> closed_at{0};
    LocalServer server([&](Connection& c, const SeenRequest&) {
      c.send(kChunkedStreamHead);
      // The stall: nothing more, until the client goes.
      if (c.closed_by_client(3s)) {
        closed_at.store(SteadyClock::now().time_since_epoch().count());
        saw_close.store(true);
      }
      return false;
    });
    auto factory = candidate.make();
    auto transport = factory->open();
    const int id = transport->start(post_request(server.port(), "/v1/chat/completions", "{}"));
    Outcome o;
    std::vector<TransportEvent> events;
    const auto deadline = SteadyClock::now() + 3s;
    while (o.status == 0 && !o.ended() && SteadyClock::now() < deadline) {
      events.clear();
      transport->wait(50ms, events);
      for (const TransportEvent& event : events) o.record(event);
    }
    REQUIRE(o.status == 200);
    events.clear();
    transport->wait(150ms, events);
    CHECK(events.empty());  // stalled: no data, no end

    const auto cancelled_at = SteadyClock::now();
    transport->cancel(id);
    CHECK(SteadyClock::now() - cancelled_at < 100ms);
    events.clear();
    transport->wait(300ms, events);
    CHECK(events.empty());  // nothing after the cancel
    const auto destroyed_at = SteadyClock::now();
    transport.reset();
    factory.reset();
    CHECK(SteadyClock::now() - destroyed_at < 200ms);

    if (wine_winhttp(candidate)) {
      // Wine does not stop a receive in progress when its handle closes: the socket stays open until the server sends
      // or closes, or the receive timeout passes. The exchange is cancelled for the client all the same, as above.
      MESSAGE("Wine's WinHTTP keeps a cancelled exchange's connection open until the server sends or closes");
      return;
    }
    const auto limit = SteadyClock::now() + 4s;
    while (!saw_close.load() && SteadyClock::now() < limit) std::this_thread::sleep_for(10ms);
    REQUIRE(saw_close.load());  // the server learns at once that nobody listens any more
    const SteadyClock::time_point closed{SteadyClock::duration(closed_at.load())};
    CHECK(closed - cancelled_at < 500ms);
  });
}

TEST_CASE("transport: UTF-8 travels unchanged both ways") {
  // Invented words in several scripts, with a dash and a check mark.
  const std::string text = "Grüße, café, ünïcødé — 你好, привет, ✓";
  for_each_transport([&text](const Candidate& candidate) {
    LocalServer echo([](Connection& c, const SeenRequest& request) {
      c.send(reply("200 OK", "application/json; charset=utf-8", request.body, true));
      return false;
    });
    auto transport = candidate.make()->open();
    const std::string body = json{{"text", text}}.dump();  // raw UTF-8, not \u escapes
    REQUIRE(contains(body, "ü"));
    const Outcome o = run_one(*transport, post_request(echo.port(), "/echo", body));
    REQUIRE(echo.seen().size() == 1);
    CHECK(echo.seen()[0].body == body);
    CHECK(o.done);
    CHECK(o.data == body);
    CHECK(json::parse(o.data)["text"] == text);

    // And through the client, as a streamed reply.
    LocalServer stream([&text](Connection& c, const SeenRequest&) {
      c.send(kStreamHead);
      c.send(sse_content(text));
      c.send("data: [DONE]\n\n");
      return false;
    });
    Client client(make_config(local_providers(stream.port())), candidate.make());
    const ChatResult r = client.chat(hello());
    REQUIRE(r.ok());
    CHECK(r.text == text);
  });
}

TEST_CASE("transport: keep-alive carries the requests of a factory's transports on one connection") {
  for_each_transport([](const Candidate& candidate) {
    const std::string body = R"({"object":"list","data":[]})";
    LocalServer server([&body](Connection& c, const SeenRequest&) {
      return c.send(reply("200 OK", "application/json", body));
    });
    auto factory = candidate.make();
    for (int i = 0; i < 3; ++i) {
      auto transport = factory->open();
      const Outcome o = run_one(*transport, get_request(server.port(), "/v1/models"));
      CHECK(o.done);
      CHECK(o.data == body);
    }
    CHECK(server.seen().size() == 3);
    CHECK(server.connections() == 1);
  });
}

TEST_CASE("transport: a redirect is reported, never followed") {
  for_each_transport([](const Candidate& candidate) {
    LocalServer elsewhere([](Connection& c, const SeenRequest&) {
      c.send(reply("200 OK", "text/plain", "elsewhere", true));
      return false;
    });
    const std::string location = url("127.0.0.1", elsewhere.port(), "/v1/chat/completions");
    LocalServer server([&location](Connection& c, const SeenRequest&) {
      c.send("HTTP/1.1 307 Temporary Redirect\r\nLocation: " + location +
             "\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
      return false;
    });
    auto transport = candidate.make()->open();
    HttpRequest request = post_request(server.port(), "/v1/chat/completions", "{}");
    request.secret_headers.emplace_back("Authorization", Secret("Bearer invented-test-key"));
    const Outcome o = run_one(*transport, request);
    CHECK(o.done);
    CHECK(o.status == 307);
    CHECK(o.header("location") == location);
    CHECK(o.data.empty());
    CHECK(elsewhere.seen().empty());  // the key never went there
  });
}

TEST_CASE("transport: loopback never goes through the configured proxy") {
  for_each_transport([](const Candidate& candidate) {
    LocalServer proxy([](Connection& c, const SeenRequest&) {
      c.send(reply("200 OK", "text/plain", "through the proxy", true));
      return false;
    });
    LocalServer server([](Connection& c, const SeenRequest&) {
      c.send(reply("200 OK", "text/plain", "direct", true));
      return false;
    });
    // libcurl reads the variable for each transfer, so there the proxy is in force and loopback must pass it by.
    // WinHTTP takes its proxy from the system's settings, where a test cannot set one (and Wine's automatic mode uses
    // none at all): there this shows only that loopback goes direct.
    const ScopedEnv http_proxy("http_proxy", "http://127.0.0.1:" + std::to_string(proxy.port()));
    auto factory = candidate.make();
    for (const std::string host : {"127.0.0.1", "localhost"}) {
      CAPTURE(host);
      auto transport = factory->open();
      const Outcome o = run_one(*transport, get_request(server.port(), "/v1/models", host));
      CHECK(o.done);
      CHECK(o.data == "direct");
    }
    CHECK(proxy.seen().empty());
    CHECK(server.seen().size() == 2);
    if (std::string(candidate.name) == "libcurl") {
      // And the proxy is in force for any other host: a name that never resolves reaches it, and nothing else does.
      auto transport = factory->open();
      const Outcome o = run_one(*transport, get_request(80, "/v1/models", "remote.example.invalid"));
      CHECK(o.done);
      CHECK(o.data == "through the proxy");
      REQUIRE(proxy.seen().size() == 1);
      CHECK(contains(proxy.seen()[0].target, "remote.example.invalid"));
    }
  });
}

TEST_CASE("transport: exchanges cancelled at any moment send nothing more, and the transport goes on") {
  for_each_transport([](const Candidate& candidate) {
    LocalServer server([](Connection& c, const SeenRequest&) {
      if (!c.send(kChunkedStreamHead)) return false;
      for (int i = 0; i < 8; ++i) {
        std::this_thread::sleep_for(4ms);
        if (!c.send(chunk(sse_content("piece " + std::to_string(i))))) return false;
      }
      c.send(kLastChunk);
      return false;
    });
    auto factory = candidate.make();
    auto transport = factory->open();
    std::set<int> cancelled;
    std::vector<TransportEvent> events;
    for (int round = 0; round < 24; ++round) {
      const int id = transport->start(post_request(server.port(), "/v1/chat/completions", "{}"));
      for (int turn = 0; turn < round % 5; ++turn) {
        events.clear();
        transport->wait(std::chrono::milliseconds(2 + (round * 7) % 11), events);
        for (const TransportEvent& event : events) CHECK(cancelled.count(event.exchange) == 0);
      }
      transport->cancel(id);
      cancelled.insert(id);
    }
    // One more runs to its end, with nothing of the cancelled ones beside it.
    const int last = transport->start(post_request(server.port(), "/v1/chat/completions", "{}"));
    auto outcomes = run(*transport, {last});
    CHECK(outcomes[last].done);
    CHECK(contains(outcomes[last].data, "piece 7"));
    // Two left running when the transport goes, and the factory with it.
    transport->start(post_request(server.port(), "/v1/chat/completions", "{}"));
    transport->start(post_request(server.port(), "/v1/chat/completions", "{}"));
    events.clear();
    transport->wait(10ms, events);
    transport.reset();
    factory.reset();
  });
}

TEST_CASE("transport: a reply cut short of its Content-Length fails") {
  for_each_transport([](const Candidate& candidate) {
    LocalServer server([](Connection& c, const SeenRequest&) {
      c.send("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: 100\r\n\r\n{\"partial\":");
      return false;  // and the connection closes, 89 bytes short
    });
    auto transport = candidate.make()->open();
    const Outcome o = run_one(*transport, get_request(server.port(), "/v1/models"));
    CHECK(o.status == 200);
    CHECK_FALSE(o.done);
    CHECK_FALSE(o.failed.empty());
    CHECK(o.data == "{\"partial\":");
  });
}

TEST_CASE("transport: TLS with a server that does not speak it fails, and names no address") {
  for_each_transport([](const Candidate& candidate) {
    RawServer server("HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
    auto transport = candidate.make()->open();
    HttpRequest request = post_request(server.port(), "/v1/chat/completions", "{}");
    request.url = "https://127.0.0.1:" + std::to_string(server.port()) + "/v1/chat/completions";
    const Outcome o = run_one(*transport, request);
    REQUIRE_FALSE(o.failed.empty());
    CHECK(o.status == 0);
    const std::string why = lower(o.failed);
    CHECK((contains(why, "tls") || contains(why, "ssl")));
    CHECK_FALSE(contains(o.failed, "127.0.0.1"));
    CHECK_FALSE(contains(o.failed, std::to_string(server.port())));
  });
}

TEST_CASE("transport: a connection that is never answered ends at the request's connect timeout") {
  for_each_transport([](const Candidate& candidate) {
    FullBacklog backlog;
    auto transport = candidate.make()->open();
    HttpRequest request = get_request(backlog.port(), "/v1/models");
    request.connect_timeout = 300ms;
    const auto started = SteadyClock::now();
    const Outcome o = run_one(*transport, request);
    const auto took = SteadyClock::now() - started;
    REQUIRE_FALSE(o.failed.empty());
    CHECK(took < 1500ms);  // not the system's own connect timeout
    if (took >= 250ms) CHECK(contains(lower(o.failed), "time"));  // it waited: a timeout, by name
    CHECK_FALSE(contains(o.failed, "127.0.0.1"));
  });
}

TEST_CASE("transport: a callback that throws trips the transport's component, and its request ends at once") {
  for_each_transport([](const Candidate& candidate) {
    using en::proto::Component;
    en::proto::clear_trip(Component::LlmTransport);
    const std::string text(12000, 'x');
    std::atomic<int> answered{0};
    LocalServer server([&](Connection& c, const SeenRequest&) {
      // Once the request is on its way, the first allocation of a body piece's size on any thread but the server's
      // fails: inside libcurl's write callback, or WinHTTP's status callback (or the exchange's driver after it).
      if (answered++ == 0) faults::arm(2000, (16u << 10) + 1);
      c.send(kStreamHead + sse_content(text) + "data: [DONE]\n\n");
      return false;
    });
    Client client(make_config(local_providers(server.port())), candidate.make());
    const auto started = SteadyClock::now();
    const ChatResult broken = client.chat(hello());
    faults::disarm();
    REQUIRE(faults::fired());
    CHECK(broken.error == ErrorClass::Server);
    CHECK(contains(broken.message, "connection failed"));
    CHECK(en::proto::tripped(Component::LlmTransport));
    CHECK(SteadyClock::now() - started < 10s);  // at once, not at the slot's total time

    // The transport goes on: the next request is served whole.
    const ChatResult next = client.chat(hello());
    CHECK(next.ok());
    CHECK(next.text == text);
    en::proto::clear_trip(Component::LlmTransport);
  });
}

#ifdef _WIN32
TEST_CASE("WinHTTP: a malformed header or an address it does not take fails before anything is sent") {
  LocalServer server([](Connection& c, const SeenRequest&) {
    c.send(reply("200 OK", "text/plain", "sent", true));
    return false;
  });
  auto transport = make_winhttp_transport_factory()->open();
  HttpRequest header_break = post_request(server.port(), "/v1/chat/completions", "{}");
  header_break.headers.push_back(HttpHeader{"X-Note", "one\r\nX-Injected: two"});
  HttpRequest secret_break = post_request(server.port(), "/v1/chat/completions", "{}");
  secret_break.secret_headers.emplace_back("Authorization", Secret("Bearer invented\r\nX-Injected: two"));
  HttpRequest bad_name = post_request(server.port(), "/v1/chat/completions", "{}");
  bad_name.headers.push_back(HttpHeader{"X Note", "spaced"});
  HttpRequest other_scheme = post_request(server.port(), "/v1/chat/completions", "{}");
  other_scheme.url = "ftp://127.0.0.1:" + std::to_string(server.port()) + "/v1";
  HttpRequest credentials = post_request(server.port(), "/v1/chat/completions", "{}");
  credentials.url = "http://someone:invented-password@127.0.0.1:" + std::to_string(server.port()) + "/v1";
  std::vector<int> ids;
  for (const HttpRequest* r : {&header_break, &secret_break, &bad_name, &other_scheme, &credentials}) {
    ids.push_back(transport->start(*r));
  }
  auto outcomes = run(*transport, ids);
  for (int id : ids) {
    CAPTURE(id);
    CHECK_FALSE(outcomes[id].failed.empty());
    CHECK(outcomes[id].status == 0);
    CHECK_FALSE(contains(outcomes[id].failed, "invented"));
    CHECK_FALSE(contains(outcomes[id].failed, "127.0.0.1"));
  }
  CHECK(server.seen().empty());
}

TEST_CASE("WinHTTP: the default transport on Windows is WinHTTP") {
  CHECK(make_default_transport_factory() != nullptr);
  CHECK(make_winhttp_transport_factory() != nullptr);
}

TEST_CASE("WinHTTP: the transport uses the system's own winhttp.dll, loaded by its full path") {
  // The build checks the other half: the test program's import table names no winhttp.dll (CMakeLists.txt).
  REQUIRE(make_winhttp_transport_factory() != nullptr);
  void* module = winhttp_module();
  REQUIRE(module != nullptr);
  const ModuleOrigin origin = system_module_origin(module, "winhttp.dll");
  INFO("loaded from: " << origin.loaded);
  INFO("the system's: " << origin.expected);
  CHECK(origin.same);
}
#else
TEST_CASE("WinHTTP: other systems have neither the transport nor its library") {
  CHECK(make_winhttp_transport_factory() == nullptr);
  CHECK(winhttp_module() == nullptr);
}
#endif
