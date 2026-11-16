#include "en/llm/transport.hpp"

namespace en::llm {

#ifndef EN_WITH_CURL
std::shared_ptr<TransportFactory> make_curl_transport_factory() { return nullptr; }
#endif

#ifndef _WIN32
std::shared_ptr<TransportFactory> make_winhttp_transport_factory() { return nullptr; }
void* winhttp_module() { return nullptr; }
#endif

std::shared_ptr<TransportFactory> make_default_transport_factory() {
#if defined(_WIN32)
  // The system's own HTTP stack: its proxy settings, certificate store and TLS, with nothing to ship beside it.
  return make_winhttp_transport_factory();
#elif defined(EN_WITH_CURL)
  return make_curl_transport_factory();
#else
  return nullptr;
#endif
}

}  // namespace en::llm
