// libFuzzer target: en-core's pager (NPC §8.5, §8.6; speech_pager.hpp). The input's first byte is reply_words_max; the
// rest, cut at each newline, is the sentences pushed in order (the pager takes any text, sanitized or not), with
// next() asked at times the second byte steps. Besides any sanitizer report, a broken property is a finding:
// - every page fits two lines of 58 characters and 180 bytes, and is not empty;
// - its hold is page_hold_ms of its text; only the reply's final page may be `last`, and it is when it was sealed after
//   the reply ended (one sealed before cannot know it is the final one: Done tells);
// - a Wait's deadline is never in the past, and once the reply has ended the pages run out within a bound.
// An exception that escapes is a finding too.
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>
#include <vector>

#include "en/core/speech_pager.hpp"

namespace {

void require(bool holds, const char* property) {
  if (holds) return;
  std::fprintf(stderr, "property broken: %s\n", property);
  std::abort();
}

void check(const en::core::Sealing& s, en::core::Mono now, std::vector<en::core::Page>& pages) {
  if (s.kind == en::core::Sealing::Kind::Page) {
    require(!s.page.text.empty(), "a page is never empty");
    require(en::core::page_fits(s.page.text), "a page fits two lines of 58 characters and 180 bytes");
    require(s.page.hold_ms == en::core::page_hold_ms(s.page.text), "a page's hold is the formula's");
    pages.push_back(s.page);
  } else if (s.kind == en::core::Sealing::Kind::Wait && s.until) {
    require(*s.until > now, "a wait's deadline is still to come");
  }
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  if (size < 2) return 0;
  const std::uint16_t words = data[0];
  const en::core::Mono step = data[1];
  // SAFETY: libFuzzer hands over `size` readable bytes, which live until this call returns; the view reads them as
  // the characters they are, and ends where they end.
  const std::string_view rest(reinterpret_cast<const char*>(data) + 2, size - 2);
  const en::core::SpeechPager pager;
  auto reply = pager.open(en::core::PageContext{words});
  std::vector<en::core::Page> pages;
  en::core::Mono now = 0;
  std::size_t start = 0;
  std::uint8_t index = 0;
  while (start <= rest.size()) {
    const std::size_t end = std::min(rest.find('\n', start), rest.size());
    reply->push(en::core::Sentence{std::string(rest.substr(start, end - start)), index++, {}});
    check(reply->next(now), now, pages);
    now += step;
    start = end + 1;
  }
  reply->end();
  const std::size_t before_end = pages.size();
  bool done = false;
  for (int guard = 0; guard < 4096 && !done; ++guard) {
    const en::core::Sealing s = reply->next(now);
    check(s, now, pages);
    done = s.kind == en::core::Sealing::Kind::Done;
    require(s.kind != en::core::Sealing::Kind::Wait, "once the reply has ended, nothing waits");
    now += s.kind == en::core::Sealing::Kind::Page ? s.page.hold_ms : 1;
  }
  require(done, "the pages run out once the reply has ended");
  for (std::size_t i = 0; i < pages.size(); ++i) {
    if (pages[i].last) require(i + 1 == pages.size(), "only the final page is last");
  }
  if (pages.size() > before_end) require(pages.back().last, "the final page, sealed after the end, is last");
  return 0;
}
