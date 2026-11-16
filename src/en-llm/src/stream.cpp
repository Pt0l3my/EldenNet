#include "en/llm/stream.hpp"

#include <algorithm>
#include <array>

#include "en/llm/secret.hpp"

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

std::string utf8_cut(std::string s, std::size_t max) {
  if (s.size() <= max) return s;
  std::size_t cut = max;
  while (cut > 0 && (static_cast<unsigned char>(s[cut]) & 0xC0) == 0x80) --cut;  // never split a character
  s.resize(cut);
  return s + "...";
}

bool token_char(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
}

// Replaces anything shaped like a credential: "sk-…" and "Bearer …" tokens, and any long run of token characters.
std::string scrub(std::string_view text) {
  std::string out;
  std::size_t i = 0;
  while (i < text.size()) {
    if (!token_char(text[i])) {
      out.push_back(is_space(text[i]) ? ' ' : text[i]);
      ++i;
      continue;
    }
    std::size_t j = i;
    while (j < text.size() && token_char(text[j])) ++j;
    const std::string_view word = text.substr(i, j - i);
    const bool keyish = word.size() >= 24 || word.rfind("sk-", 0) == 0 || word.rfind("sk_", 0) == 0;
    out.append(keyish ? std::string(Secret::kRedacted) : std::string(word));
    i = j;
  }
  // "Bearer <token>" with a short token still names a credential.
  for (std::size_t at = out.find("Bearer "); at != std::string::npos; at = out.find("Bearer ", at + 7)) {
    std::size_t end = at + 7;
    while (end < out.size() && !is_space(out[end])) ++end;
    out.replace(at + 7, end - at - 7, Secret::kRedacted);
  }
  return out;
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
  if (trim(data) == "[DONE]") {
    done_ = true;
    return {};
  }
  const json chunk = json::parse(data, nullptr, /*allow_exceptions=*/false);
  if (chunk.is_discarded() || !chunk.is_object()) {
    ++bad_chunks_;
    return {};
  }
  return on_chunk(chunk, false);
}

ReplyAssembler::Step ReplyAssembler::on_whole_body(std::string_view body) {
  const json reply = json::parse(body, nullptr, /*allow_exceptions=*/false);
  if (reply.is_discarded() || !reply.is_object()) {
    ++bad_chunks_;
    return {};
  }
  Step step = on_chunk(reply, true);
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
  if (text_.empty()) {  // the reply starts at its first visible character
    const std::size_t first = text.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos) return;
    text.remove_prefix(first);
  }
  text_.append(text);
  step.delta.append(text);
}

ReplyAssembler::Step ReplyAssembler::finish() {
  Step step;
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
  const json parsed = json::parse(body, nullptr, /*allow_exceptions=*/false);
  if (!parsed.is_discarded() && parsed.is_object()) {
    if (auto error = parsed.find("error"); error != parsed.end()) message = error_object_message(*error);
    if (message.empty()) message = error_object_message(parsed);
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
