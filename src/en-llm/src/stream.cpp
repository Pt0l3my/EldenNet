#include "en/llm/stream.hpp"

#include <algorithm>
#include <array>
#include <cmath>

#include "en/llm/secret.hpp"
#include "en/proto/json.hpp"

namespace en::llm {

using nlohmann::json;

namespace {

constexpr std::string_view kOpen = "<think>";
constexpr std::string_view kClose = "</think>";
constexpr std::string_view kEllipsis = "\xE2\x80\xA6";
constexpr std::array<std::string_view, 3> kWideClosers = {"\xE2\x80\x9D", "\xE2\x80\x99", "\xC2\xBB"};  // ” ’ »

bool is_space(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }

std::string_view trim(std::string_view s) {
  while (!s.empty() && is_space(s.front())) s.remove_prefix(1);
  while (!s.empty() && is_space(s.back())) s.remove_suffix(1);
  return s;
}

std::string ascii_lower(std::string_view s) {
  std::string out(s);
  for (char& c : out) {
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
  }
  return out;
}

// The longest suffix of `text` that could be the start of `tag` (a tag split across two chunks).
std::size_t partial_suffix(std::string_view text, std::string_view tag) {
  for (std::size_t n = std::min(text.size(), tag.size() - 1); n > 0; --n) {
    if (text.substr(text.size() - n) == tag.substr(0, n)) return n;
  }
  return 0;
}

// The length of a closing quote or bracket at `pos`, or 0.
std::size_t closer_at(std::string_view t, std::size_t pos) {
  const char c = t[pos];
  if (c == '"' || c == '\'' || c == ')' || c == ']' || c == '*') return 1;
  for (std::string_view wide : kWideClosers) {
    if (t.substr(pos, wide.size()) == wide) return wide.size();
  }
  return 0;
}

int int_field(const json& object, const char* key) {
  auto it = object.find(key);
  if (it == object.end() || !it->is_number_integer()) return -1;
  return it->get<int>();
}

// A cost the provider reported (OpenRouter's `usage.cost`): a finite number, 0 or more, below a sanity bound; else -1.
double cost_field(const json& object) {
  auto it = object.find("cost");
  if (it == object.end() || !it->is_number()) return -1;
  const double cost = it->get<double>();
  return std::isfinite(cost) && cost >= 0 && cost < 1e6 ? cost : -1;
}

// The longest prefix of `text` of at most `max` bytes that ends at a character's boundary.
std::size_t utf8_prefix(std::string_view text, std::size_t max) {
  if (text.size() <= max) return text.size();
  std::size_t cut = max;
  while (cut > 0 && (static_cast<unsigned char>(text[cut]) & 0xC0) == 0x80) --cut;  // never split a character
  return cut;
}

std::string utf8_cut(std::string s, std::size_t max) {
  if (s.size() <= max) return s;
  s.resize(utf8_prefix(s, max));
  return s + "...";
}

// Why a chunk or body was refused before parsing, in our own words.
std::string refusal_text(std::string_view what, en::proto::JsonRefusal refusal) {
  return std::string(what) + " is " + en::proto::describe(refusal);
}

// Replaces anything shaped like a credential (secret.hpp's redact_credentials), its spaces made one line first.
std::string scrub(std::string_view text) {
  std::string flat(text);
  std::replace_if(flat.begin(), flat.end(), is_space, ' ');
  return redact_credentials(flat);
}

std::string error_object_message(const json& error) {
  if (error.is_string()) return error.get<std::string>();
  if (!error.is_object()) return {};
  for (const char* key : {"message", "detail", "error"}) {
    auto it = error.find(key);
    if (it != error.end() && it->is_string()) return it->get<std::string>();
  }
  return {};
}

}  // namespace

ThinkFilter::Output ThinkFilter::feed(std::string_view chunk) {
  std::string buffer = std::move(held_);
  held_.clear();
  buffer.append(chunk);
  Output out;
  std::string_view rest = buffer;
  while (!rest.empty()) {
    if (inside_) {
      const std::size_t close = rest.find(kClose);
      if (close == std::string_view::npos) {
        held_.assign(rest.substr(rest.size() - partial_suffix(rest, kClose)));
        return out;
      }
      rest.remove_prefix(close + kClose.size());
      inside_ = false;
      continue;
    }
    const std::size_t open = rest.find(kOpen);
    const std::size_t close = rest.find(kClose);
    if (close != std::string_view::npos && (open == std::string_view::npos || close < open)) {
      if (seen_open_) {
        out.text.append(rest.substr(0, close));  // a stray closing tag after a block: drop the tag alone
      } else {
        out.text.clear();  // reasoning that never opened its tag: none of it was the reply
        out.reset = true;
      }
      rest.remove_prefix(close + kClose.size());
      seen_open_ = true;
      continue;
    }
    if (open == std::string_view::npos) {
      const std::size_t keep = std::max(partial_suffix(rest, kOpen), partial_suffix(rest, kClose));
      out.text.append(rest.substr(0, rest.size() - keep));
      held_.assign(rest.substr(rest.size() - keep));
      return out;
    }
    out.text.append(rest.substr(0, open));
    rest.remove_prefix(open + kOpen.size());
    inside_ = true;
    seen_open_ = true;
  }
  return out;
}

std::string ThinkFilter::finish() {
  std::string rest = inside_ ? std::string() : std::move(held_);
  held_.clear();
  return rest;
}

ReplyAssembler::Step ReplyAssembler::on_event_data(std::string_view data) {
  if (overflowed_ || !refused_.empty()) return {};
  if (trim(data) == "[DONE]") {
    done_ = true;
    return {};
  }
  const en::proto::ParsedJson chunk = en::proto::parse_json(data);
  if (chunk.refusal == en::proto::JsonRefusal::TooLarge || chunk.refusal == en::proto::JsonRefusal::TooDeep) {
    refused_ = refusal_text("a stream event", chunk.refusal);
    return {};
  }
  if (!chunk.ok() || !chunk.value.is_object()) {
    ++bad_chunks_;
    return {};
  }
  return on_chunk(chunk.value, false);
}

ReplyAssembler::Step ReplyAssembler::on_whole_body(std::string_view body) {
  if (overflowed_ || !refused_.empty()) return {};
  const en::proto::ParsedJson reply = en::proto::parse_json(body);
  if (reply.refusal == en::proto::JsonRefusal::TooLarge || reply.refusal == en::proto::JsonRefusal::TooDeep) {
    refused_ = refusal_text("the reply", reply.refusal);
    return {};
  }
  if (!reply.ok() || !reply.value.is_object()) {
    ++bad_chunks_;
    return {};
  }
  Step step = on_chunk(reply.value, true);
  done_ = true;
  return step;
}

ReplyAssembler::Step ReplyAssembler::on_chunk(const json& chunk, bool whole) {
  Step step;
  if (auto error = chunk.find("error"); error != chunk.end() && !error->is_null()) {
    stream_error_ = classify_error_object(*error);
    stream_error_message_ = utf8_cut(scrub(trim(error_object_message(*error))), 200);
  }
  if (auto model = chunk.find("model"); model != chunk.end() && model->is_string()) {
    served_model_ = model->get<std::string>();
  }
  if (auto usage = chunk.find("usage"); usage != chunk.end() && usage->is_object()) {
    usage_.prompt_tokens = int_field(*usage, "prompt_tokens");
    usage_.completion_tokens = int_field(*usage, "completion_tokens");
    usage_.cached_tokens = int_field(*usage, "prompt_cache_hit_tokens");
    usage_.cost = cost_field(*usage);
    if (auto details = usage->find("prompt_tokens_details"); details != usage->end() && details->is_object()) {
      const int cached = int_field(*details, "cached_tokens");
      if (cached >= 0) usage_.cached_tokens = cached;
    }
  }
  auto choices = chunk.find("choices");
  if (choices == chunk.end() || !choices->is_array() || choices->empty() || !(*choices)[0].is_object()) return step;
  const json& choice = (*choices)[0];
  if (auto body = choice.find(whole ? "message" : "delta"); body != choice.end() && body->is_object()) {
    for (const char* field : {"reasoning", "reasoning_content", "thinking"}) {
      auto it = body->find(field);
      if (it != body->end() && it->is_string()) reasoning_chars_ += static_cast<long>(it->get_ref<const std::string&>().size());
    }
    if (auto content = body->find("content"); content != body->end() && content->is_string()) {
      step = add_content(content->get_ref<const std::string&>());
    }
  }
  if (auto finish = choice.find("finish_reason"); finish != choice.end() && finish->is_string()) {
    finish_reason_ = finish->get<std::string>();
  }
  if (auto logprobs = choice.find("logprobs"); logprobs != choice.end() && logprobs->is_object()) {
    auto content = logprobs->find("content");
    if (content != logprobs->end() && content->is_array()) {
      for (const json& entry : *content) {
        if (!entry.is_object()) continue;
        TokenLogprob token;
        if (auto t = entry.find("token"); t != entry.end() && t->is_string()) token.token = t->get<std::string>();
        if (auto l = entry.find("logprob"); l != entry.end() && l->is_number()) token.logprob = l->get<double>();
        if (auto top = entry.find("top_logprobs"); top != entry.end() && top->is_array()) {
          for (const json& alt : *top) {
            if (!alt.is_object()) continue;
            auto t = alt.find("token");
            auto l = alt.find("logprob");
            if (t != alt.end() && t->is_string() && l != alt.end() && l->is_number()) {
              token.top.push_back(TopLogprob{t->get<std::string>(), l->get<double>()});
            }
          }
        }
        logprobs_.push_back(std::move(token));
      }
    }
  }
  return step;
}

ReplyAssembler::Step ReplyAssembler::add_content(std::string_view content) {
  if (overflowed_) return {};
  raw_.append(content);
  ThinkFilter::Output out = think_.feed(content);
  Step step;
  if (out.reset) {
    text_.clear();
    step.reset = true;
  }
  append_visible(out.text, step);
  return step;
}

void ReplyAssembler::append_visible(std::string_view text, Step& step) {
  if (overflowed_) return;
  if (text_.empty()) {  // the reply starts at its first visible character
    const std::size_t first = text.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos) return;
    text.remove_prefix(first);
  }
  const std::size_t room = max_text_ - std::min(max_text_, text_.size());
  if (text.size() > room) {
    // ARCH §10.2: the display text stops at its cap, at a character's boundary, and the stream is to be cancelled.
    text = text.substr(0, utf8_prefix(text, room));
    overflowed_ = true;
  }
  text_.append(text);
  step.delta.append(text);
}

