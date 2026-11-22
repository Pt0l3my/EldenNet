// The sanitizer (NPC §8.2 steps 0 to 7; speech_sanitizer.hpp): each step alone, the splitter's boundaries, the
// removals' reasons and bytes, the flow and the intent, withdraw(), and seeded property tests on random streams split
// at every byte and at random. Every line is invented.
#include <doctest.h>

#include <algorithm>
#include <cstdint>
#include <optional>
#include <random>
#include <string>
#include <vector>

#include "en/core/speech_sanitizer.hpp"

using namespace en::core;

namespace doctest {
template <>
struct StringMaker<Sentence> {
  static String convert(const Sentence& s) {
    std::string out = "#" + std::to_string(s.index) + " \"" + s.text + "\"";
    for (const Removal& r : s.removed) {
      out += " " + std::to_string(static_cast<int>(r.what)) + ":" + std::to_string(r.bytes);
    }
    return String(out.c_str());
  }
};
}  // namespace doctest

namespace {

struct Run {
  std::vector<Sentence> sentences;
  Flow flow = Flow::More;
  std::optional<std::string> intent;
};

std::vector<std::string> names_none;

Run whole(std::string_view text, const std::vector<std::string>& names = names_none) {
  const SpeechSanitizer s;
  auto stream = s.open(SanitizeContext{names});
  Run r;
  r.flow = stream->feed(text, r.sentences);
  stream->finish(r.sentences);
  r.intent = stream->intent();
  return r;
}

Run pieces(std::string_view text, const std::vector<std::size_t>& cuts,
           const std::vector<std::string>& names = names_none) {
  const SpeechSanitizer s;
  auto stream = s.open(SanitizeContext{names});
  Run r;
  std::size_t at = 0;
  for (const std::size_t cut : cuts) {
    if (cut <= at || cut > text.size()) continue;
    const Flow f = stream->feed(text.substr(at, cut - at), r.sentences);
    if (f != Flow::More) r.flow = f;
    at = cut;
  }
  const Flow f = stream->feed(text.substr(at), r.sentences);
  if (f != Flow::More) r.flow = f;
  stream->finish(r.sentences);
  r.intent = stream->intent();
  return r;
}

Run bytewise(std::string_view text, const std::vector<std::string>& names = names_none) {
  std::vector<std::size_t> cuts;
  for (std::size_t i = 1; i < text.size(); ++i) cuts.push_back(i);
  return pieces(text, cuts, names);
}

std::vector<std::string> texts(const Run& r) {
  std::vector<std::string> out;
  for (const Sentence& s : r.sentences) out.push_back(s.text);
  return out;
}

using V = std::vector<std::string>;

std::uint32_t removed(const Sentence& s, Removed what) {
  for (const Removal& r : s.removed) {
    if (r.what == what) return r.bytes;
  }
  return 0;
}

std::uint32_t removed(const Run& r, Removed what) {
  std::uint32_t n = 0;
  for (const Sentence& s : r.sentences) n += removed(s, what);
  return n;
}

}  // namespace

TEST_CASE("step 7: sentences end at a terminator run and closing quotes before white space or the stream's end") {
  CHECK(texts(whole("The ford is low. Cross at dawn! Will you? Good.")) ==
        V{"The ford is low.", "Cross at dawn!", "Will you?", "Good."});
  CHECK(texts(whole("Truly?! Then go.")) == V{"Truly?!", "Then go."});
  CHECK(texts(whole("He said it was 3.5 leagues. Far.")) == V{"He said it was 3.5 leagues.", "Far."});
  CHECK(texts(whole("Well... perhaps. Or not.")) == V{"Well... perhaps.", "Or not."});
  CHECK(texts(whole("Well... Perhaps.")) == V{"Well...", "Perhaps."});
  CHECK(texts(whole("Wait\xE2\x80\xA6 no. Go.")) == V{"Wait... no.", "Go."});  // "…" is "...", and the same rule
  CHECK(texts(whole("A last fragment with no end")) == V{"A last fragment with no end"});
  CHECK(texts(whole("   ")) == V{});
  CHECK(whole("Ends here.").sentences.front().index == 0);
  const Run r = whole("One. Two. Three.");
  REQUIRE(r.sentences.size() == 3);
  CHECK(r.sentences[2].index == 2);
}

