// The WinHTTP transport (Windows builds). The factory opens two WinHTTP sessions and shares them with every transport
// it opens, so HTTP/1.1 connections stay alive between requests: one with the system's proxy, for remote hosts, and
// one that never uses a proxy, for loopback, so that a local model server's traffic stays on the machine whatever the
// proxy settings say.
//
// winhttp.dll is never imported: the system directory's own is loaded by its full path and its functions are looked
// up in it, once (transport.hpp says why).
//
// Each exchange runs in WinHTTP's asynchronous mode and has a driver, a thread of its own. The status callback, on
// WinHTTP's threads, only queues what completed and records how far the exchange got; the driver takes every next
// step (receive the response, ask what is there, read it), posts the transport's events and closes the request handle
// at the end, on cancel() or at the connect deadline. So no WinHTTP call on a handle can overlap its closing, or reach
// another request's handle after it (Wine reuses a closed handle's value at once), and the transport's own thread
// never waits inside WinHTTP: Wine's WinHttpReadData can hold its caller until the next chunk of a reply arrives, and
// that holds up only the driver. The exchange's memory (its body, its read buffer, the context the callback sees)
// lives until WINHTTP_CALLBACK_STATUS_HANDLE_CLOSING, the handle's last notification, as WinHTTP's documentation asks,
// and until its driver has finished.
//
// The request's connect timeout covers resolving the name, connecting and the TLS handshake, as libcurl's does: the
// driver fails an exchange whose request has not started to go out by then (SENDING_REQUEST comes after the
// handshake) as a timeout. WinHTTP's own connect timeout stays as a backstop only: on the first run on real Windows it
// did not end a refused connect before the system's retries of it, about 2 s (S8.1.h).
//
// WinHTTP's calls (the status callback) and the drivers' threads are entries from foreign code: each is noexcept and
// starts with ffi_guard (R3), and a fault in one trips the transport's component and fails its exchange, which then
// ends like any other. The drivers run on the net's 2 MB stacks (ARCH §10.2).
#ifdef _WIN32

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winhttp.h>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "en/llm/thread.hpp"
#include "en/llm/transport.hpp"
#include "en/proto/ffi_guard.hpp"

