// The pager (NPC §8.5, §8.6; speech_pager.hpp): the hold formula and Tab's skip, the page's fit, the caps (words,
// sentences, bytes, the first sentence's cut), lazy sealing (page 1's 250 ms wait, page k+1 when asked, a short
// sentence joining the next), the over-long sentence's split, `last` and Done, and a seeded property test. Every line
// is invented.
#include <doctest.h>

#include <random>
#include <string>
#include <vector>

#include "en/core/speech_pager.hpp"

using namespace en::core;

namespace {

Sentence sentence(std::string text, std::uint8_t index = 0) { return Sentence{std::move(text), index, {}}; }

std::string words(std::size_t n, const char* word = "reed") {
  std::string s;
  for (std::size_t i = 0; i < n; ++i) {
    if (i > 0) s += ' ';
    s += word;
  }
  return s + ".";
}

// Every page of a reply, asking whenever asked to wait, at the waits' deadlines.
std::vector<Page> drain(ReplyPager& p, Mono now = 0) {
  std::vector<Page> out;
  for (int guard = 0; guard < 100; ++guard) {
    const Sealing s = p.next(now);
    if (s.kind == Sealing::Kind::Done) break;
    if (s.kind == Sealing::Kind::Page) {
      out.push_back(s.page);
      continue;
    }
    if (!s.until) break;
    now = *s.until;
  }
  return out;
}

}  // namespace

TEST_CASE("the hold: clamp(1200 + 55 x chars, 2000, 6500), 400 more after an ellipsis, and Tab's 300 ms floor") {
  CHECK(page_hold_ms("Short.") == 2000);
  CHECK(page_hold_ms(std::string(60, 'a')) == 1200 + 55 * 60);
  CHECK(page_hold_ms(std::string(116, 'a')) == 6500);
  CHECK(page_hold_ms("Wait...") == 2400);
  CHECK(page_hold_ms(std::string(57, 'a') + "...") == 1200 + 55 * 60 + 400);
  CHECK(page_hold_ms("\xC3\xA9t\xC3\xA9 et \xC3\xA9t\xC3\xA9") == 2000);  // 10 characters (16 bytes would be 2080)
  CHECK(hold_ends(1000, 3000) == 4000);
  CHECK(hold_ends(1000, 3000, 1100) == 1300);  // not before 300 ms
  CHECK(hold_ends(1000, 3000, 2500) == 2500);
  CHECK(hold_ends(1000, 3000, 9000) == 4000);
}

TEST_CASE("a page: two wrapped lines of 58 characters, and 180 bytes") {
  CHECK(page_fits(std::string(58, 'a')));
  CHECK(!page_fits(std::string(59, 'a')));
  CHECK(page_fits(std::string(58, 'a') + " " + std::string(58, 'b')));
  CHECK(!page_fits(std::string(58, 'a') + " " + std::string(58, 'b') + " c"));
  CHECK(!page_fits(std::string(30, 'a') + " " + std::string(30, 'b') + " " + std::string(30, 'c')));  // wraps to 3
  std::string wide;
  for (int i = 0; i < 36; ++i) wide += "\xC3\xA9\xC3\xA9 ";
  CHECK(page_fits(wide.substr(0, 179)));
  CHECK(!page_fits(wide + "\xC3\xA9"));  // two lines of 109 characters, but 182 bytes
  CHECK(reply_words("One, two... three - four.") == 4);
}

