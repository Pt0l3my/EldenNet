#pragma once
// Health per (provider, model) and per provider (ARCH §11.2), what the client learns about each during a session,
// and the requests-per-hour budget (ARCH §11.4).
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "en/llm/clock.hpp"
#include "en/llm/transport.hpp"

namespace en::llm {

inline constexpr int kFailuresToCool = 3;
inline constexpr Millis kPairCooldown{60000};
inline constexpr Millis kRateLimitCooldown{60000};
inline constexpr Millis kRetryAfterCap{300000};

struct HealthReport {
  struct Pair {
    std::string provider;
    std::string model;
    int failures = 0;
    Millis cooling_left{0};
    bool minimal_effort = false;     // refused reasoning off: starts at the lowest effort
    bool full_body_refused = false;  // only a stripped body works: starts stripped
  };
  struct Provider {
    std::string id;
    Millis cooling_left{0};
    bool disabled = false;  // its key was rejected; cleared when the configuration is reloaded
    bool schema_refused = false;
    bool logprobs_refused = false;
  };
  std::vector<Pair> pairs;
  std::vector<Provider> providers;
};

class Health {
 public:
  explicit Health(std::shared_ptr<Clock> clock) : clock_(std::move(clock)) {}

  bool provider_usable(const std::string& provider) const;
  bool pair_healthy(const std::string& provider, const std::string& model) const;

  // A counted failure. Returns true when it started the pair's cooldown (three in a row).
  bool record_failure(const std::string& provider, const std::string& model);
  void record_success(const std::string& provider, const std::string& model);
  void rate_limited(const std::string& provider, Millis cooldown);
  void auth_failed(const std::string& provider);
  void clear_disabled();

  // Each note_ returns true the first time, so that the log says it once (ARCH §11.2).
  bool note_minimal_effort(const std::string& provider, const std::string& model);
  bool minimal_effort(const std::string& provider, const std::string& model) const;
  bool note_full_body_refused(const std::string& provider, const std::string& model);
  bool full_body_refused(const std::string& provider, const std::string& model) const;
  bool note_schema_refused(const std::string& provider);
  bool schema_refused(const std::string& provider) const;
  bool note_logprobs_refused(const std::string& provider);
  bool logprobs_refused(const std::string& provider) const;

  HealthReport report() const;

 private:
  struct PairState {
    int failures = 0;
    TimePoint cooling_until{};
    bool minimal_effort = false;
    bool full_body_refused = false;
  };
  struct ProviderState {
    TimePoint cooling_until{};
    bool disabled = false;
    bool schema_refused = false;
    bool logprobs_refused = false;
  };
  using PairKey = std::pair<std::string, std::string>;

  std::shared_ptr<Clock> clock_;
  mutable std::mutex mutex_;
  std::map<PairKey, PairState> pairs_;
  std::map<std::string, ProviderState> providers_;
};

// How long a 429 cools its provider: Retry-After's seconds where given (at most 300 s), otherwise 60 s.
Millis rate_limit_cooldown(const std::vector<HttpHeader>& headers);

// Requests sent in a rolling hour, retries, backups and decision calls included (ARCH §11.4).
class RequestBudget {
 public:
  RequestBudget(std::shared_ptr<Clock> clock, int per_hour) : clock_(std::move(clock)), limit_(per_hour) {}
  // Counts one request and returns true, or returns false and counts nothing when the bound is reached; with
  // `always` (a reply: the player asked) it counts and returns true even past the bound.
  bool take(bool always);
  int used() const;
  int limit() const;
  void set_limit(int per_hour);

 private:
  void prune(TimePoint now) const;

  std::shared_ptr<Clock> clock_;
  mutable std::mutex mutex_;
  mutable std::deque<TimePoint> sent_;
  int limit_;
};

}  // namespace en::llm