namespace en::llm {

namespace {

using en::proto::Component;
using en::proto::ffi_guard;

// The client's deadlines end an exchange long before this (ARCH §11.4's longest is a minute): it only stops an
// exchange that nothing cancels, such as a server that stalls for good, and leaves a model room to load.
constexpr DWORD kSafetyNetMs = 5 * 60 * 1000;
// WinHTTP reads through an 8 KB buffer of its own; ours is larger, so a read never has to leave data behind.
constexpr DWORD kReadBufferSize = 16 * 1024;
// The completions, the handle's notices and TLS failures, and how far the exchange got: connected, and its request
// starting to go out.
constexpr DWORD kNotifications = WINHTTP_CALLBACK_FLAG_ALL_COMPLETIONS | WINHTTP_CALLBACK_FLAG_HANDLES |
                                 WINHTTP_CALLBACK_FLAG_SECURE_FAILURE | WINHTTP_CALLBACK_FLAG_CONNECT_TO_SERVER |
                                 WINHTTP_CALLBACK_FLAG_SEND_REQUEST;
constexpr const char* kCannotStart = "the HTTP library could not start a request";
constexpr const char* kFaulted = "the transport faulted";

// ---- The system's winhttp.dll ----

// The WinHTTP functions the transport calls, looked up once in the system's own winhttp.dll, which is loaded by its
// full path and never by name (transport.hpp says why). It stays loaded while the program runs: a notification may
// still come after the last transport has gone.
struct WinHttpApi {
  HMODULE module = nullptr;
  decltype(&::WinHttpOpen) open = nullptr;
  decltype(&::WinHttpCloseHandle) close_handle = nullptr;
  decltype(&::WinHttpConnect) connect = nullptr;
  decltype(&::WinHttpOpenRequest) open_request = nullptr;
  decltype(&::WinHttpSetOption) set_option = nullptr;
  decltype(&::WinHttpSetTimeouts) set_timeouts = nullptr;
  decltype(&::WinHttpSetStatusCallback) set_status_callback = nullptr;
  decltype(&::WinHttpAddRequestHeaders) add_request_headers = nullptr;
  decltype(&::WinHttpSendRequest) send_request = nullptr;
  decltype(&::WinHttpReceiveResponse) receive_response = nullptr;
  decltype(&::WinHttpQueryHeaders) query_headers = nullptr;
  decltype(&::WinHttpQueryDataAvailable) query_data_available = nullptr;
  decltype(&::WinHttpReadData) read_data = nullptr;
};

// GetProcAddress's answer as the function it is, by way of void (*)(), the one function type that converts to any
// other without a warning.
template <typename Function>
bool find(HMODULE module, const char* name, Function& function) {
  // SAFETY: GetProcAddress answers the address of the export `name` of the system's winhttp.dll, as a FARPROC.
  // `Function` is that export's own type, taken from its declaration in winhttp.h (decltype), so every call through
  // it matches the function's real signature; a missing export stays null and leaves the transport unbuilt.
  function = reinterpret_cast<Function>(reinterpret_cast<void (*)()>(GetProcAddress(module, name)));
  return function != nullptr;
}

// The system directory's winhttp.dll by its full path, or "" when the directory cannot be read.
std::wstring system_winhttp_path() {
  std::wstring directory(MAX_PATH, L'\0');
  UINT length = GetSystemDirectoryW(directory.data(), static_cast<UINT>(directory.size()));
  if (length >= directory.size()) {  // too small: the size it needs came back instead
    directory.resize(length);
    length = GetSystemDirectoryW(directory.data(), static_cast<UINT>(directory.size()));
  }
  if (length == 0 || length >= directory.size()) return {};
  directory.resize(length);
  return directory + L"\\winhttp.dll";
}

WinHttpApi load_winhttp() {
  const std::wstring path = system_winhttp_path();
  if (path.empty()) return {};
  // A full path, so the loader never searches for the name (the game's folder would come first), and winhttp.dll's
  // own dependencies from the system directory too; a system without that flag takes the older way of saying so.
  HMODULE module = LoadLibraryExW(path.c_str(), nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
  if (module == nullptr && GetLastError() == ERROR_INVALID_PARAMETER) {
    module = LoadLibraryExW(path.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
  }
  if (module == nullptr) return {};
  WinHttpApi api;
  const bool complete =
      find(module, "WinHttpOpen", api.open) && find(module, "WinHttpCloseHandle", api.close_handle) &&
      find(module, "WinHttpConnect", api.connect) && find(module, "WinHttpOpenRequest", api.open_request) &&
      find(module, "WinHttpSetOption", api.set_option) && find(module, "WinHttpSetTimeouts", api.set_timeouts) &&
      find(module, "WinHttpSetStatusCallback", api.set_status_callback) &&
      find(module, "WinHttpAddRequestHeaders", api.add_request_headers) &&
      find(module, "WinHttpSendRequest", api.send_request) &&
      find(module, "WinHttpReceiveResponse", api.receive_response) &&
      find(module, "WinHttpQueryHeaders", api.query_headers) &&
      find(module, "WinHttpQueryDataAvailable", api.query_data_available) &&
      find(module, "WinHttpReadData", api.read_data);
  if (!complete) return {};  // no transport then; the module stays loaded, which does no harm
  api.module = module;
  return api;
}

// Loaded on first use, once, whichever thread comes first.
const WinHttpApi& winhttp() {
  static WinHttpApi api;
  static std::once_flag once;
  std::call_once(once, [] { api = load_winhttp(); });
  return api;
}

// ---- Text ----

int wide_length(std::string_view text) {
  if (text.empty()) return 0;
  return MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
}

void widen_into(std::string_view text, wchar_t* out, int length) {
  if (length > 0) MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), out, length);
}

std::wstring widen(std::string_view text) {
  std::wstring out(static_cast<std::size_t>(wide_length(text)), L'\0');
  widen_into(text, out.data(), static_cast<int>(out.size()));
  return out;
}

std::string narrow(std::wstring_view text) {
  if (text.empty()) return {};
  const int size = static_cast<int>(text.size());
  const int length = WideCharToMultiByte(CP_UTF8, 0, text.data(), size, nullptr, 0, nullptr, nullptr);
  std::string out(static_cast<std::size_t>(length), '\0');
  WideCharToMultiByte(CP_UTF8, 0, text.data(), size, out.data(), length, nullptr, nullptr);
  return out;
}

// "Name: value" in UTF-16, written into one buffer of its final size, so that no copy of a key's value is left in
// memory a reallocation gave back.
std::wstring header_line(std::string_view name, std::string_view value) {
  const int name_length = wide_length(name);
  const int value_length = wide_length(value);
  const auto at = static_cast<std::size_t>(name_length);
  std::wstring line(at + 2 + static_cast<std::size_t>(value_length), L'\0');
  widen_into(name, line.data(), name_length);
  line[at] = L':';
  line[at + 1] = L' ';
  widen_into(value, line.data() + at + 2, value_length);
  return line;
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
  while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r' || s.back() == '\n')) {
    s.remove_suffix(1);
  }
  return s;
}

bool all_digits(std::string_view s) {
  return !s.empty() && std::all_of(s.begin(), s.end(), [](char c) { return c >= '0' && c <= '9'; });
}

// An HTTP token (RFC 9110 §5.6.2), as a method or a header name must be: in particular no ':', space or line break,
// which WinHTTP would read as the start of another header.
bool is_token(std::string_view s) {
  constexpr std::string_view kSeparators = "\"(),/:;<=>?@[\\]{}";
  return !s.empty() && std::all_of(s.begin(), s.end(), [&](char c) {
           return c > ' ' && c < 127 && kSeparators.find(c) == std::string_view::npos;
         });
}

bool is_header_value(std::string_view s) {
  return std::none_of(s.begin(), s.end(), [](char c) { return c == '\r' || c == '\n' || c == '\0'; });
}

bool headers_valid(const HttpRequest& request) {
  for (const HttpHeader& header : request.headers) {
    if (!is_token(header.name) || !is_header_value(header.value)) return false;
  }
  for (const auto& [name, secret] : request.secret_headers) {
    if (!is_token(name) || !is_header_value(secret.reveal())) return false;
  }
  return true;
}

// ---- Addresses ----

struct Target {
  bool secure = false;
  bool loopback = false;
  std::wstring host;  // as WinHttpConnect takes it: an IPv6 address keeps its brackets
  INTERNET_PORT port = 0;
  std::wstring path;  // the path and query; a fragment is never sent
};

// A dotted IPv4 address in 127.0.0.0/8, "localhost" or "[::1]": the hosts that never go through a proxy, as for the
// libcurl transport.
bool is_loopback(std::string_view host) {
  if (host == "localhost" || host == "[::1]") return true;
  int parts = 0;
  bool first_is_127 = false;
  while (true) {
    const std::size_t dot = host.find('.');
    const std::string_view part = host.substr(0, dot);
    if (!all_digits(part) || part.size() > 3) return false;
    if (parts == 0) first_is_127 = part == "127";
    ++parts;
    if (dot == std::string_view::npos) break;
    host.remove_prefix(dot + 1);
  }
  return parts == 4 && first_is_127;
}

// http:// and https:// only, with no user name or password in the address (the libcurl transport also takes only
// these two schemes).
std::optional<Target> parse_target(std::string_view url) {
  const std::size_t scheme_end = url.find("://");
  if (scheme_end == std::string_view::npos) return std::nullopt;
  Target target;
  const std::string scheme = lower(url.substr(0, scheme_end));
  if (scheme == "https") {
    target.secure = true;
  } else if (scheme != "http") {
    return std::nullopt;
  }
  std::string_view rest = url.substr(scheme_end + 3);
  const std::size_t authority_end = rest.find_first_of("/?#");
  const std::string_view authority = rest.substr(0, authority_end);
  std::string_view path = authority_end == std::string_view::npos ? std::string_view() : rest.substr(authority_end);
  path = path.substr(0, path.find('#'));
  if (authority.empty() || authority.find('@') != std::string_view::npos) return std::nullopt;

  std::string_view host = authority;
  std::string_view port;
  if (authority.front() == '[') {
    const std::size_t close = authority.find(']');
    if (close == std::string_view::npos || close < 2) return std::nullopt;
    host = authority.substr(0, close + 1);
    const std::string_view after = authority.substr(close + 1);
    if (!after.empty()) {
      if (after.front() != ':') return std::nullopt;
      port = after.substr(1);
    }
  } else if (const std::size_t colon = authority.find(':'); colon != std::string_view::npos) {
    host = authority.substr(0, colon);
    port = authority.substr(colon + 1);
  }
  if (host.empty()) return std::nullopt;
  unsigned long number = target.secure ? 443 : 80;
  if (!port.empty()) {
    if (!all_digits(port) || port.size() > 5) return std::nullopt;
    number = std::stoul(std::string(port));
    if (number == 0 || number > 65535) return std::nullopt;
  }
  const std::string host_name = lower(host);
  target.loopback = is_loopback(host_name);
  target.host = widen(host_name);
  target.port = static_cast<INTERNET_PORT>(number);
  if (path.empty()) {
    target.path = L"/";
  } else if (path.front() == '?') {
    target.path = L"/" + widen(path);
  } else {
    target.path = widen(path);
  }
  return target;
}

// ---- Failures, in winhttp_failure()'s words (transport_winhttp_failure.cpp) ----

TransportEvent failed_event(int exchange, std::string error) {
  TransportEvent event;
  event.exchange = exchange;
  event.kind = TransportEventKind::Failed;
  event.error = std::move(error);
  return event;
}

// ---- Responses ----

// WINHTTP_QUERY_RAW_HEADERS_CRLF: the status line, then one "Name: value" per line.
std::vector<HttpHeader> parse_header_block(std::string_view block) {
  std::vector<HttpHeader> headers;
  bool status_line = true;
  while (!block.empty()) {
    const std::size_t end = block.find("\r\n");
    const std::string_view line = block.substr(0, end);
    block = end == std::string_view::npos ? std::string_view() : block.substr(end + 2);
    if (status_line) {
      status_line = false;
      continue;
    }
    if (line.empty()) break;
    const std::size_t colon = line.find(':');
    if (colon == std::string_view::npos) continue;
    headers.push_back(HttpHeader{lower(trim(line.substr(0, colon))), std::string(trim(line.substr(colon + 1)))});
  }
  return headers;
}

std::vector<HttpHeader> response_headers(HINTERNET request) {
  DWORD bytes = 0;
  winhttp().query_headers(request, WINHTTP_QUERY_RAW_HEADERS_CRLF, WINHTTP_HEADER_NAME_BY_INDEX,
                          WINHTTP_NO_OUTPUT_BUFFER, &bytes, WINHTTP_NO_HEADER_INDEX);
  if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || bytes == 0) return {};
  std::wstring raw(bytes / sizeof(wchar_t) + 1, L'\0');
  bytes = static_cast<DWORD>(raw.size() * sizeof(wchar_t));
  if (!winhttp().query_headers(request, WINHTTP_QUERY_RAW_HEADERS_CRLF, WINHTTP_HEADER_NAME_BY_INDEX, raw.data(),
                               &bytes, WINHTTP_NO_HEADER_INDEX)) {
    return {};
  }
  raw.resize(bytes / sizeof(wchar_t));
  return parse_header_block(narrow(raw));
}