ReplyAssembler::Step ReplyAssembler::finish() {
  Step step;
  if (overflowed_ || !refused_.empty()) return step;
  append_visible(think_.finish(), step);
  while (!text_.empty() && is_space(text_.back())) text_.pop_back();
  return step;
}

ErrorClass classify_error_object(const json& error) {
  int code = -1;
  std::string kind;
  if (error.is_object()) {
    for (const char* key : {"code", "status"}) {
      auto it = error.find(key);
      if (it == error.end()) continue;
      if (it->is_number_integer() && code < 0) code = it->get<int>();
      if (it->is_string()) kind += ascii_lower(it->get<std::string>()) + " ";
    }
    if (auto type = error.find("type"); type != error.end() && type->is_string()) kind += ascii_lower(type->get<std::string>());
  }
  if (code >= 100) {
    const ErrorClass by_status = classify_http_status(code);
    return by_status == ErrorClass::None ? ErrorClass::Server : by_status;
  }
  if (kind.find("rate") != std::string::npos) return ErrorClass::RateLimited;
  if (kind.find("auth") != std::string::npos || kind.find("api_key") != std::string::npos) return ErrorClass::Auth;
  if (kind.find("invalid_request") != std::string::npos) return ErrorClass::BadRequest;
  return ErrorClass::Server;
}

