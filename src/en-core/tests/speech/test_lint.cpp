// The lint (NPC §8.3, §8.4; speech_lint.hpp) on an invented lexicon: each hard list and its actions (Regenerate,
// Deflect, Stop), the safety list's fingerprints and their normalisation on invented words only (O107), each soft
// item, the tally carried between replies, the deflection ring, the lexicon's reader and its errors, and SHA-256 on
// NIST's examples. Every word and line here is invented, the safety list's above all.
#include <doctest.h>

#include <algorithm>
#include <string>
#include <vector>

#include "../../src/speech/sha256.hpp"
#include "en/core/speech_lint.hpp"

using namespace en::core;

namespace {

using V = std::vector<std::string>;

// Invented safety words: nonsense coined for these tests, standing in for the real list's.
std::string safety_toml() {
  return "format = 1\nalways = [\"" + *safety_fingerprint("zorblat") + "\", \"" + *safety_fingerprint("quix vandle") +
         "\"]\ngentle = [\"" + *safety_fingerprint("grubbleflesh") + "\"]\n";
}

constexpr const char* kBanned = R"(format = 1
[hard]
mechanics = ["quest", "respawn*", "level up"]
modern = ["okay"]
[promise]
hard = ["follow me", "take this"]
wider = ["i will"]
[soft]
interjections = ["hmm", "well", "ah", "oh"]
archaic = ["thou", "thee"]
)";

Lexicon lexicon() {
  auto lex = parse_lexicon(kBanned, safety_toml());
  REQUIRE(lex);
  return std::move(*lex);
}

struct Lines {
  std::vector<Sentence> sentences;
  explicit Lines(const V& texts) {
    std::uint8_t i = 0;
    for (const std::string& t : texts) sentences.push_back(Sentence{t, i++, {}});
  }
};

std::vector<LintVerdict> lint(const SpeechLint& l, const LintContext& c, std::vector<Sentence>& sentences) {
  auto reply = l.open(c);
  std::vector<LintVerdict> out;
  for (Sentence& s : sentences) out.push_back(reply->check(s));
  return out;
}

LintVerdict one(const SpeechLint& l, const LintContext& c, std::string text) {
  Sentence s{std::move(text), 0, {}};
  return l.open(c)->check(s);
}

bool has(const LintVerdict& v, SoftHit h) { return std::find(v.soft.begin(), v.soft.end(), h) != v.soft.end(); }

}  // namespace