int response_status(HINTERNET request) {
  DWORD status = 0;
  DWORD size = sizeof status;
  if (!winhttp().query_headers(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                               WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX)) {
    return 0;
  }
  return static_cast<int>(status);
}

// ---- Sessions ----

// The factory's two sessions. Every exchange holds them too, so that a session is closed only after the last request
// handle opened on it has closed.
class Sessions {
 public:
  Sessions() : remote_(open_remote()), direct_(open(WINHTTP_ACCESS_TYPE_NO_PROXY)) {}
  ~Sessions() {
    if (remote_ != nullptr) winhttp().close_handle(remote_);
    if (direct_ != nullptr) winhttp().close_handle(direct_);
  }
  Sessions(const Sessions&) = delete;
  Sessions& operator=(const Sessions&) = delete;

  HINTERNET get(bool loopback) const { return loopback ? direct_ : remote_; }

 private:
  // Windows 8.1 and later find the proxy as the rest of the system does (its settings, a configuration script or
  // detection); older ones refuse the automatic mode and get WinHTTP's own default configuration.
  static HINTERNET open_remote() {
    if (HINTERNET session = open(WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY)) return session;
    return open(WINHTTP_ACCESS_TYPE_DEFAULT_PROXY);
  }

  static HINTERNET open(DWORD access) {
    // No user agent: as with libcurl, nothing names the program but the headers the provider's configuration sets.
    HINTERNET session =
        winhttp().open(nullptr, access, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, WINHTTP_FLAG_ASYNC);
    if (session == nullptr) return nullptr;
    // TLS 1.2 or 1.3, nothing older. A system that does not know the TLS 1.3 flag refuses it and keeps TLS 1.2.
    DWORD protocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2 | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3;
    if (!winhttp().set_option(session, WINHTTP_OPTION_SECURE_PROTOCOLS, &protocols, sizeof protocols)) {
      protocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2;
      if (!winhttp().set_option(session, WINHTTP_OPTION_SECURE_PROTOCOLS, &protocols, sizeof protocols)) {
        winhttp().close_handle(session);
        return nullptr;
      }
    }
    return session;
  }

