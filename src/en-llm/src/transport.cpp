#include "en/llm/transport.hpp"

namespace en::llm {

#ifndef EN_WITH_CURL
std::shared_ptr<TransportFactory> make_curl_transport_factory() { return nullptr; }
#endif

std::shared_ptr<TransportFactory> make_default_transport_factory() {
#ifdef EN_WITH_CURL
  return make_curl_transport_factory();
#else
  // The Windows build's WinHTTP transport is later work; it implements the same TransportFactory.
  return nullptr;
#endif
}

}  // namespace en::llm
