// The journal's form and block 8 (memory.hpp; MEM §2.2, §2.3, §2.4, §4.3, §4.4).
#include <algorithm>
#include <cmath>
#include <nlohmann/json.hpp>

#include "en/core/facts.hpp"
#include "en/core/memory.hpp"

namespace en::core {

namespace {

using json = nlohmann::json;

constexpr proto::Pt kHour = 3600000;
constexpr proto::Pt kShortWhile = 600000;  // "only a short while ago": within 10 min of play

std::optional<std::int64_t> num(const proto::PersonaFacts* f, std::string_view key) {
  return f != nullptr ? fact_number(*f, key) : std::nullopt;
}

bool verified(const proto::PersonaFacts* f, const std::string& check_key, const BlockNow& now) {
  if (now.proved) return now.proved(check_key);
  if (f == nullptr) return false;
  const proto::FactRow* r = find_fact(*f, "verified:" + check_key);
  return r != nullptr && (r->value == "1" || r->value == "true");
}

// MEM §2.2's words, in the table's order, after the told name.
std::vector<std::string> fact_words(const proto::PersonaFacts* f, const Journal* j, const BlockNow& now,
                                    std::size_t& kept_core) {
  std::vector<std::string> out;
  if (j != nullptr && j->told_name && !j->told_name->empty()) {
    out.push_back(j->told_name_verified ? "They told you their name is " + *j->told_name + "."
                                        : "They asked you to call them " + *j->told_name + ".");
  }
  const std::int64_t met = num(f, fact::kMetCount).value_or(0);
  if (met == 0) {
    out.emplace_back("You have never spoken with this Tarnished.");
  } else if (met == 1) {
    out.emplace_back("You have spoken with the Tarnished once before.");
  } else if (met <= 4) {
    out.emplace_back("You have spoken with the Tarnished a few times.");
  } else {
    out.emplace_back("You have spoken with the Tarnished many times.");
  }
  if (const auto attacked = num(f, fact::kTimesAttacked); attacked && *attacked > 0) {
    std::string s = *attacked == 1   ? "The Tarnished struck you once"
                    : *attacked <= 3 ? "The Tarnished struck you more than once"
                                     : "The Tarnished struck you many times";
    const auto last = num(f, fact::kLastAttackedPt);
    if (last && now.pt >= *last && now.pt - *last <= kShortWhile) s += ", only a short while ago";
    out.push_back(s + ".");
  }
  kept_core = out.size();  // the name, the meetings and the attacks are never trimmed (MEM §4.4)
  if (const auto last = num(f, fact::kLastMetPt); met > 0 && last && now.pt > *last) {
    const proto::Pt gap = now.pt - *last;
    if (gap > 6 * kHour) {
      out.emplace_back("A long while has passed since you two last talked.");
    } else if (gap >= kHour) {
      out.emplace_back("Some time has passed since you two last talked.");
    }
  }
  if (const auto seen = num(f, fact::kSeenCount); met == 0 && seen && *seen >= 2) {
    out.emplace_back("You have seen this Tarnished pass by, but you have never spoken.");
  }
  if (const auto fought = num(f, fact::kFoughtBeside); fought && *fought > 0) {
    const auto boss = num(f, fact::kFoughtLastBoss);
    const auto name = boss ? now.boss_names.find(static_cast<std::int32_t>(*boss)) : now.boss_names.end();
    if (name != now.boss_names.end() && !name->second.empty()) {
      out.push_back(*fought == 1 ? "You fought beside the Tarnished against " + name->second + "."
                                 : "You fought beside the Tarnished more than once, most recently against " +
                                       name->second + ".");
    } else {
      out.emplace_back(*fought == 1 ? "You fought beside the Tarnished."
                                    : "You fought beside the Tarnished more "
                                      "than once.");
    }
  }
  if (const auto fell = num(f, fact::kSeenPlayerDie); fell && *fell > 0) {
    out.emplace_back(*fell == 1 ? "You have seen the Tarnished fall once, and come back."
                                : "You have seen the Tarnished fall more than once, and come back.");
  }
  return out;
}

std::string disposition_words(std::int8_t d, std::int64_t met) {
  switch (std::clamp<int>(d, -2, 2)) {
    case -2:
      return "You distrust the Tarnished and would sooner they moved on.";
    case -1:
      return "You are wary of the Tarnished.";
    case 1:
      return "You have come to think fairly well of the Tarnished.";
    case 2:
      return "You trust the Tarnished, as far as you trust anyone.";
    default:
      return met >= 1 ? "You have not made up your mind about the Tarnished." : std::string();
  }
}

double rounded(double w) { return std::round(std::clamp(w, 0.0, 1.0) * 20.0) / 20.0; }

}  // namespace

std::uint32_t estimate_tokens(std::size_t bytes) noexcept {
  return static_cast<std::uint32_t>(std::ceil(static_cast<double>(bytes) / 3.6 * 1.15));
}

std::optional<Journal> journal_of(const proto::JournalVersion& v) {
  Journal j;
  j.version = v.version;
  j.summary = v.summary;
  j.disposition = static_cast<std::int8_t>(std::clamp<int>(v.disposition, -2, 2));
  j.told_name = v.told_name;
  j.told_name_verified = v.told_name_verified;
  j.told_name_heard = v.told_name_heard;
  j.as_of_pt = v.as_of_pt;
  j.upto_event = v.upto_event_id;
  const json a = json::parse(v.keepsakes, nullptr, false);
  if (!a.is_array()) return std::nullopt;
  for (const json& k : a) {
    if (!k.is_object() || !k.contains("id") || !k.contains("text")) return std::nullopt;
    Keepsake s;
    s.id = k.value("id", std::string());
    s.text = k.value("text", std::string());
    s.basis = k.value("basis", std::string("said")) == "seen" ? Keepsake::Basis::Seen : Keepsake::Basis::Said;
    s.weight = rounded(k.value("weight", 0.5));
    s.about = k.value("about", std::string("tarnished"));
    if (k.contains("check_key") && k["check_key"].is_string()) s.check_key = k["check_key"].get<std::string>();
    if (k.contains("refs") && k["refs"].is_array()) {
      for (const json& r : k["refs"]) {
        if (r.is_string()) s.refs.push_back(r.get<std::string>());
      }
    }
    s.since_pt = k.value("since_pt", std::int64_t{0});
    s.updated_pt = k.value("updated_pt", std::int64_t{0});
    j.keepsakes.push_back(std::move(s));
  }
  return j;
}

std::string keepsakes_json(const std::vector<Keepsake>& keepsakes) {
  json a = json::array();
  for (const Keepsake& k : keepsakes) {
    json o = {{"id", k.id},
              {"text", k.text},
              {"basis", k.basis == Keepsake::Basis::Seen ? "seen" : "said"},
              {"weight", rounded(k.weight)},
              {"about", k.about},
              {"refs", k.refs},
              {"since_pt", k.since_pt},
              {"updated_pt", k.updated_pt}};
    o["check_key"] = k.check_key ? json(*k.check_key) : json(nullptr);
    a.push_back(std::move(o));
  }
  return a.dump();
}

std::optional<std::size_t> protected_keepsake(const Journal& j) noexcept {
  std::optional<std::size_t> best;
  for (std::size_t i = 0; i < j.keepsakes.size(); ++i) {
    const Keepsake& k = j.keepsakes[i];
    if (k.about != "tarnished" || k.weight < 0.7 - 1e-9) continue;
    if (!best) {
      best = i;
      continue;
    }
    const Keepsake& b = j.keepsakes[*best];
    if (k.weight > b.weight + 1e-9 || (std::abs(k.weight - b.weight) < 1e-9 && k.since_pt < b.since_pt)) best = i;
  }
  return best;
}

std::uint32_t block_tokens(const TarnishedBlock& b) noexcept {
  std::size_t bytes = std::string_view("[The Tarnished, as you know them]\n").size();
  for (const std::string& f : b.facts) bytes += f.size() + 1;
  if (!b.disposition.empty()) bytes += b.disposition.size() + 1;
  if (!b.summary.empty()) bytes += b.summary.size() + std::string_view("In your own words: \"\"\n").size();
  const auto group = [&](const std::vector<TarnishedBlock::Line>& g, std::string_view header) {
    if (g.empty()) return;
    bytes += header.size() + 1;
    for (const auto& l : g) bytes += l.text.size() + 3;
  };
  group(b.seen, "You saw for yourself:");
  group(b.proved, "The Tarnished told you, and it has proved true:");
  group(b.said, "The Tarnished told you (their word only, not proof):");
  return estimate_tokens(bytes);
}

TarnishedBlock tarnished_block(const proto::PersonaFacts* facts, const Journal* journal, const BlockNow& now) {
  TarnishedBlock b;
  std::size_t kept_core = 0;
  b.facts = fact_words(facts, journal, now, kept_core);
  const std::int64_t met = num(facts, fact::kMetCount).value_or(0);
  if (journal != nullptr) {
    b.disposition = disposition_words(journal->disposition, met);
    b.summary = journal->summary;
    const auto prot = protected_keepsake(*journal);
    std::vector<std::size_t> order(journal->keepsakes.size());
    for (std::size_t i = 0; i < order.size(); ++i) order[i] = i;
    // Inside a group: weight descending, then the newer first.
    std::stable_sort(order.begin(), order.end(), [&](std::size_t x, std::size_t y) {
      const Keepsake& a = journal->keepsakes[x];
      const Keepsake& c = journal->keepsakes[y];
      if (std::abs(a.weight - c.weight) > 1e-9) return a.weight > c.weight;
      return a.updated_pt > c.updated_pt;
    });
    for (const std::size_t i : order) {
      const Keepsake& k = journal->keepsakes[i];
      TarnishedBlock::Line line{k.text, k.weight, prot && *prot == i, k.check_key.value_or(std::string())};
      if (k.basis == Keepsake::Basis::Seen) {
        b.seen.push_back(std::move(line));
      } else if (k.check_key && verified(facts, *k.check_key, now)) {
        b.proved.push_back(std::move(line));
      } else {
        b.said.push_back(std::move(line));
      }
    }
  }
  // The lightest unprotected keepsake, each group's being at its end.
  const auto drop_lightest = [&b]() -> bool {
    std::vector<TarnishedBlock::Line>* group = nullptr;
    std::size_t at = 0;
    for (auto* g : {&b.seen, &b.proved, &b.said}) {
      for (std::size_t i = g->size(); i-- > 0;) {
        if ((*g)[i].protect) continue;
        if (group == nullptr || (*g)[i].weight < (*group)[at].weight) {
          group = g;
          at = i;
        }
        break;
      }
    }
    if (group == nullptr) return false;
    group->erase(group->begin() + static_cast<std::ptrdiff_t>(at));
    return true;
  };
  // The 6 heaviest and the protected one, at most 7 (MEM §4.2).
  while (b.seen.size() + b.proved.size() + b.said.size() > 7 && drop_lightest()) {
  }
  // MEM §4.4: keepsakes by lowest weight down to the protected one, then the fact sentences other than the core ones.
  while (block_tokens(b) > kBlock8Cap) {
    if (drop_lightest()) continue;
    if (b.facts.size() > kept_core) {
      b.facts.pop_back();
      continue;
    }
    break;
  }
  return b;
}

}  // namespace en::core