  HINTERNET remote_;
  HINTERNET direct_;
};

// ---- Exchanges ----

// What a status callback saw, for the exchange's driver to act on.
struct Completion {
  DWORD status = 0;
  DWORD value = 0;   // DATA_AVAILABLE: the bytes there; REQUEST_ERROR: the error; SECURE_FAILURE: what failed
  std::string data;  // READ_COMPLETE: the bytes read
};

// The transport's events, as the drivers post them for wait().
struct Inbox {
  std::mutex mutex;
  std::condition_variable ready;
  std::vector<TransportEvent> events;
};

struct Exchange {
  Exchange(int id_, std::shared_ptr<Inbox> inbox_, std::shared_ptr<Sessions> sessions_, std::string body_)
      : id(id_),
        inbox(std::move(inbox_)),
        sessions(std::move(sessions_)),
        body(std::move(body_)),
        buffer(std::make_unique<char[]>(kReadBufferSize)) {}

  const int id;
  const std::shared_ptr<Inbox> inbox;        // the transport's, which may have gone by the time this ends
  const std::shared_ptr<Sessions> sessions;  // open until this request handle has closed
  std::string body;                          // WinHttpSendRequest's data, which must last until the handle closes
  std::unique_ptr<char[]> buffer;            // WinHttpReadData's target
  HINTERNET connect = nullptr;
  HINTERNET request = nullptr;
  std::shared_ptr<Exchange>* context = nullptr;  // what the callback sees, once it is set; HANDLE_CLOSING deletes it
  bool check_length = true;
  bool https = false;
  std::chrono::steady_clock::time_point connect_by;  // the connect deadline: the request is to start going out by then

  // From the callback to the driver; guarded by mutex.
  std::mutex mutex;
  std::condition_variable wake;
  std::deque<Completion> completions;
  bool connected = false;  // CONNECTED_TO_SERVER: a connection of its own is open (on Wine, only after any handshake)
  bool sending = false;    // SENDING_REQUEST: the request has started to go out, after any TLS handshake
  bool stop = false;       // cancelled: the driver closes the handle and ends
  bool faulted = false;    // a fault in the callback lost a completion: the driver fails the exchange

  // Guarded by inbox->mutex: once set, nothing more of this exchange reaches the inbox.
  bool dropped = false;

  // The driver's own.
  std::optional<std::uint64_t> content_length;
  std::uint64_t received = 0;
  DWORD secure_flags = 0;
};

void post(Exchange& exchange, TransportEvent event) {
  Inbox& inbox = *exchange.inbox;
  {
    std::lock_guard lock(inbox.mutex);
    if (exchange.dropped) return;
    inbox.events.push_back(std::move(event));
  }
  inbox.ready.notify_all();
}

void stop(Exchange& exchange) {
  {
    std::lock_guard lock(exchange.mutex);
    exchange.stop = true;
  }
  exchange.wake.notify_one();
}