bool ends_at_sentence(std::string_view text) {
  text = trim(text);
  bool trimmed = true;
  while (trimmed && !text.empty()) {
    trimmed = false;
    const char c = text.back();
    if (c == '"' || c == '\'' || c == ')' || c == ']' || c == '*' || is_space(c)) {
      text.remove_suffix(1);
      trimmed = true;
      continue;
    }
    for (std::string_view wide : kWideClosers) {
      if (text.size() >= wide.size() && text.substr(text.size() - wide.size()) == wide) {
        text.remove_suffix(wide.size());
        trimmed = true;
        break;
      }
    }
  }
  if (text.empty()) return false;
  const char last = text.back();
  if (last == '.' || last == '!' || last == '?') return true;
  return text.size() >= kEllipsis.size() && text.substr(text.size() - kEllipsis.size()) == kEllipsis;
}

std::string complete_sentences(std::string_view text) {
  std::size_t best = 0;
  for (std::size_t i = 0; i < text.size(); ++i) {
    std::size_t end = 0;
    if (text[i] == '.' || text[i] == '!' || text[i] == '?') {
      end = i + 1;
    } else if (text.substr(i, kEllipsis.size()) == kEllipsis) {
      end = i + kEllipsis.size();
    }
    if (end == 0) continue;
    while (end < text.size()) {
      const std::size_t closer = closer_at(text, end);
      if (closer == 0) break;
      end += closer;
    }
    if (end == text.size() || is_space(text[end])) best = end;
  }
  return std::string(trim(text.substr(0, best)));
}