TEST_CASE("step 7: the open fragment is held until a boundary is known, and a sentence until the next has begun") {
  const SpeechSanitizer s;
  auto stream = s.open(SanitizeContext{});
  std::vector<Sentence> out;
  stream->feed("The reeds are tall.", out);
  CHECK(out.empty());  // "." may be the start of "..."
  stream->feed(" ", out);
  CHECK(out.empty());  // the next sentence has not begun: what lies between is not known yet
  stream->feed("Mind", out);
  REQUIRE(out.size() == 1);
  CHECK(out[0].text == "The reeds are tall.");
  stream->feed(" them...", out);
  stream->feed(" ", out);
  CHECK(out.size() == 1);  // "..." before a word not yet seen
  stream->feed("they cut.", out);
  CHECK(out.size() == 1);
  stream->finish(out);
  REQUIRE(out.size() == 2);
  CHECK(out[1].text == "Mind them... they cut.");
}

TEST_CASE("step 0: lossy UTF-8, NFC within Latin, a character split across deltas, and the 4 KB cap") {
  // A broken sequence is one replacement, which the whitelist takes.
  const Run broken = whole("Bad \xC3 byte. Fine.");
  CHECK(texts(broken) == V{"Bad byte.", "Fine."});
  CHECK(removed(broken.sentences[0], Removed::Character) == 1);
  // e and a combining acute are é; two bytes of the raw stream become one letter.
  const Run nfc = whole(
      "Caf"
      "e\xCC\x81"
      " by the weir.");
  CHECK(texts(nfc) == V{"Caf\xC3\xA9 by the weir."});
  CHECK(nfc.sentences[0].removed.empty());
  // Split inside a character, and between a letter and its mark.
  CHECK(texts(pieces("Caf"
                     "e\xCC\x81"
                     " by the weir.",
                     {4, 5})) == V{"Caf\xC3\xA9 by the weir."});
  CHECK(texts(pieces("Na\xC3\xAFve.", {3})) == V{"Na\xC3\xAFve."});
  // The cap: what is past 4 KB is OverLimit, and the stream stops.
  std::string big;
  while (big.size() < 4090) big += "Low tide. ";
  big += "Over the edge.";
  const SpeechSanitizer s;
  auto stream = s.open(SanitizeContext{});
  std::vector<Sentence> out;
  CHECK(stream->feed(big, out) == Flow::Stop);
  REQUIRE(!out.empty());
  CHECK(removed(out.back(), Removed::OverLimit) == big.size() - kRawStreamMax);
  CHECK(stream->feed("More.", out) == Flow::Stop);
}

TEST_CASE("step 1: reasoning tags, any case, to their own close; an unclosed one swallows the rest") {
  const Run r = whole("<think>The user wants a reply.</think>The water is cold.");
  CHECK(texts(r) == V{"The water is cold."});
  CHECK(removed(r.sentences[0], Removed::Reasoning) == 38);
  CHECK(texts(whole("<THINKING>plan</thinking> Yes. <Reasoning>no</REASONING>No.")) == V{"Yes.", "No."});
  CHECK(texts(whole("Before. <think>after, unclosed")) == V{"Before."});
  CHECK(texts(whole("A <thinker> stays a tag.")) == V{"A stays a tag."});  // not a reasoning tag: markup
  // Split inside the tags.
  CHECK(texts(pieces("<think>x</think>Cold.", {2, 9, 12})) == V{"Cold."});
}

