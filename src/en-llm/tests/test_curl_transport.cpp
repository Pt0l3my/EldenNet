// The libcurl transport against a small HTTP server on 127.0.0.1 (POSIX builds with EN_WITH_CURL): streaming, the
// key's header, a 429 with Retry-After, cancellation mid-stream and a refused connection, on real time.
#include <doctest.h>

#if defined(EN_WITH_CURL) && !defined(_WIN32)

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <cctype>
#include <chrono>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "rig.hpp"

using namespace en::llm;
using namespace en::llm::test;
using nlohmann::json;
using namespace std::chrono_literals;

namespace {

struct Seen {
  std::string head;
  std::string body;
};

void send_all(int fd, const std::string& text) {
  std::size_t sent = 0;
  while (sent < text.size()) {
    const ssize_t n = ::send(fd, text.data() + sent, text.size() - sent, MSG_NOSIGNAL);
    if (n <= 0) return;  // the client went away (a cancelled request)
    sent += static_cast<std::size_t>(n);
  }
}

// One connection at a time; each request is answered by the handler and the connection closed.
class LocalServer {
 public:
  using Handler = std::function<void(int fd)>;

  explicit LocalServer(Handler handler) : handler_(std::move(handler)) {
    fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    const int one = 1;
    ::setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;
    ::bind(fd_, reinterpret_cast<sockaddr*>(&addr), sizeof addr);
    ::listen(fd_, 8);
    socklen_t length = sizeof addr;
    ::getsockname(fd_, reinterpret_cast<sockaddr*>(&addr), &length);
    port_ = ntohs(addr.sin_port);
    thread_ = std::thread([this] { serve(); });
  }

  ~LocalServer() {
    stop_.store(true);
    thread_.join();
    ::close(fd_);
  }

  int port() const { return port_; }

  std::vector<Seen> seen() const {
    std::lock_guard lock(mutex_);
    return seen_;
  }

 private:
  void serve() {
    while (!stop_.load()) {
      pollfd p{fd_, POLLIN, 0};
      if (::poll(&p, 1, 20) <= 0) continue;
      const int client = ::accept(fd_, nullptr, nullptr);
      if (client < 0) continue;
      Seen request = read_request(client);
      {
        std::lock_guard lock(mutex_);
        seen_.push_back(request);
      }
      handler_(client);
      ::close(client);
    }
  }

  static Seen read_request(int fd) {
    std::string data;
    char buffer[4096];
    while (data.find("\r\n\r\n") == std::string::npos) {
      const ssize_t n = ::recv(fd, buffer, sizeof buffer, 0);
      if (n <= 0) return Seen{data, ""};
      data.append(buffer, static_cast<std::size_t>(n));
    }
    const std::size_t end = data.find("\r\n\r\n");
    Seen seen{data.substr(0, end), data.substr(end + 4)};
    std::string lower = seen.head;
    for (char& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    std::size_t length = 0;
    if (const auto at = lower.find("content-length:"); at != std::string::npos) length = std::stoul(lower.substr(at + 15));
    while (seen.body.size() < length) {
      const ssize_t n = ::recv(fd, buffer, sizeof buffer, 0);
      if (n <= 0) break;
      seen.body.append(buffer, static_cast<std::size_t>(n));
    }
    return seen;
  }

  Handler handler_;
  int fd_ = -1;
  int port_ = 0;
  std::atomic<bool> stop_{false};
  std::thread thread_;
  mutable std::mutex mutex_;
  std::vector<Seen> seen_;
};

constexpr const char* kStreamHead =
    "HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\nCache-Control: no-cache\r\nConnection: close\r\n\r\n";

std::string sse_content(const std::string& content) { return sse_data(delta_chunk(content)); }

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

}  // namespace

TEST_CASE("curl: a reply streams from a local server, and the key travels only in its header") {
  LocalServer server([](int fd) {
    send_all(fd, kStreamHead);
    send_all(fd, ": keep-alive\n\n");
    send_all(fd, sse_content("Hello from"));
    std::this_thread::sleep_for(20ms);
    send_all(fd, sse_content(" the local server."));
    const json last = {{"choices", json::array({{{"index", 0}, {"delta", json::object()}, {"finish_reason", "stop"}}})},
                       {"usage", {{"prompt_tokens", 12}, {"completion_tokens", 5}}}};
    send_all(fd, sse_data(last) + "data: [DONE]\n\n");
  });
  Client client(make_config(local_providers(server.port())), make_curl_transport_factory());
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
  CHECK(seen[0].head.find("Authorization: Bearer invented-test-key") != std::string::npos);
  CHECK(seen[0].head.find("Accept: text/event-stream") != std::string::npos);
  CHECK(seen[0].head.find("Expect:") == std::string::npos);
  const json body = json::parse(seen[0].body);
  CHECK(body["model"] == "local-model");
  CHECK(body["cache_prompt"] == true);
  CHECK(body["messages"][1]["content"] == "Hello.");
  CHECK(seen[0].body.find("invented-test-key") == std::string::npos);
}

TEST_CASE("curl: a 429 with Retry-After cools the provider for that long") {
  const std::string body = R"({"error": {"message": "slow down now"}})";
  LocalServer server([&body](int fd) {
    send_all(fd, "HTTP/1.1 429 Too Many Requests\r\nRetry-After: 7\r\nContent-Type: application/json\r\nContent-Length: " +
                     std::to_string(body.size()) + "\r\nConnection: close\r\n\r\n" + body);
  });
  Client client(make_config(local_providers(server.port())), make_curl_transport_factory());
  const ChatResult r = client.chat(hello());
  CHECK(r.error == ErrorClass::RateLimited);
  CHECK(r.message.find("slow down now") != std::string::npos);
  const HealthReport report = client.health();
  REQUIRE(report.providers.size() == 1);
  CHECK(report.providers[0].cooling_left > Millis(6000));
  CHECK(report.providers[0].cooling_left <= Millis(7000));
}

TEST_CASE("curl: cancelling mid-stream stops at once and keeps what came") {
  LocalServer server([](int fd) {
    send_all(fd, kStreamHead);
    send_all(fd, sse_content("First"));
    std::this_thread::sleep_for(700ms);
    send_all(fd, sse_content(" late"));
  });
  Client client(make_config(local_providers(server.port())), make_curl_transport_factory());
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
  const auto started = std::chrono::steady_clock::now();
  const ChatResult r = client.chat(hello(), &sink, stop.get_token());
  CHECK(r.error == ErrorClass::Cancelled);
  CHECK(r.text == "First");
  CHECK(std::chrono::steady_clock::now() - started < 500ms);  // well before the server's next piece
}

TEST_CASE("curl: a refused connection is a Server error") {
  int port = 0;
  {
    const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    ::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof addr);
    socklen_t length = sizeof addr;
    ::getsockname(fd, reinterpret_cast<sockaddr*>(&addr), &length);
    port = ntohs(addr.sin_port);
    ::close(fd);  // nothing listens there now
  }
  Client client(make_config(local_providers(port)), make_curl_transport_factory());
  const ChatResult r = client.chat(hello());
  CHECK(r.error == ErrorClass::Server);
  CHECK(r.message.find("connection failed") != std::string::npos);
  CHECK(r.message.find("127.0.0.1") == std::string::npos);  // curl's wording only, no address
}

#endif  // EN_WITH_CURL && !_WIN32
