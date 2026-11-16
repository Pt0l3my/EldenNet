#include "local_server.hpp"

#include "fault_alloc.hpp"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <stdexcept>
#include <thread>

namespace en::llm::test {

namespace {

#ifdef _WIN32
using Socket = SOCKET;
const Socket kNoSocket = INVALID_SOCKET;
constexpr int kSendFlags = 0;
void close_socket(Socket s) { ::closesocket(s); }
// Winsock is started once for the test program and runs until it exits.
void start_sockets() {
  static const bool started = [] {
    WSADATA data;
    return WSAStartup(MAKEWORD(2, 2), &data) == 0;
  }();
  if (!started) throw std::runtime_error("Winsock did not start");
}
#else
using Socket = int;
constexpr Socket kNoSocket = -1;
constexpr int kSendFlags = MSG_NOSIGNAL;  // a client that went away is an error, not a signal
void close_socket(Socket s) { ::close(s); }
void start_sockets() {}
#endif

// Whether the socket has something to read, or was closed, within `ms`.
bool readable(Socket s, int ms) {
  fd_set set;
  FD_ZERO(&set);
  FD_SET(s, &set);
  timeval limit{};
  limit.tv_sec = ms / 1000;
  limit.tv_usec = (ms % 1000) * 1000;
  return ::select(static_cast<int>(s) + 1, &set, nullptr, nullptr, &limit) > 0;
}

int receive(Socket s, char* buffer, int size) { return static_cast<int>(::recv(s, buffer, size, 0)); }

bool send_all(Socket s, std::string_view text) {
  while (!text.empty()) {
    const int piece = static_cast<int>(std::min<std::size_t>(text.size(), 64 * 1024));
    const int sent = static_cast<int>(::send(s, text.data(), piece, kSendFlags));
    if (sent <= 0) return false;
    text.remove_prefix(static_cast<std::size_t>(sent));
  }
  return true;
}

sockaddr_in loopback_address(int port) {
  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  address.sin_port = htons(static_cast<unsigned short>(port));
  return address;
}

int port_of(Socket s) {
  sockaddr_in address{};
  socklen_t length = sizeof address;
  // SAFETY: the sockets API takes every address as a sockaddr*, with its length; this socket is IPv4, so its address
  // is a sockaddr_in, and `length` says the whole of it may be written.
  ::getsockname(s, reinterpret_cast<sockaddr*>(&address), &length);
  return ntohs(address.sin_port);
}

// A socket bound to 127.0.0.1 on a port the system picks, listening with `backlog` places.
Socket listen_loopback(int backlog) {
  start_sockets();
  const Socket s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (s == kNoSocket) throw std::runtime_error("the test server could not open a socket");
  const sockaddr_in address = loopback_address(0);
  // SAFETY: as in port_of: an IPv4 address passed as the API's sockaddr*, with its own size.
  if (::bind(s, reinterpret_cast<const sockaddr*>(&address), sizeof address) != 0 || ::listen(s, backlog) != 0) {
    close_socket(s);
    throw std::runtime_error("the test server could not listen on 127.0.0.1");
  }
  return s;
}

std::string lower(std::string_view s) {
  std::string out(s);
  for (char& c : out) {
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
  }
  return out;
}

std::string_view trim(std::string_view s) {
  while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.remove_prefix(1);
  while (!s.empty() && (s.back() == ' ' || s.back() == '\t')) s.remove_suffix(1);
  return s;
}

class SocketConnection final : public Connection {
 public:
  explicit SocketConnection(Socket s) : socket_(s) {}

  bool send(std::string_view text) override { return send_all(socket_, text); }

  bool closed_by_client(std::chrono::milliseconds limit) override {
    const auto deadline = std::chrono::steady_clock::now() + limit;
    char buffer[512];
    while (std::chrono::steady_clock::now() < deadline) {
      if (!readable(socket_, 10)) continue;
      if (receive(socket_, buffer, sizeof buffer) <= 0) return true;  // closed, or reset
    }
    return false;
  }

 private:
  Socket socket_;
};

}  // namespace

std::string SeenRequest::header(std::string_view name) const {
  const std::string wanted = lower(name);
  std::string_view rest = head;
  bool first = true;
  while (!rest.empty()) {
    const std::size_t end = rest.find("\r\n");
    const std::string_view line = rest.substr(0, end);
    rest = end == std::string_view::npos ? std::string_view() : rest.substr(end + 2);
    if (first) {  // the request line
      first = false;
      continue;
    }
    const std::size_t colon = line.find(':');
    if (colon != std::string_view::npos && lower(trim(line.substr(0, colon))) == wanted) {
      return std::string(trim(line.substr(colon + 1)));
    }
  }
  return "";
}

bool SeenRequest::has_header(std::string_view name) const {
  const std::string wanted = lower(name) + ":";
  return lower(head).find("\r\n" + wanted) != std::string::npos;
}

struct LocalServer::Impl {
  Handler handler;
  Socket listener = kNoSocket;
  int port = 0;
  std::atomic<bool> stopping{false};
  std::thread acceptor;
  mutable std::mutex mutex;
  std::vector<std::thread> workers;  // guarded by mutex
  std::vector<SeenRequest> seen;     // guarded by mutex
  int connections = 0;               // guarded by mutex