// The exchange a notification is for: WinHTTP hands back the context configure() set, as an integer.
std::shared_ptr<Exchange>* link_of(DWORD_PTR context) noexcept {
  // SAFETY: the context is the value configure() set with WINHTTP_OPTION_CONTEXT_VALUE, and send_request() passed,
  // for this request handle: the address of a shared_ptr<Exchange> made with new, which only the handle's last
  // notification (HANDLE_CLOSING) deletes, so it is alive for every notification before that one.
  return reinterpret_cast<std::shared_ptr<Exchange>*>(context);
}

// How far the exchange got, for the driver's connect deadline and its words for a failure. There is nothing to act on,
// so the driver is not woken; CONNECTED_TO_SERVER's information, the server's address, is never read.
void record_progress(Exchange& exchange, DWORD status) {
  std::lock_guard lock(exchange.mutex);
  if (status == WINHTTP_CALLBACK_STATUS_CONNECTED_TO_SERVER) exchange.connected = true;
  if (status == WINHTTP_CALLBACK_STATUS_SENDING_REQUEST) exchange.sending = true;
}

// Queues what completed for the exchange's driver and records how far the exchange got; at HANDLE_CLOSING, frees what
// WinHTTP no longer needs.
void take_status(std::shared_ptr<Exchange>* link, DWORD status, LPVOID info, DWORD length) {
  if (status == WINHTTP_CALLBACK_STATUS_HANDLE_CLOSING) {
    // The handle's last notification: WinHTTP holds nothing of the exchange any more. Its connection handle, kept
    // open as the request's parent until now, goes too.
    if ((*link)->connect != nullptr) winhttp().close_handle((*link)->connect);
    delete link;
    return;
  }
  Completion completion;
  completion.status = status;
  switch (status) {
    case WINHTTP_CALLBACK_STATUS_CONNECTED_TO_SERVER:
    case WINHTTP_CALLBACK_STATUS_SENDING_REQUEST:
      record_progress(**link, status);
      return;
    case WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE:
    case WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE:
      break;
    case WINHTTP_CALLBACK_STATUS_DATA_AVAILABLE:
    case WINHTTP_CALLBACK_STATUS_SECURE_FAILURE:
      // SAFETY: for these two notifications WinHTTP's status information is a DWORD: the bytes available, or the
      // flags of what failed in TLS.
      completion.value = info != nullptr ? *static_cast<const DWORD*>(info) : 0;
      break;
    case WINHTTP_CALLBACK_STATUS_READ_COMPLETE:
      // The bytes are in the exchange's buffer, which no other read uses until the driver has handled this one.
      // SAFETY: for READ_COMPLETE the status information is that buffer, the one read_data() was given, and
      // `length` is how many bytes of it the read filled.
      if (info != nullptr && length > 0) completion.data.assign(static_cast<const char*>(info), length);
      break;
    case WINHTTP_CALLBACK_STATUS_REQUEST_ERROR:
      // SAFETY: for REQUEST_ERROR the status information is a WINHTTP_ASYNC_RESULT, WinHTTP's own structure.
      completion.value = info != nullptr ? static_cast<const WINHTTP_ASYNC_RESULT*>(info)->dwError
                                         : static_cast<DWORD>(ERROR_WINHTTP_INTERNAL_ERROR);
      break;
    default:
      return;  // the other progress notices (connecting, the request sent) need nothing
  }
  Exchange& exchange = **link;
  {
    std::lock_guard lock(exchange.mutex);
    // The request has gone out, should SENDING_REQUEST not have come: the connect deadline no longer applies.
    if (status == WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE) exchange.sending = true;
    exchange.completions.push_back(std::move(completion));
  }
  exchange.wake.notify_one();
}

// A completion a fault lost would leave the driver waiting for it: the driver is told instead, and fails the exchange.
void lost_completion(std::shared_ptr<Exchange>* link) noexcept {
  Exchange& exchange = **link;
  ffi_guard(Component::LlmTransport, [&] {
    std::lock_guard lock(exchange.mutex);
    exchange.faulted = true;
  });
  exchange.wake.notify_one();
}

// WinHTTP's call, on its own threads.
void CALLBACK on_status(HINTERNET, DWORD_PTR context, DWORD status, LPVOID info, DWORD length) noexcept {
  const bool taken = ffi_guard(Component::LlmTransport, false, [&] {
    if (std::shared_ptr<Exchange>* link = link_of(context)) take_status(link, status, info, length);
    return true;
  });
  // HANDLE_CLOSING never throws once it has freed the link, so a fault is always before it.
  if (!taken) {
    if (std::shared_ptr<Exchange>* link = link_of(context)) lost_completion(link);
  }
}

// Drives one exchange on a thread of its own, from the request sent to its last event: once the request is sent, the
// only caller of WinHTTP on its handle, closing included.
class Driver {
 public:
  explicit Driver(std::shared_ptr<Exchange> exchange) : exchange_(std::move(exchange)) {}
  void run();

 private:
  bool step(Completion& completion);  // false once the exchange has ended
  void post_fault() noexcept;
  void send_head();
  bool ask_for_data();
  bool end();
  bool fail(DWORD error);
  bool finish(TransportEvent event);

  std::shared_ptr<Exchange> exchange_;
  WinHttpPhase phase_ = WinHttpPhase::Connecting;  // how far the exchange had got when the driver took its last step
};