TEST_CASE("step 2: the trailer, at a line's start, ends the speech and keeps the intent") {
  const Run r = whole("Take the left path.\nACT: points west");
  CHECK(texts(r) == V{"Take the left path."});
  CHECK(r.flow == Flow::IntentOnly);
  REQUIRE(r.intent);
  CHECK(*r.intent == "points west");
  CHECK(removed(r.sentences[0], Removed::Trailer) == 4);  // the line's start through its colon
  CHECK(*whole("Fine.\n> **action:** nods\nslowly").intent == "** nods slowly");
  CHECK(whole("Fine.\n> **action:** nods").flow == Flow::IntentOnly);
  CHECK(!whole("They act: badly.").intent);  // not at a line's start
  CHECK(texts(whole("An open fragment\nACT: waits")) == V{"An open fragment"});
  CHECK(!whole("No trailer here.").intent);
  // IntentOnly comes as soon as ACT: is seen, and the intent grows with the stream.
  const SpeechSanitizer s;
  auto stream = s.open(SanitizeContext{});
  std::vector<Sentence> out;
  CHECK(stream->feed("Go.\nAC", out) == Flow::More);
  CHECK(stream->feed("T: wav", out) == Flow::IntentOnly);
  CHECK(out.size() == 1);
  stream->feed("es", out);
  stream->finish(out);
  CHECK(*stream->intent() == "waves");
}

TEST_CASE("step 3: a run-on into the Tarnished's turn ends the reply") {
  const Run r = whole("Rest here.\nThe Tarnished: I will.\nRest here.");
  CHECK(texts(r) == V{"Rest here."});
  CHECK(r.flow == Flow::Stop);
  CHECK(removed(r.sentences[0], Removed::RunOn) == 14);
  CHECK(whole("Rest.\ntarnished: yes").flow == Flow::Stop);
  CHECK(whole("Rest.\n**Tarnished:** yes").flow == Flow::Stop);
  CHECK(texts(whole("Tarnished: nothing")).empty());
  CHECK(whole("I know the Tarnished: well.").flow == Flow::More);  // not at a line's start
}

TEST_CASE("step 4: markup, emoji and controls, each with its reason and bytes") {
  const Run tags = whole("<b>Bold</b> words <?pi?> here.");
  CHECK(texts(tags) == V{"Bold words here."});
  CHECK(removed(tags, Removed::Markup) == 3 + 4 + 6);
  CHECK(texts(whole("[softly] Come closer. [Then](somewhere) leave.")) == V{"Come closer.", "Then leave."});
  CHECK(texts(whole("Go (he points) now.")) == V{"Go now."});
  CHECK(texts(whole("Go (this aside runs on far too long to be a stage direction at all, so it stays) now.")) ==
        V{"Go this aside runs on far too long to be a stage direction at all, so it stays now."});
  CHECK(texts(whole("*nods slowly* Yes. _sighs_ No.")) == V{"Yes.", "No."});
  CHECK(texts(whole("It is **very** cold.")) == V{"It is very cold."});
  CHECK(texts(whole("A lone * star.")) == V{"A lone star."});
  CHECK(texts(whole("snake_case stays one word.")) == V{"snakecase stays one word."});
  CHECK(texts(whole("# Heading\n- first\n* second\n> quoted\nend.")) == V{"Heading first second quoted end."});
  CHECK(texts(whole("Use `this` word.")) == V{"Use this word."});
  const Run emoji = whole("Warm \xF0\x9F\x94\xA5 fire\xEF\xB8\x8F.");
  CHECK(texts(emoji) == V{"Warm fire."});
  CHECK(removed(emoji, Removed::Emoji) == 7);
  const Run controls = whole("Line one\nline two\x07.");
  CHECK(texts(controls) == V{"Line one line two."});
  CHECK(removed(controls, Removed::Control) == 1);
  CHECK(removed(whole("A\x01"
                      "b.\xC2\x85"),
                Removed::Control) == 3);  // C0 and C1
}

