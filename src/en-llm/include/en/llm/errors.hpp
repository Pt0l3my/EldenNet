#pragma once
// Error classes (ARCH §11.2) and the client's own outcomes: the enum is en-proto's (llm.hpp), shared with the core;
// what a class means for health and which class an HTTP status gives are the client's.
#include <cstdint>
#include <string_view>

#include "en/proto/llm.hpp"

namespace en::llm {

using en::proto::ErrorClass;
using en::proto::to_string;

// Whether a failure of this class adds to its (provider, model) pair's count (ARCH §11.2's last column).
bool counts_in_health(ErrorClass e);

// The class an HTTP status gives (2xx gives None).
ErrorClass classify_http_status(int status);

}  // namespace en::llm
