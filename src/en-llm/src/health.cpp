#include "en/llm/health.hpp"

#include <algorithm>

namespace en::llm {

namespace {

Millis left_until(TimePoint until, TimePoint now) {
  return until > now ? std::chrono::duration_cast<Millis>(until - now) : Millis(0);
}

}  // namespace

bool Health::provider_usable(const std::string& provider) const {
  std::lock_guard lock(mutex_);
  auto it = providers_.find(provider);
  if (it == providers_.end()) return true;
  return !it->second.disabled && clock_->now() >= it->second.cooling_until;
}

bool Health::pair_healthy(const std::string& provider, const std::string& model) const {
  if (!provider_usable(provider)) return false;
  std::lock_guard lock(mutex_);
  auto it = pairs_.find({provider, model});
  return it == pairs_.end() || clock_->now() >= it->second.cooling_until;
}

bool Health::record_failure(const std::string& provider, const std::string& model) {
  std::lock_guard lock(mutex_);
  PairState& pair = pairs_[{provider, model}];
  if (++pair.failures < kFailuresToCool) return false;
  // The count starts again after the cooldown, so the pair gets three fresh tries once it is back.
  pair.failures = 0;
  pair.cooling_until = clock_->now() + kPairCooldown;
  return true;
}

void Health::record_success(const std::string& provider, const std::string& model) {
  std::lock_guard lock(mutex_);
  pairs_[{provider, model}].failures = 0;
}

void Health::rate_limited(const std::string& provider, Millis cooldown) {
  std::lock_guard lock(mutex_);
  ProviderState& state = providers_[provider];
  state.cooling_until = std::max(state.cooling_until, clock_->now() + cooldown);
}

void Health::auth_failed(const std::string& provider) {
  std::lock_guard lock(mutex_);
  providers_[provider].disabled = true;
}

void Health::clear_disabled() {
  std::lock_guard lock(mutex_);
  for (auto& [id, state] : providers_) state.disabled = false;
}

bool Health::note_minimal_effort(const std::string& provider, const std::string& model) {
  std::lock_guard lock(mutex_);
  return !std::exchange(pairs_[{provider, model}].minimal_effort, true);
}

bool Health::minimal_effort(const std::string& provider, const std::string& model) const {
  std::lock_guard lock(mutex_);
  auto it = pairs_.find({provider, model});
  return it != pairs_.end() && it->second.minimal_effort;
}

bool Health::note_full_body_refused(const std::string& provider, const std::string& model) {
  std::lock_guard lock(mutex_);
  return !std::exchange(pairs_[{provider, model}].full_body_refused, true);
}

bool Health::full_body_refused(const std::string& provider, const std::string& model) const {
  std::lock_guard lock(mutex_);
  auto it = pairs_.find({provider, model});
  return it != pairs_.end() && it->second.full_body_refused;
}

bool Health::note_schema_refused(const std::string& provider) {
  std::lock_guard lock(mutex_);
  return !std::exchange(providers_[provider].schema_refused, true);
}

bool Health::schema_refused(const std::string& provider) const {
  std::lock_guard lock(mutex_);
  auto it = providers_.find(provider);
  return it != providers_.end() && it->second.schema_refused;
}

bool Health::note_logprobs_refused(const std::string& provider) {
  std::lock_guard lock(mutex_);
  return !std::exchange(providers_[provider].logprobs_refused, true);
}

bool Health::logprobs_refused(const std::string& provider) const {
  std::lock_guard lock(mutex_);
  auto it = providers_.find(provider);
  return it != providers_.end() && it->second.logprobs_refused;
}

HealthReport Health::report() const {
  std::lock_guard lock(mutex_);
  const TimePoint now = clock_->now();
  HealthReport report;
  for (const auto& [key, state] : pairs_) {
    report.pairs.push_back({key.first, key.second, state.failures, left_until(state.cooling_until, now),
                            state.minimal_effort, state.full_body_refused});
  }
  for (const auto& [id, state] : providers_) {
    report.providers.push_back({id, left_until(state.cooling_until, now), state.disabled, state.schema_refused,
                                state.logprobs_refused});
  }
  return report;
}

Millis rate_limit_cooldown(const std::vector<HttpHeader>& headers) {
  for (const HttpHeader& header : headers) {
    if (header.name != "retry-after") continue;
    const std::string& value = header.value;
    const auto first = value.find_first_not_of(' ');
    if (first == std::string::npos) break;
    std::size_t end = first;
    while (end < value.size() && value[end] >= '0' && value[end] <= '9') ++end;
    if (end == first || end - first > 6) break;  // an HTTP date, or nonsense: the default stands
    const Millis asked = std::chrono::seconds(std::stol(value.substr(first, end - first)));
    return std::clamp(asked, Millis(1000), kRetryAfterCap);
  }
  return kRateLimitCooldown;
}

void RequestBudget::prune(TimePoint now) const {
  while (!sent_.empty() && now - sent_.front() >= std::chrono::hours(1)) sent_.pop_front();
}

bool RequestBudget::take(bool always) {
  std::lock_guard lock(mutex_);
  const TimePoint now = clock_->now();
  prune(now);
  if (!always && static_cast<int>(sent_.size()) >= limit_) return false;
  sent_.push_back(now);
  return true;
}

int RequestBudget::used() const {
  std::lock_guard lock(mutex_);
  prune(clock_->now());
  return static_cast<int>(sent_.size());
}

int RequestBudget::limit() const {
  std::lock_guard lock(mutex_);
  return limit_;
}

void RequestBudget::set_limit(int per_hour) {
  std::lock_guard lock(mutex_);
  limit_ = per_hour;
}

}  // namespace en::llm