bool looks_like_refusal(std::string_view text) {
  static constexpr std::array<std::string_view, 22> kOpenings = {
      "as an ai",                    "as a language model",          "as an assistant",
      "i can't help with",           "i cannot help with",           "i can't assist with",
      "i cannot assist with",        "i'm sorry, but i can't help",  "i'm sorry, but i cannot help",
      "i am sorry, but i cannot help", "i'm not able to help with",  "i am not able to help with",
      "i'm unable to help with",     "i am unable to help with",     "i can't comply",
      "i cannot comply",             "i can't fulfill",              "i cannot fulfill",
      "i can't fulfil",              "i cannot fulfil",              "sorry, i can't help",
      "sorry, but i can't help"};
  std::string head;
  std::size_t i = 0;
  while (i < text.size() && (is_space(text[i]) || text[i] == '"' || text[i] == '*' || text[i] == '\'')) ++i;
  for (; i < text.size() && head.size() < 64; ++i) {
    if (text.substr(i, 3) == "\xE2\x80\x99") {  // a curly apostrophe reads as a straight one
      head.push_back('\'');
      i += 2;
      continue;
    }
    head.push_back(text[i]);
  }
  head = ascii_lower(head);
  return std::any_of(kOpenings.begin(), kOpenings.end(), [&](std::string_view p) { return head.rfind(p, 0) == 0; });
}

std::string describe_error_body(std::string_view body, int status) {
  const std::string prefix = "HTTP " + std::to_string(status);
  if (status == 401 || status == 403) return prefix + ": the provider rejected the key";
  std::string message;
  const en::proto::ParsedJson parsed = en::proto::parse_json(body);
  if (parsed.ok() && parsed.value.is_object()) {
    if (auto error = parsed.value.find("error"); error != parsed.value.end()) message = error_object_message(*error);
    if (message.empty()) message = error_object_message(parsed.value);
  } else {
    message = std::string(body);
  }
  message = utf8_cut(scrub(trim(message)), 200);
  return message.empty() ? prefix : prefix + ": " + message;
}

bool names_reasoning(std::string_view body) {
  const std::string text = ascii_lower(body);
  return text.find("reasoning") != std::string::npos || text.find("think") != std::string::npos;
}

}  // namespace en::llm
