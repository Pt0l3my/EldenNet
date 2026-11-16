// The libcurl transport (EN_WITH_CURL). One multi handle per request runs its exchanges (the request and a backup);
// one share handle across requests keeps HTTP/1.1 connections alive between them, with DNS and TLS sessions.
#ifdef EN_WITH_CURL

#include <curl/curl.h>

#include <array>
#include <cstdlib>
#include <cstring>
#include <map>
#include <mutex>
#include <thread>

#include "en/llm/transport.hpp"

namespace en::llm {

namespace {

class CurlShare {
 public:
  CurlShare() {
    static std::once_flag once;
    std::call_once(once, [] { curl_global_init(CURL_GLOBAL_DEFAULT); });
    share_ = curl_share_init();
    curl_share_setopt(share_, CURLSHOPT_LOCKFUNC, &CurlShare::lock);
    curl_share_setopt(share_, CURLSHOPT_UNLOCKFUNC, &CurlShare::unlock);
    curl_share_setopt(share_, CURLSHOPT_USERDATA, this);
    curl_share_setopt(share_, CURLSHOPT_SHARE, CURL_LOCK_DATA_CONNECT);
    curl_share_setopt(share_, CURLSHOPT_SHARE, CURL_LOCK_DATA_DNS);
    curl_share_setopt(share_, CURLSHOPT_SHARE, CURL_LOCK_DATA_SSL_SESSION);
  }
  ~CurlShare() { curl_share_cleanup(share_); }
  CurlShare(const CurlShare&) = delete;
  CurlShare& operator=(const CurlShare&) = delete;

  CURLSH* get() const { return share_; }

 private:
  static void lock(CURL*, curl_lock_data data, curl_lock_access, void* self) {
    static_cast<CurlShare*>(self)->mutexes_[static_cast<std::size_t>(data)].lock();
  }
  static void unlock(CURL*, curl_lock_data data, void* self) {
    static_cast<CurlShare*>(self)->mutexes_[static_cast<std::size_t>(data)].unlock();
  }

  CURLSH* share_ = nullptr;
  std::array<std::mutex, CURL_LOCK_DATA_LAST> mutexes_;
};

// Loopback never goes through a proxy: a local model server's traffic stays on the machine.
const char* no_proxy_hosts() {
  static const std::string hosts = [] {
    std::string list;
    for (const char* name : {"NO_PROXY", "no_proxy"}) {
      if (const char* value = std::getenv(name); value != nullptr && *value != '\0') {
        list = value;
        break;
      }
    }
    if (!list.empty()) list += ",";
    return list + "localhost,127.0.0.0/8,::1";
  }();
  return hosts.c_str();
}

std::string lower(std::string s) {
  for (char& c : s) {
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
  }
  return s;
}

std::string trim(std::string_view s) {
  while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.remove_prefix(1);
  while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r' || s.back() == '\n')) s.remove_suffix(1);
  return std::string(s);
}

class CurlTransport final : public Transport {
 public:
  explicit CurlTransport(std::shared_ptr<CurlShare> share) : share_(std::move(share)), multi_(curl_multi_init()) {}

  ~CurlTransport() override {
    for (auto& [id, exchange] : exchanges_) release(*exchange);
    curl_multi_cleanup(multi_);
  }

  int start(const HttpRequest& request) override;
  void cancel(int exchange) override;
  void wait(std::chrono::milliseconds max_wait, std::vector<TransportEvent>& out) override;

 private:
  struct Exchange {
    CurlTransport* owner = nullptr;
    int id = 0;
    CURL* easy = nullptr;
    curl_slist* headers = nullptr;
    std::string body;
    int status = 0;
    std::vector<HttpHeader> response_headers;
    bool head_sent = false;
  };

  static std::size_t on_header(char* data, std::size_t size, std::size_t count, void* user);
  static std::size_t on_body(char* data, std::size_t size, std::size_t count, void* user);
  void send_head(Exchange& exchange);
  void drain();
  void release(Exchange& exchange);

