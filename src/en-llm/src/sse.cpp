#include "en/llm/sse.hpp"

#include <algorithm>

namespace en::llm {

bool SseParser::feed(std::string_view bytes, std::vector<SseEvent>& out) {
  if (failed_) return false;
  std::size_t pos = 0;
  while (pos < bytes.size()) {
    if (after_cr_) {
      after_cr_ = false;
      if (bytes[pos] == '\n') {
        ++pos;
        continue;
      }
    }
    const std::size_t end = bytes.find_first_of("\r\n", pos);
    const std::size_t stop = end == std::string_view::npos ? bytes.size() : end;
    line_.append(bytes.substr(pos, stop - pos));
    if (line_.size() > max_line_) {
      failed_ = true;
      line_.clear();
      return false;
    }
    if (end == std::string_view::npos) break;
    after_cr_ = bytes[end] == '\r';
    line(out);
    pos = end + 1;
  }
  return true;
}

void SseParser::finish(std::vector<SseEvent>& out) {
  if (failed_) return;
  if (!line_.empty()) line(out);
  dispatch(out);
}

void SseParser::line(std::vector<SseEvent>& out) {
  std::string_view text = line_;
  if (first_line_) {
    first_line_ = false;
    if (text.substr(0, 3) == "\xEF\xBB\xBF") text.remove_prefix(3);
  }
  if (text.empty()) {
    dispatch(out);
  } else if (text.front() != ':') {  // a line opening with ':' is a comment, such as a keep-alive
    const std::size_t colon = text.find(':');
    const std::string_view field = text.substr(0, colon);
    std::string_view value = colon == std::string_view::npos ? std::string_view{} : text.substr(colon + 1);
    if (!value.empty() && value.front() == ' ') value.remove_prefix(1);
    if (field == "data") {
      if (has_data_) data_.push_back('\n');
      data_.append(value);
      has_data_ = true;
    } else if (field == "event") {
      type_.assign(value);
    } else if (field == "id") {
      if (value.find('\0') == std::string_view::npos) last_id_.assign(value);
    } else if (field == "retry") {
      if (!value.empty() && value.size() < 10 &&
          std::all_of(value.begin(), value.end(), [](char c) { return c >= '0' && c <= '9'; })) {
        retry_ms_ = std::stoi(std::string(value));
      }
    }
    // Other fields are ignored, as the spec says.
  }
  line_.clear();
}

void SseParser::dispatch(std::vector<SseEvent>& out) {
  if (has_data_) {
    SseEvent event;
    event.type = type_.empty() ? "message" : type_;
    event.data = std::move(data_);
    event.id = last_id_;
    out.push_back(std::move(event));
  }
  data_.clear();
  has_data_ = false;
  type_.clear();
}

}  // namespace en::llm
