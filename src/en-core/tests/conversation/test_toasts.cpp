// The toasts' policy (NPC §10.5) on a simulated clock: two up at once for 6 s, one per error class per minute, held in
// a boss fight, a queue of eight that drops the oldest info toast first, error before degradation before info.
#include <doctest.h>

#include <string>

#include "en/core/toasts.hpp"

using namespace en;
using namespace en::core;
using proto::ToastLevel;

namespace {

std::vector<std::string> texts(const Toasts& t) {
  std::vector<std::string> out;
  for (const proto::Toast& x : t.visible()) out.push_back(x.text);
  return out;
}

}  // namespace

TEST_CASE("two up at once, each for 6 s, with ids unique in the session") {
  Toasts t;
  CHECK(t.post(ToastClass::Status, ToastLevel::Info, "a", 0));
  CHECK(t.post(ToastClass::Status, ToastLevel::Info, "b", 0));
  CHECK(t.post(ToastClass::Status, ToastLevel::Info, "c", 0));
  t.step(0, false);
  CHECK(texts(t) == std::vector<std::string>{"a", "b"});
  CHECK(t.visible()[0].id != t.visible()[1].id);
  t.step(5999, false);
  CHECK(texts(t).size() == 2);
  t.step(6000, false);
  CHECK(texts(t) == std::vector<std::string>{"c"});
  CHECK_FALSE(t.post(ToastClass::Status, ToastLevel::Info, "c", 6000));  // up already: not twice
  t.step(12000, false);
  CHECK(texts(t).empty());
}

TEST_CASE("one per error class per minute") {
  Toasts t;
  CHECK(t.post(ToastClass::Timeout, ToastLevel::Error, "No answer: provider timed out", 0));
  t.step(0, false);
  t.step(7000, false);
  CHECK_FALSE(t.post(ToastClass::Timeout, ToastLevel::Error, "No answer: provider timed out", 30000));
  CHECK(t.post(ToastClass::Auth, ToastLevel::Error, "API key rejected: paste it again on AI › Providers", 30000));  // another class
  CHECK(t.post(ToastClass::Timeout, ToastLevel::Error, "No answer: provider timed out", 60000));
}

TEST_CASE("held while a boss fight lasts, shown when it ends") {
  Toasts t;
  t.post(ToastClass::Danger, ToastLevel::Info, "Not now: danger is near.", 0);
  t.step(0, true);
  CHECK(texts(t).empty());
  t.step(20000, true);
  CHECK(t.waiting() == 1);
  t.step(21000, false);
  CHECK(texts(t) == std::vector<std::string>{"Not now: danger is near."});
}

TEST_CASE("the queue holds eight, dropping the oldest info toast first; error before degradation before info") {
  Toasts t;
  for (int i = 0; i < 7; ++i) t.post(ToastClass::Status, ToastLevel::Info, "info " + std::to_string(i), 0);
  t.post(ToastClass::Degraded, ToastLevel::Warning, "degraded", 0);
  CHECK(t.waiting() == 8);
  CHECK(t.post(ToastClass::Fault, ToastLevel::Error, "error", 0));
  CHECK(t.waiting() == 8);  // "info 0" went
  t.step(0, false);
  CHECK(texts(t) == std::vector<std::string>{"error", "degraded"});
  t.step(6000, false);
  CHECK(texts(t) == std::vector<std::string>{"info 1", "info 2"});
}

TEST_CASE("a status toast may ask for a shorter hold") {
  Toasts t;
  t.post(ToastClass::Status, ToastLevel::Info, "They have nothing to say to you.", 0, 2000);
  t.step(0, false);
  t.step(1999, false);
  CHECK(texts(t).size() == 1);
  t.step(2000, false);
  CHECK(texts(t).empty());
}