TEST_CASE("SHA-256 gives NIST's examples") {
  CHECK(speech::sha256_hex("") == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
  CHECK(speech::sha256_hex("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
  CHECK(speech::sha256_hex("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq") ==
        "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
  CHECK(speech::sha256_hex(std::string(1000000, 'a')) ==
        "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
}

TEST_CASE("the safety normalisation: case, accents, look-alikes, separators, spelling out, repeats") {
  CHECK(safety_words("Zorblat") == V{"zorblat"});
  CHECK(safety_words("Z\xC3\x96RBL\xC3\x80T") == V{"zorblat"});
  CHECK(safety_words("z0rbl4t") == V{"zorblat"});
  CHECK(safety_words("zooorrrblaaat!") == V{"zorblat"});
  CHECK(safety_words("z.o.r.b.l.a.t") == V{"zorblat"});
  CHECK(safety_words("zor-blat zor_blat zor'blat zor*blat") == V{"zorblat", "zorblat", "zorblat", "zorblat"});
  CHECK(safety_words("z o r b l a t") == V{"zorblat"});
  CHECK(safety_words("$orbl@t |n") == V{"sorblat", "in"});
  CHECK(safety_words("in 1000 years") == V{"in", "10", "years"});  // no letter: digits kept, squeezed
  CHECK(safety_words("Stra\xC3\x9F"
                     "e, \xC3\x86on.") == V{"strase", "aeon"});
  CHECK(safety_words("...!!! --") == V{});
  CHECK(safety_words("a b") == V{"a", "b"});  // two one-letter words are left alone
  CHECK(safety_fingerprint("zorblat") == speech::sha256_hex("eldennet.safety.v1:zorblat"));
  CHECK(safety_fingerprint("Quix  VANDLE") == speech::sha256_hex("eldennet.safety.v1:quix vandle"));
  CHECK(!safety_fingerprint("one two three four"));
  CHECK(!safety_fingerprint(" - "));
}

TEST_CASE("the safety list: fingerprints match a reply's words and runs, at both levels, and gore at gentle only") {
  const SpeechLint l(lexicon());
  LintContext c;
  for (const char* text : {"A zorblat stood there.", "Z0RBL4T!", "The z o r b l a t, yes.", "The quix vandle came."}) {
    CAPTURE(text);
    const LintVerdict v = one(l, c, text);
    CHECK(v.action == LintAction::Regenerate);
    CHECK(v.hit == HardList::Safety);
  }
  CHECK(one(l, c, "A quix came, and a vandle.").action == LintAction::Pass);  // not in a row
  CHECK(one(l, c, "Zorblatter.").action == LintAction::Pass);                 // a whole word only
  CHECK(one(l, c, "The grubbleflesh.").action == LintAction::Pass);           // faithful: gore passes
  c.safety = Safety::Gentle;
  CHECK(one(l, c, "The grubbleflesh.").hit == HardList::Safety);
}

TEST_CASE("the hard lists decide NPC 8.4's action by the sentence and the try") {
  const SpeechLint l(lexicon());
  LintContext c;
  CHECK(one(l, c, "A fine quest, that.") == LintVerdict{LintAction::Regenerate, HardList::Banned, {}});
  CHECK(one(l, c, "Follow me, then.").hit == HardList::Promise);
  CHECK(one(l, c, "Take this.").hit == HardList::Promise);
  CHECK(one(l, c, "They respawned at dusk.").hit == HardList::Banned);  // a prefix entry
  CHECK(one(l, c, "Level up, they said.").hit == HardList::Banned);
  CHECK(one(l, c, "OKAY.").hit == HardList::Banned);
  CHECK(one(l, c, "A questing knight.").action == LintAction::Pass);  // whole words only
  CHECK(one(l, c, "Follow the road, me.").action == LintAction::Pass);
  CHECK(one(l, c, "Follow me to the zorblat.").hit == HardList::Safety);  // the worst first
  c.regeneration = true;
  CHECK(one(l, c, "A fine quest.").action == LintAction::Deflect);
  c.regeneration = false;
  Lines lines({"The tide turns.", "A quest awaits.", "Never shown."});
  const std::vector<LintVerdict> v = lint(l, c, lines.sentences);
  CHECK(v[0].action == LintAction::Pass);
  CHECK(v[1] == LintVerdict{LintAction::Stop, HardList::Banned, {}});
  CHECK(v[2] == LintVerdict{LintAction::Stop, HardList::Banned, {}});  // the reply has ended
}

TEST_CASE("openers: one from the persona's set, none from the global set it lacks, not the last reply's") {
  const SpeechLint l(lexicon());
  const V openers = {"Ah", "Hmm"};
  LintContext c;
  c.openers = openers;
  Sentence s{"Well, the tide turns.", 0, {}};
  auto reply = l.open(c);
  LintVerdict v = reply->check(s);
  CHECK(v.action == LintAction::Pass);
  CHECK(has(v, SoftHit::Opener));
  CHECK(s.text == "The tide turns.");
  CHECK(!reply->tally().opener);

  s = Sentence{"Ah, hmm, the tide.", 0, {}};
  reply = l.open(c);
  v = reply->check(s);
  CHECK(s.text == "Ah, the tide.");
  CHECK(has(v, SoftHit::Opener));
  CHECK(reply->tally().opener == 0);

  LintContext next = c;
  next.tally = reply->tally();
  s = Sentence{"Ah, again.", 0, {}};
  reply = l.open(next);
  v = reply->check(s);
  CHECK(has(v, SoftHit::RepeatedOpener));
  CHECK(s.text == "Again.");
  CHECK(!reply->tally().opener);

  // A sentence of interjections alone: a kept one keeps the next sentence leading; a stripped one is dropped.
  reply = l.open(c);
  Sentence a{"Hmm.", 0, {}};
  Sentence b{"Well, no.", 1, {}};
  CHECK(reply->check(a).action == LintAction::Pass);
  v = reply->check(b);
  CHECK(has(v, SoftHit::Opener));
  CHECK(b.text == "No.");
  reply = l.open(c);
  Sentence d{"Oh.", 0, {}};
  CHECK(reply->check(d).action == LintAction::Drop);
  CHECK(one(l, c, "Well met, stranger.").soft.empty());  // no punctuation after it: no interjection
}

TEST_CASE("the other soft items: duplicates, tics, the told name, wider promises, nouns, archaic, third person") {
  const SpeechLint l(lexicon());
  LintContext c;
  Lines dup({"The weir holds.", "the weir holds!"});
  const std::vector<LintVerdict> d = lint(l, c, dup.sentences);
  CHECK(d[1].action == LintAction::Drop);
  CHECK(has(d[1], SoftHit::Duplicate));

  const V tics = {"by the reeds"};
  const std::vector<std::uint8_t> max = {1};
  c.tics = tics;
  c.tic_max = max;
  auto reply = l.open(c);
  Sentence s1{"By the reeds, it is cold.", 0, {}};
  Sentence s2{"It is cold by the reeds.", 1, {}};
  CHECK(!has(reply->check(s1), SoftHit::TicOverCap));
  CHECK(has(reply->check(s2), SoftHit::TicOverCap));
  CHECK(reply->tally().tics[0] == 2);

  c = LintContext{};
  c.told_name = "Odwen";
  reply = l.open(c);
  Sentence n1{"Odwen, come in.", 0, {}};
  Sentence n2{"Sit, Odwen.", 1, {}};
  CHECK(!has(reply->check(n1), SoftHit::NameTwice));
  CHECK(has(reply->check(n2), SoftHit::NameTwice));
  CHECK(reply->tally().name_uses == 2);

  c = LintContext{};
  CHECK(has(one(l, c, "I will think on it."), SoftHit::WiderPromise));
  const V names = {"the Saltmarsh Weir"};
  c.names = names;
  CHECK(has(one(l, c, "I saw Velkar there."), SoftHit::UnknownNoun));
  CHECK(!has(one(l, c, "I saw Saltmarsh there, and I'll go."), SoftHit::UnknownNoun));
  CHECK(!has(one(l, c, "Velkar saw it."), SoftHit::UnknownNoun));  // the first word
  CHECK(!has(one(l, c, "It said NO."), SoftHit::UnknownNoun));     // capitals throughout
  CHECK(has(one(l, c, "Thou art late."), SoftHit::Archaic));
  c.archaic = true;
  CHECK(!has(one(l, c, "Thou art late."), SoftHit::Archaic));
  const V self = {"Brannoch"};
  c.self_names = self;
  CHECK(has(one(l, c, "Brannoch does not forget."), SoftHit::ThirdPerson));
  CHECK(!has(one(l, c, "I am Brannoch."), SoftHit::ThirdPerson));
  CHECK(!has(one(l, c, "They call me Brannoch."), SoftHit::ThirdPerson));
}

TEST_CASE("the deflection ring: least recently used first, never-used in file order, a saved order kept") {
  DeflectionRing ring(3);
  CHECK(ring.next() == 0);
  CHECK(ring.next() == 1);
  CHECK(ring.next() == 2);
  CHECK(ring.next() == 0);
  CHECK(ring.order() == std::vector<std::uint8_t>{1, 2, 0});
  DeflectionRing saved(3, {2, 0, 1});
  CHECK(saved.next() == 2);
  DeflectionRing broken(3, {0, 0, 1});
  CHECK(broken.next() == 0);
  DeflectionRing none;
  CHECK(!none.next());
}

TEST_CASE("the lexicon's reader: its keys, its format, and errors that name a key, never an entry") {
  CHECK(parse_lexicon(kBanned, "format = 1\n"));
  CHECK(parse_lexicon("format = 1\n", "format = 1\nalways = []\ngentle = []\n"));
  const auto no_format = parse_lexicon("[hard]\nx = []\n", "format = 1\n");
  REQUIRE(!no_format);
  CHECK(no_format.error().file == "banned.toml");
  const auto unknown = parse_lexicon("format = 1\n[soft]\nmystery = [\"secretword\"]\n", "format = 1\n");
  REQUIRE(!unknown);
  CHECK(unknown.error().problem == "has an unknown key soft.mystery");
  const auto star = parse_lexicon("format = 1\n[hard]\nx = [\"a*b\"]\n", "format = 1\n");
  REQUIRE(!star);
  CHECK(star.error().problem.find("a*b") == std::string::npos);
  const auto not_hex = parse_lexicon("format = 1\n", "format = 1\nalways = [\"plainword\"]\n");
  REQUIRE(!not_hex);
  CHECK(not_hex.error().file == "safety.toml");
  CHECK(not_hex.error().problem.find("plainword") == std::string::npos);
  const std::string fp = *safety_fingerprint("zorblat");
  CHECK(!parse_lexicon("format = 1\n", "format = 1\nalways = [\"" + fp + "\", \"" + fp + "\"]\n"));
  CHECK(!parse_lexicon("format = 2\n", "format = 1\n"));
  CHECK(lint_words("I'll go \xE2\x80\x94 Caf\xC3\xA9's end, 'twas") == V{"i'll", "go", "cafe's", "end", "twas"});
}