  void accept_loop();
  void serve(Socket client, int number);
  bool fill(Socket client, std::string& buffered, std::size_t wanted_size);
  bool read_request(Socket client, std::string& buffered, SeenRequest& request);
};

LocalServer::LocalServer(Handler handler) : impl_(std::make_unique<Impl>()) {
  impl_->handler = std::move(handler);
  impl_->listener = listen_loopback(64);
  impl_->port = port_of(impl_->listener);
  impl_->acceptor = std::thread([impl = impl_.get()] {
    faults::exempt_this_thread();
    impl->accept_loop();
  });
}

LocalServer::~LocalServer() {
  impl_->stopping.store(true);
  impl_->acceptor.join();
  for (std::thread& worker : impl_->workers) worker.join();  // the acceptor, which adds them, has stopped
  close_socket(impl_->listener);
}

int LocalServer::port() const { return impl_->port; }

std::vector<SeenRequest> LocalServer::seen() const {
  std::lock_guard lock(impl_->mutex);
  return impl_->seen;
}

int LocalServer::connections() const {
  std::lock_guard lock(impl_->mutex);
  return impl_->connections;
}

void LocalServer::Impl::accept_loop() {
  while (!stopping.load()) {
    if (!readable(listener, 20)) continue;
    const Socket client = ::accept(listener, nullptr, nullptr);
    if (client == kNoSocket) continue;
    std::lock_guard lock(mutex);
    const int number = ++connections;
    workers.emplace_back([this, client, number] {
      faults::exempt_this_thread();  // the server keeps serving while a test's fault is armed
      serve(client, number);
    });
  }
}

void LocalServer::Impl::serve(Socket client, int number) {
  SocketConnection connection(client);
  std::string buffered;
  while (!stopping.load()) {
    SeenRequest request;
    if (!read_request(client, buffered, request)) break;
    request.connection = number;
    {
      std::lock_guard lock(mutex);
      seen.push_back(request);
    }
    if (!handler(connection, request)) break;
  }
  close_socket(client);
}

// Reads until `buffered` holds `wanted_size` bytes; false if the client closed first or the server is stopping.
bool LocalServer::Impl::fill(Socket client, std::string& buffered, std::size_t wanted_size) {
  char buffer[4096];
  while (buffered.size() < wanted_size) {
    if (stopping.load()) return false;
    if (!readable(client, 20)) continue;
    const int n = receive(client, buffer, sizeof buffer);
    if (n <= 0) return false;
    buffered.append(buffer, static_cast<std::size_t>(n));
  }
  return true;
}

bool LocalServer::Impl::read_request(Socket client, std::string& buffered, SeenRequest& request) {
  std::size_t end = 0;
  while ((end = buffered.find("\r\n\r\n")) == std::string::npos) {
    if (!fill(client, buffered, buffered.size() + 1)) return false;
  }
  request.head = buffered.substr(0, end);
  buffered.erase(0, end + 4);
  const std::size_t first_space = request.head.find(' ');
  const std::size_t second_space = request.head.find(' ', first_space == std::string::npos ? 0 : first_space + 1);
  request.method = request.head.substr(0, first_space);
  if (first_space != std::string::npos) {
    request.target = request.head.substr(first_space + 1, second_space - first_space - 1);
  }
  const std::string length_text = request.header("content-length");
  const std::size_t length = length_text.empty() ? 0 : std::stoul(length_text);
  if (!fill(client, buffered, length)) return false;
  request.body = buffered.substr(0, length);
  buffered.erase(0, length);
  return true;
}

struct RawServer::Impl {
  std::string text;
  Socket listener = kNoSocket;
  int port = 0;
  std::atomic<bool> stopping{false};
  std::thread acceptor;