TEST_CASE("step 5: a leading Name:, and quotation marks at a sentence's edges") {
  const std::vector<std::string> names = {"Brannoch", "Reed-Cutter"};
  const Run named = whole("Brannoch: The weir holds.", names);
  CHECK(texts(named) == V{"The weir holds."});
  CHECK(removed(named.sentences[0], Removed::NamePrefix) == 9);
  CHECK(texts(whole("reed-cutter: Low water.", names)) == V{"Low water."});
  CHECK(texts(whole("Old Ferry-Hand: Low water.")) == V{"Low water."});  // the general pattern
  CHECK(texts(whole("Low water. Brannoch: again.", names)) == V{"Low water.", "Brannoch: again."});  // first only
  const Run quoted = whole("\"The weir holds. Mostly.\"");
  CHECK(texts(quoted) == V{"The weir holds.", "Mostly."});
  CHECK(removed(quoted, Removed::Quotes) == 2);
  CHECK(texts(whole("'Hold fast.' Then go.")) == V{"Hold fast.", "Then go."});
  CHECK(texts(whole("\xE2\x80\x9C"
                    "Curly.\xE2\x80\x9D")) == V{"Curly."});
  CHECK(texts(whole("'Tis late, lads.")) == V{"'Tis late, lads."});  // an elision stays
  CHECK(texts(whole("The lads' boat.")) == V{"The lads' boat."});
  CHECK(texts(whole("He said \"go\" twice.")) == V{"He said go twice."});
  CHECK(removed(whole("He said \"go\" twice."), Removed::Character) == 2);
}

TEST_CASE("step 6: the whitelist, apostrophes, the ellipsis, collapsed spaces and long tokens") {
  CHECK(texts(whole("Don\xE2\x80\x99t\xC2\xA0 go \xE2\x80\x94 not yet.")) == V{"Don't go \xE2\x80\x94 not yet."});
  const Run greek = whole("\xCE\xA9mega here.");
  CHECK(texts(greek) == V{"mega here."});
  CHECK(removed(greek.sentences[0], Removed::Character) == 2);
  CHECK(texts(whole("Wait\xE2\x80\xA6")) == V{"Wait..."});
  CHECK(texts(whole("So   many    spaces.")) == V{"So many spaces."});
  const Run long_token = whole("A antidisestablishmentarianisms word.");
  CHECK(texts(long_token) == V{"A word."});
  CHECK(removed(long_token.sentences[0], Removed::LongToken) == 29);
  CHECK(texts(whole("Fine. \xE2\x98\x85\xE2\x98\x85. Next.")) ==
        V{"Fine.", "Next."});  // a sentence with no word is none
}

TEST_CASE("removals between sentences go with the earlier, and leading ones with the first") {
  const Run r = whole("*waves* Hello. *bows* Farewell. *leaves*");
  CHECK(texts(r) == V{"Hello.", "Farewell."});
  CHECK(removed(r.sentences[0], Removed::Markup) == 7 + 6);
  CHECK(removed(r.sentences[1], Removed::Markup) == 8);
}

TEST_CASE("withdraw drops what is held and keeps counting sentences") {
  const SpeechSanitizer s;
  auto stream = s.open(SanitizeContext{});
  std::vector<Sentence> out;
  stream->feed("Reasoning, it seems. Still", out);
  REQUIRE(out.size() == 1);
  stream->withdraw();
  stream->feed("The real reply.", out);
  stream->finish(out);
  REQUIRE(out.size() == 2);
  CHECK(out[1].text == "The real reply.");
  CHECK(out[1].index == 1);
  CHECK(!stream->intent());
}