// How far the exchange has got, from what the callback recorded; under the exchange's mutex.
WinHttpPhase phase_of(const Exchange& exchange) {
  if (exchange.sending) return WinHttpPhase::Sending;
  return exchange.connected ? WinHttpPhase::Connected : WinHttpPhase::Connecting;
}

void Driver::run() {
  Exchange& ex = *exchange_;
  bool live = true;
  while (live) {
    Completion completion;
    bool faulted = false;
    {
      std::unique_lock lock(ex.mutex);
      const auto ready = [&ex] { return ex.stop || ex.faulted || !ex.completions.empty(); };
      // Until the request starts to go out, the wait ends at the connect deadline too.
      bool timed_out = false;
      if (ex.sending) {
        ex.wake.wait(lock, ready);
      } else if (!ex.wake.wait_until(lock, ex.connect_by, ready)) {
        if (ex.sending) continue;  // it started to go out as the deadline came: no deadline from here on
        timed_out = true;
      }
      if (ex.stop) break;
      phase_ = phase_of(ex);
      faulted = ex.faulted;
      if (timed_out) {
        // The request has not started to go out in time: the exchange fails as WinHTTP's own connect timeout would
        // fail it, and its handle closes below, which cancels it, as after cancel().
        completion.status = WINHTTP_CALLBACK_STATUS_REQUEST_ERROR;
        completion.value = ERROR_WINHTTP_TIMEOUT;
      } else if (!faulted) {
        completion = std::move(ex.completions.front());
        ex.completions.pop_front();
      }
    }
    if (faulted) {  // the callback lost a completion, so no next step can come: the exchange fails now
      post_fault();
      break;
    }
    // R3: a fault in a step (out of memory, in practice) trips the transport's component and fails the exchange.
    const std::optional<bool> next =
        ffi_guard(Component::LlmTransport, std::optional<bool>(), [&] { return std::optional<bool>(step(completion)); });
    if (!next) post_fault();
    live = next.value_or(false);
  }
  // No other thread calls WinHTTP on this handle, so nothing overlaps its closing; HANDLE_CLOSING frees the rest.
  winhttp().close_handle(ex.request);
}

// The exchange's last event after a fault: a failure, so that it ends like any other. If even that cannot be posted,
// the client's own deadline ends it.
void Driver::post_fault() noexcept {
  ffi_guard(Component::LlmTransport, [this] { post(*exchange_, failed_event(exchange_->id, kFaulted)); });
}

bool Driver::step(Completion& completion) {
  Exchange& ex = *exchange_;
  switch (completion.status) {
    case WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE:
      if (!winhttp().receive_response(ex.request, nullptr)) return fail(GetLastError());
      return true;
    case WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE:
      send_head();
      return ask_for_data();
    case WINHTTP_CALLBACK_STATUS_DATA_AVAILABLE:
      // Reading only what is there completes at once, so each piece reaches the client as it arrived.
      if (completion.value == 0) return end();
      if (!winhttp().read_data(ex.request, ex.buffer.get(), std::min(completion.value, kReadBufferSize), nullptr)) {
        return fail(GetLastError());
      }
      return true;
    case WINHTTP_CALLBACK_STATUS_READ_COMPLETE: {
      if (completion.data.empty()) return end();
      ex.received += completion.data.size();
      TransportEvent event;
      event.exchange = ex.id;
      event.kind = TransportEventKind::Data;
      event.data = std::move(completion.data);
      post(ex, std::move(event));
      return ask_for_data();
    }
    case WINHTTP_CALLBACK_STATUS_SECURE_FAILURE:
      ex.secure_flags |= completion.value;  // the REQUEST_ERROR that follows says TLS
      return true;
    case WINHTTP_CALLBACK_STATUS_REQUEST_ERROR:
      return fail(completion.value);
    default:
      return true;
  }
}

void Driver::send_head() {
  Exchange& ex = *exchange_;
  TransportEvent event;
  event.exchange = ex.id;
  event.kind = TransportEventKind::Head;
  event.status = response_status(ex.request);
  event.headers = response_headers(ex.request);
  // A body shorter than its Content-Length is a failure, as libcurl reports it; 204 and 304 have none to check.
  bool chunked = false;
  for (const HttpHeader& header : event.headers) {
    if (header.name == "content-length" && all_digits(header.value) && header.value.size() < 19) {
      ex.content_length = std::stoull(header.value);
    } else if (header.name == "transfer-encoding" && lower(header.value).find("chunked") != std::string::npos) {
      chunked = true;
    }
  }
  if (chunked || event.status == 204 || event.status == 304 || !ex.check_length) ex.content_length.reset();
  post(ex, std::move(event));
}

bool Driver::ask_for_data() {
  if (!winhttp().query_data_available(exchange_->request, nullptr)) return fail(GetLastError());
  return true;
}

bool Driver::end() {
  Exchange& ex = *exchange_;
  if (ex.content_length && ex.received < *ex.content_length) {
    return finish(failed_event(ex.id, "the connection closed before the reply ended"));
  }
  TransportEvent done;
  done.exchange = ex.id;
  done.kind = TransportEventKind::Done;
  return finish(std::move(done));
}

bool Driver::fail(DWORD error) {
  const Exchange& ex = *exchange_;
  return finish(failed_event(ex.id, winhttp_failure(error, phase_, ex.https, ex.secure_flags)));
}