  void accept_loop() {
    while (!stopping.load()) {
      if (!readable(listener, 20)) continue;
      const Socket client = ::accept(listener, nullptr, nullptr);
      if (client == kNoSocket) continue;
      send_all(client, text);
      close_socket(client);
    }
  }
};

RawServer::RawServer(std::string text) : impl_(std::make_unique<Impl>()) {
  impl_->text = std::move(text);
  impl_->listener = listen_loopback(16);
  impl_->port = port_of(impl_->listener);
  impl_->acceptor = std::thread([impl = impl_.get()] {
    faults::exempt_this_thread();
    impl->accept_loop();
  });
}

RawServer::~RawServer() {
  impl_->stopping.store(true);
  impl_->acceptor.join();
  close_socket(impl_->listener);
}

int RawServer::port() const { return impl_->port; }

struct FullBacklog::Impl {
  Socket listener = kNoSocket;
  Socket filler = kNoSocket;
  int port = 0;
};

FullBacklog::FullBacklog() : impl_(std::make_unique<Impl>()) {
  impl_->listener = listen_loopback(0);
  impl_->port = port_of(impl_->listener);
  // The one place in the queue, taken by a connection that is never accepted.
  impl_->filler = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  const sockaddr_in address = loopback_address(impl_->port);
  // SAFETY: as in port_of: an IPv4 address passed as the API's sockaddr*, with its own size.
  if (impl_->filler == kNoSocket ||
      ::connect(impl_->filler, reinterpret_cast<const sockaddr*>(&address), sizeof address) != 0) {
    throw std::runtime_error("the test could not fill the listening queue");
  }
}

FullBacklog::~FullBacklog() {
  close_socket(impl_->filler);
  close_socket(impl_->listener);
}

int FullBacklog::port() const { return impl_->port; }

int unused_port() {
  const Socket s = listen_loopback(1);
  const int port = port_of(s);
  close_socket(s);  // nothing listens there now
  return port;
}

bool running_under_wine() {
#ifdef _WIN32
  // Wine's ntdll exports its version; Windows' does not.
  const HMODULE ntdll = GetModuleHandleA("ntdll.dll");
  return ntdll != nullptr && GetProcAddress(ntdll, "wine_get_version") != nullptr;
#else
  return false;
#endif
}

ModuleOrigin system_module_origin(void* module, std::string_view file_name) {
  ModuleOrigin origin;
#ifdef _WIN32
  if (module == nullptr) return origin;
  const auto utf8 = [](const std::wstring& text) {
    if (text.empty()) return std::string();
    const int size = static_cast<int>(text.size());
    const int length = WideCharToMultiByte(CP_UTF8, 0, text.data(), size, nullptr, 0, nullptr, nullptr);
    std::string out(static_cast<std::size_t>(length), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), size, out.data(), length, nullptr, nullptr);
    return out;
  };
  std::wstring loaded(32768, L'\0');
  loaded.resize(GetModuleFileNameW(static_cast<HMODULE>(module), loaded.data(), static_cast<DWORD>(loaded.size())));
  std::wstring directory(MAX_PATH, L'\0');
  UINT length = GetSystemDirectoryW(directory.data(), static_cast<UINT>(directory.size()));
  if (length >= directory.size()) {
    directory.resize(length);
    length = GetSystemDirectoryW(directory.data(), static_cast<UINT>(directory.size()));
  }
  directory.resize(length < directory.size() ? length : 0);
  const std::wstring expected = directory + L"\\" + std::wstring(file_name.begin(), file_name.end());  // an ASCII name
  origin.loaded = utf8(loaded);
  origin.expected = utf8(expected);
  origin.same = !loaded.empty() && !directory.empty() &&
                CompareStringOrdinal(loaded.c_str(), -1, expected.c_str(), -1, TRUE) == CSTR_EQUAL;
#else
  (void)module;
  (void)file_name;
#endif
  return origin;
}

std::string chunk(std::string_view data) {
  std::string size;
  for (std::size_t n = data.size(); size.empty() || n > 0; n /= 16) {
    size.insert(size.begin(), "0123456789abcdef"[n % 16]);
  }
  return size + "\r\n" + std::string(data) + "\r\n";
}

void Signal::notify() {
  {
    std::lock_guard lock(mutex_);
    notified_ = true;
  }
  changed_.notify_all();
}

bool Signal::wait_for(std::chrono::milliseconds limit) {
  std::unique_lock lock(mutex_);
  return changed_.wait_for(lock, limit, [this] { return notified_; });
}

bool Signal::notified() const {
  std::lock_guard lock(mutex_);
  return notified_;
}

ScopedEnv::ScopedEnv(std::string name, const std::string& value) : name_(std::move(name)) {
  if (const char* old = std::getenv(name_.c_str()); old != nullptr) {
    had_value_ = true;
    old_value_ = old;
  }
#ifdef _WIN32
  _putenv((name_ + "=" + value).c_str());
#else
  ::setenv(name_.c_str(), value.c_str(), 1);
#endif
}

ScopedEnv::~ScopedEnv() {
#ifdef _WIN32
  _putenv((name_ + "=" + (had_value_ ? old_value_ : std::string())).c_str());
#else
  if (had_value_) {
    ::setenv(name_.c_str(), old_value_.c_str(), 1);
  } else {
    ::unsetenv(name_.c_str());
  }
#endif
}

}  // namespace en::llm::test