namespace {

// A random reply of invented pieces: words, punctuation, markup, reasoning, emoji, controls, broken bytes, newlines,
// trailers and run-ons.
std::string random_stream(std::mt19937& rng) {
  static const std::vector<std::string> kPieces = {"the",
                                                   "weir",
                                                   "Holds",
                                                   "cold",
                                                   "Water",
                                                   "reed",
                                                   "tide",
                                                   " ",
                                                   " ",
                                                   " ",
                                                   " ",
                                                   ".",
                                                   ". ",
                                                   "! ",
                                                   "? ",
                                                   "...",
                                                   "... ",
                                                   "\xE2\x80\xA6 ",
                                                   ",",
                                                   ";",
                                                   "'",
                                                   "\"",
                                                   "\xE2\x80\x99",
                                                   "\xE2\x80\x9C",
                                                   "*",
                                                   "**",
                                                   "_",
                                                   "(",
                                                   ")",
                                                   "[",
                                                   "]",
                                                   "<",
                                                   ">",
                                                   "<b>",
                                                   "</b>",
                                                   "<think>",
                                                   "</think>",
                                                   "<THINKING>",
                                                   "#",
                                                   "- ",
                                                   "> ",
                                                   "`",
                                                   "\n",
                                                   "\r\n",
                                                   "\t",
                                                   "\x01",
                                                   "\xC2\x85",
                                                   "\xC3\xA9",
                                                   "e\xCC\x81",
                                                   "\xF0\x9F\x94\xA5",
                                                   "\xEF\xB8\x8F",
                                                   "\xC3",
                                                   "\xFF",
                                                   "\xE2\x82",
                                                   "\nACT: nods",
                                                   "\nThe Tarnished: so",
                                                   "\ntarnished:",
                                                   "Brannoch: ",
                                                   "aaaaaaaaaaaaaaaaaaaaaaaaaaaa",
                                                   "3.5",
                                                   "[a](b)",
                                                   "'tis",
                                                   "\xCE\xA9",
                                                   "\xE2\x80\x94"};
  std::uniform_int_distribution<std::size_t> count(0, 60);
  std::uniform_int_distribution<std::size_t> pick(0, kPieces.size() - 1);
  std::string s;
  const std::size_t n = count(rng);
  for (std::size_t i = 0; i < n; ++i) s += kPieces[pick(rng)];
  return s;
}

void same(const Run& a, const Run& b) {
  REQUIRE(a.sentences.size() == b.sentences.size());
  for (std::size_t i = 0; i < a.sentences.size(); ++i) CHECK(a.sentences[i] == b.sentences[i]);
  CHECK(a.flow == b.flow);
  CHECK(a.intent == b.intent);
}

}  // namespace

TEST_CASE("property: a stream split at every byte, or at random, gives what the whole stream gives") {
  std::mt19937 rng(20261122);
  const std::vector<std::string> names = {"Brannoch"};
  for (int round = 0; round < 400; ++round) {
    const std::string text = random_stream(rng);
    CAPTURE(round);
    CAPTURE(text);
    const Run all = whole(text, names);
    same(all, bytewise(text, names));
    std::vector<std::size_t> cuts;
    std::uniform_int_distribution<std::size_t> at(0, text.size());
    for (int k = 0; k < 6; ++k) cuts.push_back(at(rng));
    std::sort(cuts.begin(), cuts.end());
    same(all, pieces(text, cuts, names));
  }
}

TEST_CASE("property: every sentence holds only the whitelist, trimmed and collapsed, with no long token") {
  std::mt19937 rng(7);
  for (int round = 0; round < 400; ++round) {
    const std::string text = random_stream(rng);
    for (const Sentence& s : whole(text).sentences) {
      CAPTURE(round);
      CHECK(!s.text.empty());
      CHECK(s.text.front() != ' ');
      CHECK(s.text.back() != ' ');
      CHECK(s.text.find("  ") == std::string::npos);
      for (const char c : s.text) {
        const auto u = static_cast<unsigned char>(c);
        const bool ok = u >= 0x80 || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                        std::string_view(" .,;:!?'-").find(c) != std::string_view::npos;
        CHECK(ok);
      }
      for (const Removal& r : s.removed) CHECK(r.bytes > 0);
    }
  }
}