bool Driver::finish(TransportEvent event) {
  post(*exchange_, std::move(event));
  return false;
}

void drive(std::shared_ptr<Exchange> exchange) { Driver(std::move(exchange)).run(); }

int connect_timeout_ms(std::chrono::milliseconds timeout) {
  // WinHTTP reads 0 as "for ever": a request without a connect timeout of its own gets the safety net.
  if (timeout.count() <= 0) return static_cast<int>(kSafetyNetMs);
  return static_cast<int>(std::min<long long>(timeout.count(), kSafetyNetMs));
}

// The request handle's options, then the context and the callback. Any that is refused ends the exchange before it
// is sent.
bool configure(const std::shared_ptr<Exchange>& exchange, const HttpRequest& request) {
  HINTERNET handle = exchange->request;
  // A redirect would carry the key to another address; cookies, and logging on with the player's Windows account,
  // have no place in a model request.
  DWORD features = WINHTTP_DISABLE_COOKIES | WINHTTP_DISABLE_REDIRECTS;
  if (!winhttp().set_option(handle, WINHTTP_OPTION_DISABLE_FEATURE, &features, sizeof features)) return false;
  DWORD logon = WINHTTP_AUTOLOGON_SECURITY_LEVEL_HIGH;
  if (!winhttp().set_option(handle, WINHTTP_OPTION_AUTOLOGON_POLICY, &logon, sizeof logon)) return false;
  // Resolving the name and connecting take the request's connect timeout, a backstop for the driver's own connect
  // deadline (Driver::run); sending and receiving only the safety net, since the client's own deadlines cancel first.
  const int connect = connect_timeout_ms(request.connect_timeout);
  const int net = static_cast<int>(kSafetyNetMs);
  if (!winhttp().set_timeouts(handle, connect, connect, net, net)) return false;
  DWORD headers_wait = kSafetyNetMs;  // WinHTTP's own default (90 s) would also outlast the client's deadlines
  winhttp().set_option(handle, WINHTTP_OPTION_RECEIVE_RESPONSE_TIMEOUT, &headers_wait, sizeof headers_wait);

  auto link = std::make_unique<std::shared_ptr<Exchange>>(exchange);
  // SAFETY: WinHTTP keeps the context as an integer and hands it back to on_status, which turns it back into this
  // pointer (link_of); the link lives until HANDLE_CLOSING deletes it.
  DWORD_PTR context = reinterpret_cast<DWORD_PTR>(link.get());
  if (!winhttp().set_option(handle, WINHTTP_OPTION_CONTEXT_VALUE, &context, sizeof context)) return false;
  if (winhttp().set_status_callback(handle, &on_status, kNotifications, 0) == WINHTTP_INVALID_STATUS_CALLBACK) {
    return false;
  }
  exchange->context = link.release();  // from here on, HANDLE_CLOSING frees it
  return true;
}

bool add_headers(Exchange& exchange, const HttpRequest& request) {
  for (const HttpHeader& header : request.headers) {
    const std::wstring line = header_line(header.name, header.value);
    if (!winhttp().add_request_headers(exchange.request, line.c_str(), static_cast<DWORD>(line.size()),
                                       WINHTTP_ADDREQ_FLAG_ADD)) {
      return false;
    }
  }
  // A key is revealed only here, just before the request is sent: WinHTTP keeps its own copy for the request's life,
  // and ours is cleared at once.
  for (const auto& [name, secret] : request.secret_headers) {
    std::wstring line = header_line(name, secret.reveal());
    const BOOL added = winhttp().add_request_headers(exchange.request, line.c_str(),
                                                     static_cast<DWORD>(line.size()), WINHTTP_ADDREQ_FLAG_ADD);
    SecureZeroMemory(line.data(), line.size() * sizeof(wchar_t));
    if (!added) return false;
  }
  return true;
}

// Closes a request that no driver runs, because it failed before or as it was sent. With the callback set,
// HANDLE_CLOSING frees the context and closes the connection handle; without it, nothing of WinHTTP refers to the
// exchange.
void abandon(Exchange& exchange) {
  const bool callback_set = exchange.context != nullptr;
  HINTERNET connect = exchange.connect;
  if (exchange.request != nullptr) winhttp().close_handle(exchange.request);
  if (!callback_set && connect != nullptr) winhttp().close_handle(connect);
}

class WinHttpTransport final : public Transport {
 public:
  explicit WinHttpTransport(std::shared_ptr<Sessions> sessions)
      : sessions_(std::move(sessions)), inbox_(std::make_shared<Inbox>()) {}

  // The drivers still running are told to stop; they and WinHTTP's notifications hold what they need until they end.
  ~WinHttpTransport() override {
    for (auto& entry : exchanges_) drop(*entry.second);
  }

  WinHttpTransport(const WinHttpTransport&) = delete;
  WinHttpTransport& operator=(const WinHttpTransport&) = delete;

  int start(const HttpRequest& request) override;
  void cancel(int exchange) override;
  void wait(std::chrono::milliseconds max_wait, std::vector<TransportEvent>& out) override;

 private:
  int failed_at_start(int id, std::string error);
  void drop(Exchange& exchange);