  std::shared_ptr<CurlShare> share_;
  CURLM* multi_;
  std::map<int, std::unique_ptr<Exchange>> exchanges_;
  std::vector<TransportEvent> pending_;
  int next_id_ = 1;
};

int CurlTransport::start(const HttpRequest& request) {
  auto owned = std::make_unique<Exchange>();
  Exchange& ex = *owned;
  ex.owner = this;
  ex.id = next_id_++;
  ex.body = request.body;
  ex.easy = curl_easy_init();
  if (ex.easy == nullptr || multi_ == nullptr) {
    TransportEvent failed;
    failed.exchange = ex.id;
    failed.kind = TransportEventKind::Failed;
    failed.error = "the HTTP library could not start a request";
    pending_.push_back(std::move(failed));
    if (ex.easy != nullptr) curl_easy_cleanup(ex.easy);
    return ex.id;
  }
  CURL* e = ex.easy;
  curl_easy_setopt(e, CURLOPT_URL, request.url.c_str());
  curl_easy_setopt(e, CURLOPT_SHARE, share_->get());
  curl_easy_setopt(e, CURLOPT_PRIVATE, &ex);
  curl_easy_setopt(e, CURLOPT_HTTP_VERSION, static_cast<long>(CURL_HTTP_VERSION_1_1));
  curl_easy_setopt(e, CURLOPT_NOSIGNAL, 1L);
  curl_easy_setopt(e, CURLOPT_CONNECTTIMEOUT_MS, static_cast<long>(request.connect_timeout.count()));
  curl_easy_setopt(e, CURLOPT_TCP_KEEPALIVE, 1L);
  curl_easy_setopt(e, CURLOPT_SUPPRESS_CONNECT_HEADERS, 1L);
  curl_easy_setopt(e, CURLOPT_NOPROXY, no_proxy_hosts());
  curl_easy_setopt(e, CURLOPT_FOLLOWLOCATION, 0L);  // a key is never carried to another address
#if LIBCURL_VERSION_NUM >= 0x075500
  curl_easy_setopt(e, CURLOPT_PROTOCOLS_STR, "http,https");
#endif
  curl_easy_setopt(e, CURLOPT_HEADERFUNCTION, &CurlTransport::on_header);
  curl_easy_setopt(e, CURLOPT_HEADERDATA, &ex);
  curl_easy_setopt(e, CURLOPT_WRITEFUNCTION, &CurlTransport::on_body);
  curl_easy_setopt(e, CURLOPT_WRITEDATA, &ex);
  if (request.method == "GET") {
    curl_easy_setopt(e, CURLOPT_HTTPGET, 1L);
  } else {
    curl_easy_setopt(e, CURLOPT_POST, 1L);
    curl_easy_setopt(e, CURLOPT_POSTFIELDS, ex.body.data());
    curl_easy_setopt(e, CURLOPT_POSTFIELDSIZE_LARGE, static_cast<curl_off_t>(ex.body.size()));
  }
  // "Expect: 100-continue" would hold a body over 1 KB back for up to a second.
  ex.headers = curl_slist_append(ex.headers, "Expect:");
  for (const HttpHeader& header : request.headers) {
    ex.headers = curl_slist_append(ex.headers, (header.name + ": " + header.value).c_str());
  }
  for (const auto& [name, secret] : request.secret_headers) {
    std::string line = name + ": " + secret.reveal();
    ex.headers = curl_slist_append(ex.headers, line.c_str());
    volatile char* p = line.data();
    for (std::size_t i = 0; i < line.size(); ++i) p[i] = '\0';
  }
  curl_easy_setopt(e, CURLOPT_HTTPHEADER, ex.headers);
  curl_multi_add_handle(multi_, e);
  const int id = ex.id;
  exchanges_.emplace(id, std::move(owned));
  return id;
}

void CurlTransport::cancel(int exchange) {
  auto it = exchanges_.find(exchange);
  if (it != exchanges_.end()) {
    release(*it->second);
    exchanges_.erase(it);
  }
  std::erase_if(pending_, [exchange](const TransportEvent& e) { return e.exchange == exchange; });
}

void CurlTransport::wait(std::chrono::milliseconds max_wait, std::vector<TransportEvent>& out) {
  int running = 0;
  curl_multi_perform(multi_, &running);
  drain();
  if (pending_.empty()) {
    if (exchanges_.empty()) {
      std::this_thread::sleep_for(max_wait);
    } else {
      curl_multi_poll(multi_, nullptr, 0, static_cast<int>(max_wait.count()), nullptr);
      curl_multi_perform(multi_, &running);
      drain();
    }
  }
  for (TransportEvent& event : pending_) out.push_back(std::move(event));
  pending_.clear();
}

std::size_t CurlTransport::on_header(char* data, std::size_t size, std::size_t count, void* user) {
  auto* ex = static_cast<Exchange*>(user);
  const std::size_t n = size * count;
  const std::string line = trim(std::string_view(data, n));
  if (line.rfind("HTTP/", 0) == 0) {  // a status line opens a header block (a 100 Continue has its own)
    const std::size_t space = line.find(' ');
    ex->status = space == std::string::npos ? 0 : std::atoi(line.c_str() + space + 1);
    ex->response_headers.clear();
  } else if (line.empty()) {
    if (ex->status >= 200) ex->owner->send_head(*ex);
  } else if (const std::size_t colon = line.find(':'); colon != std::string::npos) {
    ex->response_headers.push_back(HttpHeader{lower(line.substr(0, colon)), trim(std::string_view(line).substr(colon + 1))});
  }
  return n;
}

std::size_t CurlTransport::on_body(char* data, std::size_t size, std::size_t count, void* user) {
  auto* ex = static_cast<Exchange*>(user);
  const std::size_t n = size * count;
  if (!ex->head_sent) ex->owner->send_head(*ex);
  TransportEvent event;
  event.exchange = ex->id;
  event.kind = TransportEventKind::Data;
  event.data.assign(data, n);
  ex->owner->pending_.push_back(std::move(event));
  return n;
}

void CurlTransport::send_head(Exchange& ex) {
  if (ex.head_sent) return;
  ex.head_sent = true;
  if (ex.status == 0) {
    long code = 0;
    curl_easy_getinfo(ex.easy, CURLINFO_RESPONSE_CODE, &code);
    ex.status = static_cast<int>(code);
  }
  TransportEvent event;
  event.exchange = ex.id;
  event.kind = TransportEventKind::Head;
  event.status = ex.status;
  event.headers = ex.response_headers;
  pending_.push_back(std::move(event));
}

void CurlTransport::drain() {
  int left = 0;
  while (CURLMsg* message = curl_multi_info_read(multi_, &left)) {
    if (message->msg != CURLMSG_DONE) continue;
    const CURLcode result = message->data.result;
    char* priv = nullptr;
    curl_easy_getinfo(message->easy_handle, CURLINFO_PRIVATE, &priv);
    auto* ex = reinterpret_cast<Exchange*>(priv);
    if (ex == nullptr) continue;
    TransportEvent event;
    event.exchange = ex->id;
    if (result == CURLE_OK) {
      send_head(*ex);
      event.kind = TransportEventKind::Done;
    } else {
      // curl's own wording only: its error buffer can name local addresses, which logs must not carry.
      event.kind = TransportEventKind::Failed;
      event.error = curl_easy_strerror(result);
    }
    pending_.push_back(std::move(event));
    const int id = ex->id;
    release(*ex);
    exchanges_.erase(id);
  }
}

void CurlTransport::release(Exchange& ex) {
  if (ex.easy != nullptr) {
    curl_multi_remove_handle(multi_, ex.easy);
    curl_easy_cleanup(ex.easy);
    ex.easy = nullptr;
  }
  // The header lines held a key: clear them before the memory goes back.
  for (curl_slist* item = ex.headers; item != nullptr; item = item->next) {
    volatile char* p = item->data;
    for (std::size_t i = 0; p[i] != '\0'; ++i) p[i] = '\0';
  }
  curl_slist_free_all(ex.headers);
  ex.headers = nullptr;
}

class CurlFactory final : public TransportFactory {
 public:
  CurlFactory() : share_(std::make_shared<CurlShare>()) {}
  std::unique_ptr<Transport> open() override { return std::make_unique<CurlTransport>(share_); }

 private:
  std::shared_ptr<CurlShare> share_;
};

}  // namespace

std::shared_ptr<TransportFactory> make_curl_transport_factory() { return std::make_shared<CurlFactory>(); }

}  // namespace en::llm

#endif  // EN_WITH_CURL