TEST_CASE("the caps: words to the cap, the crossing sentence within the cap + 10, three sentences, 600 bytes") {
  const SpeechPager pager;
  auto p = pager.open(PageContext{20});
  CHECK(p->push(sentence(words(8))) == Take::Kept);
  CHECK(p->push(sentence(words(8))) == Take::Kept);
  CHECK(p->push(sentence(words(8))) == Take::Full);  // 24: crosses 20, within 30
  CHECK(p->push(sentence(words(2))) == Take::Over);

  p = pager.open(PageContext{20});
  CHECK(p->push(sentence(words(8))) == Take::Kept);
  CHECK(p->push(sentence(words(8))) == Take::Kept);
  CHECK(p->push(sentence(words(15))) == Take::Over);  // 31: past 30, left out, and the reply is full
  CHECK(p->push(sentence(words(1))) == Take::Over);

  p = pager.open(PageContext{40});
  CHECK(p->push(sentence("One.")) == Take::Kept);
  CHECK(p->push(sentence("Two.")) == Take::Kept);
  CHECK(p->push(sentence("Three.")) == Take::Full);  // at most three sentences

  p = pager.open(PageContext{20});
  CHECK(p->push(sentence(words(20))) == Take::Kept);  // the total may equal the cap
  CHECK(p->push(sentence(words(1))) == Take::Full);

  p = pager.open(PageContext{0});  // 0: the schema's default of 40
  CHECK(p->push(sentence(words(45))) == Take::Full);
}

TEST_CASE("the first sentence past the cap + 10 is cut at its last ';', dash or ',' within it, else at the word") {
  const SpeechPager pager;
  auto p = pager.open(PageContext{10});
  std::string long_one = "The tide came in, the reeds bent low; and then the water rose over the weir and the path";
  long_one += " and the old stones and the boats.";
  CHECK(p->push(sentence(long_one)) == Take::Full);
  p->end();
  std::string all;
  for (const Page& page : drain(*p)) all += (all.empty() ? "" : " ") + page.text;
  CHECK(all == "The tide came in, the reeds bent low...");

  p = pager.open(PageContext{10});
  CHECK(p->push(sentence(words(30))) == Take::Full);
  p->end();
  all.clear();
  for (const Page& page : drain(*p)) all += (all.empty() ? "" : " ") + page.text;
  CHECK(reply_words(all) == 20);
  CHECK(all.size() > 3);
  CHECK(all.substr(all.size() - 3) == "...");
}

TEST_CASE("600 bytes a reply: a later sentence past it is left out") {
  const SpeechPager pager;
  auto p = pager.open(PageContext{60});
  const std::string fat(24, 'x');
  std::string s1;
  for (int i = 0; i < 12; ++i) s1 += fat + " ";
  s1 += "end.";
  CHECK(p->push(sentence(s1)) == Take::Kept);
  CHECK(p->push(sentence(s1)) == Take::Over);
}

TEST_CASE("page 1: sealed at once when a line or more, else it waits up to 250 ms for another that fits") {
  const SpeechPager pager;
  auto p = pager.open(PageContext{40});
  CHECK(p->next(0).kind == Sealing::Kind::Wait);
  p->push(sentence("The tide is turning, and the reeds bend low by the weir now."));
  Sealing s = p->next(100);
  REQUIRE(s.kind == Sealing::Kind::Page);
  CHECK(!s.until);

  p = pager.open(PageContext{40});
  p->push(sentence("The tide is turning."));
  s = p->next(1000);
  CHECK(s.kind == Sealing::Kind::Wait);
  CHECK(s.until == 1250);
  CHECK(p->next(1100).until == 1250);  // from when it was first asked for
  p->push(sentence("Mind the reeds."));
  s = p->next(1150);
  CHECK(s.kind == Sealing::Kind::Wait);  // still under a line: the wait goes on
  s = p->next(1250);
  REQUIRE(s.kind == Sealing::Kind::Page);
  CHECK(s.page.text == "The tide is turning. Mind the reeds.");
  CHECK(!s.page.last);
  p->end();
  CHECK(p->next(1300).kind == Sealing::Kind::Done);

  p = pager.open(PageContext{40});
  p->push(sentence("The tide is turning."));
  CHECK(p->next(0).kind == Sealing::Kind::Wait);
  p->end();  // the reply ended: no wait
  s = p->next(10);
  REQUIRE(s.kind == Sealing::Kind::Page);
  CHECK(s.page.last);
}