  std::shared_ptr<Sessions> sessions_;
  std::shared_ptr<Inbox> inbox_;
  std::map<int, std::shared_ptr<Exchange>> exchanges_;  // started, and not yet ended as far as wait() has seen
  int next_id_ = 1;
};

int WinHttpTransport::start(const HttpRequest& request) {
  const int id = next_id_++;
  const std::optional<Target> target = parse_target(request.url);
  if (!target) return failed_at_start(id, "the address is not a plain http:// or https:// URL");
  if (!is_token(request.method)) return failed_at_start(id, "the request method is not valid");
  if (!headers_valid(request)) return failed_at_start(id, "a request header is malformed");
  if (request.body.size() > 0xFFFFFFFFu) return failed_at_start(id, "the request body is too large");
  HINTERNET session = sessions_->get(target->loopback);
  if (session == nullptr) return failed_at_start(id, kCannotStart);

  auto exchange = std::make_shared<Exchange>(id, inbox_, sessions_, request.body);
  exchange->check_length = request.method != "HEAD";
  exchange->https = target->secure;
  exchange->connect = winhttp().connect(session, target->host.c_str(), target->port, 0);
  if (exchange->connect != nullptr) {
    exchange->request =
        winhttp().open_request(exchange->connect, widen(request.method).c_str(), target->path.c_str(), nullptr,
                               WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                               target->secure ? WINHTTP_FLAG_SECURE : 0);
  }
  if (exchange->request == nullptr || !configure(exchange, request) || !add_headers(*exchange, request)) {
    abandon(*exchange);
    return failed_at_start(id, kCannotStart);
  }
  // The body is the exchange's own copy, which lasts until the handle has closed, as WinHTTP needs.
  void* data = exchange->body.empty() ? WINHTTP_NO_REQUEST_DATA : exchange->body.data();
  const auto size = static_cast<DWORD>(exchange->body.size());
  // SAFETY: the same context as configure() set, as the integer WinHTTP takes; on_status turns it back (link_of).
  const auto context = reinterpret_cast<DWORD_PTR>(exchange->context);
  // Resolving the name, connecting and any TLS handshake are to be done by the connect deadline (Driver::run).
  exchange->connect_by =
      std::chrono::steady_clock::now() + std::chrono::milliseconds(connect_timeout_ms(request.connect_timeout));
  if (!winhttp().send_request(exchange->request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, data, size, size, context)) {
    const DWORD error = GetLastError();
    abandon(*exchange);
    return failed_at_start(id, winhttp_failure(error, WinHttpPhase::Connecting, target->secure, 0));
  }
  try {
    // The driver holds the exchange until it has finished, on a thread of the net's stack size (ARCH §10.2).
    StackThread(Component::LlmTransport, kNetStack, [exchange] { drive(exchange); }).detach();
  } catch (const std::exception&) {  // the system refused the thread, or its memory
    abandon(*exchange);  // no driver, so this thread may still close the handle
    return failed_at_start(id, kCannotStart);
  }
  exchanges_.emplace(id, std::move(exchange));
  return id;
}

void WinHttpTransport::cancel(int exchange) {
  std::shared_ptr<Exchange> running;
  if (auto it = exchanges_.find(exchange); it != exchanges_.end()) {
    running = std::move(it->second);
    exchanges_.erase(it);
  }
  {
    std::lock_guard lock(inbox_->mutex);
    if (running) running->dropped = true;
    std::erase_if(inbox_->events, [exchange](const TransportEvent& e) { return e.exchange == exchange; });
  }
  if (running) stop(*running);
}

void WinHttpTransport::wait(std::chrono::milliseconds max_wait, std::vector<TransportEvent>& out) {
  std::vector<TransportEvent> events;
  {
    std::unique_lock lock(inbox_->mutex);
    inbox_->ready.wait_for(lock, max_wait, [this] { return !inbox_->events.empty(); });
    events.swap(inbox_->events);
  }
  for (TransportEvent& event : events) {
    if (event.kind == TransportEventKind::Done || event.kind == TransportEventKind::Failed) {
      exchanges_.erase(event.exchange);
    }
    out.push_back(std::move(event));
  }
}

int WinHttpTransport::failed_at_start(int id, std::string error) {
  {
    std::lock_guard lock(inbox_->mutex);
    inbox_->events.push_back(failed_event(id, std::move(error)));
  }
  inbox_->ready.notify_all();
  return id;
}

void WinHttpTransport::drop(Exchange& exchange) {
  {
    std::lock_guard lock(inbox_->mutex);
    exchange.dropped = true;
  }
  stop(exchange);
}

class WinHttpFactory final : public TransportFactory {
 public:
  WinHttpFactory() : sessions_(std::make_shared<Sessions>()) {}
  std::unique_ptr<Transport> open() override { return std::make_unique<WinHttpTransport>(sessions_); }

 private:
  std::shared_ptr<Sessions> sessions_;
};

}  // namespace

std::shared_ptr<TransportFactory> make_winhttp_transport_factory() {
  // Without the system's winhttp.dll, or with a function missing from it, there is no transport, as in a build
  // without one.
  if (winhttp().module == nullptr) return nullptr;
  return std::make_shared<WinHttpFactory>();
}

void* winhttp_module() { return winhttp().module; }

}  // namespace en::llm

#endif  // _WIN32