TEST_CASE("a sentence under 12 characters joins the next on its page, waiting for it while the reply goes on") {
  const SpeechPager pager;
  auto p = pager.open(PageContext{40});
  p->push(sentence("Hm."));
  Sealing s = p->next(0);
  CHECK(s.kind == Sealing::Kind::Wait);
  CHECK(!s.until);  // no deadline: it joins the next
  CHECK(p->next(5000).kind == Sealing::Kind::Wait);
  p->push(sentence("The ford is low tonight, so cross before the moon is up."));
  s = p->next(5000);
  REQUIRE(s.kind == Sealing::Kind::Page);
  CHECK(s.page.text == "Hm. The ford is low tonight, so cross before the moon is up.");

  // At a page's end it leaves for the next page, when the next sentence does not fit beside it.
  p = pager.open(PageContext{60});
  p->push(sentence("The ford is low tonight, so cross before the moon is fully up, friend."));
  p->push(sentence("Hm."));
  p->push(sentence("The reeds will hide you from the far bank, if you keep your head down low."));
  p->end();
  const std::vector<Page> pages = drain(*p);
  REQUIRE(pages.size() == 2);
  CHECK(pages[0].text == "The ford is low tonight, so cross before the moon is fully up, friend.");
  CHECK(pages[1].text.substr(0, 4) == "Hm. ");
}

TEST_CASE("an over-long sentence splits at its last mark among characters 40 to 116, else at a space") {
  const SpeechPager pager;
  auto p = pager.open(PageContext{60});
  const std::string text =
      "The water came up over the stones in the night, and the boats that were tied below the weir broke loose; "
      "we found them at dawn in the reeds.";
  p->push(sentence(text));
  p->end();
  std::vector<Page> pages = drain(*p);
  REQUIRE(pages.size() == 2);
  CHECK(pages[0].text ==
        "The water came up over the stones in the night, and the boats that were tied below the weir broke loose;");
  CHECK(pages[1].text == "we found them at dawn in the reeds.");
  CHECK(pages[1].last);
  for (const Page& page : pages) CHECK(page_fits(page.text));

  CHECK(page_split("", std::string(50, 'a') + " " + std::string(70, 'b')) == 50);  // no mark: the last space that fits
  CHECK(page_split("", std::string(70, 'a')) == 0);
}

namespace {

std::string random_sentence(std::mt19937& rng) {
  static const std::vector<std::string> kWords = {"the", "reeds", "weir", "a", "tide-water", "boatman's", "cold,",
                                                  "low;", "\xC3\xA9t\xC3\xA9", "stone", "\xE2\x80\x94", "night",
                                                  "xxxxxxxxxxxxxxxxxxxxxxxx", "ford"};
  std::uniform_int_distribution<std::size_t> count(1, 30);
  std::uniform_int_distribution<std::size_t> pick(0, kWords.size() - 1);
  std::string s;
  const std::size_t n = count(rng);
  for (std::size_t i = 0; i < n; ++i) s += (i > 0 ? " " : "") + kWords[pick(rng)];
  return s + ".";
}

}  // namespace

TEST_CASE("property: every page fits, holds as the formula says, and the pages keep the reply's words in order") {
  std::mt19937 rng(11);
  const SpeechPager pager;
  for (int round = 0; round < 500; ++round) {
    CAPTURE(round);
    auto p = pager.open(PageContext{60});
    std::string kept;
    std::uniform_int_distribution<int> n(1, 3);
    const int count = n(rng);
    bool cut = false;
    for (int i = 0; i < count; ++i) {
      const std::string s = random_sentence(rng);
      const Take t = p->push(sentence(s, static_cast<std::uint8_t>(i)));
      if (t == Take::Over) break;
      if (i == 0 && (reply_words(s) > 70 || s.size() > kReplyBytes)) cut = true;
      if (!cut) kept += (kept.empty() ? "" : " ") + s;
      if (t == Take::Full) break;
    }
    p->end();
    const std::vector<Page> pages = drain(*p);
    std::string shown;
    for (std::size_t k = 0; k < pages.size(); ++k) {
      CHECK(page_fits(pages[k].text));
      CHECK(pages[k].hold_ms == page_hold_ms(pages[k].text));
      CHECK(pages[k].last == (k + 1 == pages.size()));
      shown += (shown.empty() ? "" : " ") + pages[k].text;
    }
    if (!cut) CHECK(shown == kept);
  }
}
