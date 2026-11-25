// Tools › Workshop's topics (workshop.hpp). Every word a player reads here is a draft for the owner's review, but for
// the section titles, the description, the help and the gate (space.toml), which the owner gave (O129).
#include "en/overlay/workshop.hpp"

#include <algorithm>
#include <array>
#include <cstdio>

#include "en/overlay/space.hpp"
#include "en/proto/calls.hpp"

namespace en::overlay {

namespace {

using nlohmann::json;

constexpr std::size_t kShareMax = 256 * 1024;  // bytes of a share's text (GraceUI D263)
constexpr std::string_view kShareFile = "eldennet-unknown-characters.txt";

constexpr std::array<std::string_view, 7> kTopics = {kWorkshopExchange, kWorkshopEvents, kWorkshopWorld, kWorkshopKnown,
                                                     kWorkshopUnknown,  kWorkshopStatus, kWorkshopKinds};

json item(std::string_view label, std::string value) { return json{{"label", label}, {"value", std::move(value)}}; }

json column(std::string_view id, std::string_view label, bool numeric = false) {
  json c{{"id", id}, {"label", label}};
  if (numeric) c["numeric"] = true;
  return c;
}

json field(std::string_view label, std::string value) {
  return json{{"label", label}, {"value", std::move(value)}, {"code", true}};
}

json part(std::string id, std::string label, json fields) {
  return json{{"id", std::move(id)}, {"label", std::move(label)}, {"optional", false}, {"included", true},
              {"fields", std::move(fields)}};
}

std::string yes(bool b) { return b ? "yes" : "no"; }

std::string seconds(std::int64_t ms) {
  char text[32];
  std::snprintf(text, sizeof text, "%.2f s", static_cast<double>(ms) / 1000.0);
  return text;
}

std::string metres(float m) {
  char text[32];
  std::snprintf(text, sizeof text, "%.1f m", static_cast<double>(m));
  return text;
}

std::string grouped(std::uint64_t n) {
  std::string digits = std::to_string(n);
  for (std::size_t at = digits.size(); at > 3; at -= 3) digits.insert(at - 3, ",");
  return digits;
}

std::string bytes_of(std::uint64_t n) {
  char text[32];
  if (n < 1024 * 1024) {
    std::snprintf(text, sizeof text, "%.1f KB", static_cast<double>(n) / 1024.0);
  } else {
    std::snprintf(text, sizeof text, "%.1f MB", static_cast<double>(n) / (1024.0 * 1024.0));
  }
  return text;
}

// Rows taken while the body stays within kWorkshopBodyMax (GraceUI's frame to a page, less room for the rest): `room`
// bytes for the rows; the rows left out counted in `cut`.
json fitted(const std::vector<json>& rows, std::size_t room, std::size_t& cut) {
  json out = json::array();
  std::size_t used = 2;
  for (const json& r : rows) {
    const std::size_t size = r.dump(-1, ' ', false, json::error_handler_t::replace).size() + 1;
    if (used + size > room) {
      ++cut;
      continue;
    }
    used += size;
    out.push_back(r);
  }
  return out;
}

std::string role_words(std::string_view role) {
  if (role == "system") return "System";
  if (role == "assistant") return "Assistant";
  if (role == "user") return "User";
  return std::string(role);
}

// The text of the prompt or the reply, its words shaped like a path or a local address scrubbed again (PV5).
std::string shown(std::string_view text) { return words_scrubbed(text); }

json exchange_body(const proto::WorkshopExchange& x) {
  if (!x.any) return json::object();
  json items = json::array();
  items.push_back(item("Job", x.slot));
  items.push_back(item("Purpose", x.purpose));
  items.push_back(item("Sent", clock_of(x.at_ms)));
  items.push_back(item("Provider", x.provider.empty() ? std::string("Not sent yet") : x.provider));
  if (!x.model.empty()) items.push_back(item("Model", x.model));
  if (!x.served_model.empty() && x.served_model != x.model) items.push_back(item("Answered by", x.served_model));
  std::string outcome = !x.ended ? "Waiting" : x.error == proto::ErrorClass::None ? "Answered" : "Failed";
  if (x.ended && x.error != proto::ErrorClass::None) {
    const char* why = proto::error_words(x.error, x.http_status);
    if (*why != '\0') outcome += ": " + std::string(why);
  }
  items.push_back(item("Outcome", outcome));
  if (!x.finish.empty()) items.push_back(item("Finish reason", x.finish));
  if (x.first_token_ms >= 0) items.push_back(item("First words", seconds(x.first_token_ms)));
  if (x.ended && x.total_ms >= 0) items.push_back(item("Total", seconds(x.total_ms)));
  std::string tokens;
  if (x.tokens_in >= 0) tokens += grouped(static_cast<std::uint64_t>(x.tokens_in)) + " in";
  if (x.tokens_out >= 0) {
    tokens += (tokens.empty() ? "" : ", ") + grouped(static_cast<std::uint64_t>(x.tokens_out)) + " out";
  }
  if (x.tokens_cached >= 0) {
    tokens += (tokens.empty() ? "" : ", ") + grouped(static_cast<std::uint64_t>(x.tokens_cached)) + " cached";
  }
  if (!tokens.empty()) items.push_back(item("Tokens", tokens));
  if (x.ended) items.push_back(item("Attempts", std::to_string(x.attempts)));
  std::string asked = x.max_tokens >= 0 ? "at most " + std::to_string(x.max_tokens) + " tokens" : "the job's length";
  if (x.temperature >= 0) {
    char t[32];
    std::snprintf(t, sizeof t, "%.2g", x.temperature);
    asked += "; temperature " + std::string(t);
  }
  if (x.stops > 0) asked += "; " + std::to_string(x.stops) + (x.stops == 1 ? " stop sequence" : " stop sequences");
  if (x.schema) asked += "; a JSON schema";
  items.push_back(item("Asked for", asked));
  if (x.messages_cut > 0 || x.text_cut > 0 || x.raw_cut > 0) {
    items.push_back(item("Left out", grouped(x.messages_cut) + " bytes of the prompt, " +
                                         grouped(x.text_cut + x.raw_cut) + " of the reply"));
  }
  json parts = json::array();
  for (std::size_t i = 0; i < x.messages.size(); ++i) {
    const proto::WorkshopMessage& m = x.messages[i];
    std::string label = std::to_string(i + 1) + ". " + role_words(m.role);
    if (m.cache_break) label += " · a cache mark after it";
    parts.push_back(part("m" + std::to_string(i), std::move(label), json::array({field("Text", shown(m.text))})));
  }
  json reply = json::array();
  reply.push_back(field("Text", x.text.empty() ? std::string(x.ended ? "Nothing." : "No words yet.") : shown(x.text)));
  if (!x.raw.empty()) reply.push_back(field("As the model wrote it", shown(x.raw)));
  parts.push_back(part("reply", "Reply", std::move(reply)));
  return json{{"facts", {{"items", std::move(items)}}}, {"parts", {{"parts", std::move(parts)}}}};
}

json world_body(const proto::WorkshopWorld& w) {
  json stages = json::array();
  for (const proto::WorkshopStage& s : w.stages) {
    stages.push_back(json{{"id", s.id}, {"value", s.value}, {"when", s.when}, {"boss", s.boss},
                          {"spoiler", s.spoiler}, {"rank", s.rank}});
  }
  std::vector<json> flag_rows;
  for (const proto::WorkshopFlag& f : w.flags) {
    json row{{"alias", f.alias},
             {"id", f.id},
             {"watched", yes(f.watched)},
             {"bit", f.bit ? "1" : "0"},
             {"valid", yes(f.valid)},
             {"verified_file", yes(f.verified_file)},
             {"verified_here", yes(f.verified_here)},
             {"reads", f.reads},
             {"kind", f.kind},
             {"ng", f.ng},
             {"spoiler", f.spoiler}};
    if (!f.block.empty()) row["block"] = f.block + " + " + std::to_string(f.offset);
    flag_rows.push_back(std::move(row));
  }
  json tracks = json::array();
  for (const proto::WorkshopTrack& t : w.tracks) {
    tracks.push_back(json{{"id", t.id}, {"step", t.step.empty() ? std::string("none") : t.step}});
  }
  // The flags within the frame, after the rest (stages and tracks are few: the content's own).
  const std::size_t rest = stages.dump().size() + tracks.dump().size() + 4096;
  std::size_t cut = w.flags_cut;
  json flags = fitted(flag_rows, proto::kWorkshopBodyMax > rest ? proto::kWorkshopBodyMax - rest : 0, cut);
  json frame = json::array();
  frame.push_back(item("Flag frame", w.have_frame ? "Read" : "Not read yet in this playthrough"));
  frame.push_back(item("Journey", std::to_string(w.journey)));
  frame.push_back(item("Watch list", "version " + std::to_string(w.watch_version) + ", " + grouped(w.watched) +
                                         (w.watched == 1 ? " id" : " ids")));
  frame.push_back(item("Region", w.region.empty() ? std::string("Not known") : w.region));
  if (cut > 0) frame.push_back(item("Flags left out", grouped(cut)));
  return json{
      {"frame", {{"items", std::move(frame)}}},
      {"stages",
       {{"columns", json::array({column("id", "Stage"), column("value", "Holds"), column("when", "When"),
                                 column("boss", "Boss"), column("spoiler", "Spoiler", true),
                                 column("rank", "Rank", true)})},
        {"rows", std::move(stages)}}},
      {"flags",
       {{"columns",
         json::array({column("alias", "Alias"), column("id", "Id", true), column("block", "Block"),
                      column("watched", "Watched"), column("bit", "Bit"), column("valid", "Exists"),
                      column("verified_file", "Verified in the file"), column("verified_here", "Verified here"),
                      column("reads", "Reads"), column("kind", "Kind"), column("ng", "NG+"),
                      column("spoiler", "Spoiler", true)})},
        {"rows", std::move(flags)}}},
      {"tracks", {{"columns", json::array({column("id", "Track"), column("step", "Active step")})},
                  {"rows", std::move(tracks)}}},
  };
}

json known_body(const proto::WorkshopKnownSets& k) {
  std::vector<json> all;
  for (const proto::WorkshopKnown& r : k.rows) {
    json row{{"persona", r.persona}, {"stage", r.stage},       {"item", r.item},
             {"certainty", r.certainty}, {"path", r.path}, {"own", yes(r.own_affairs)}};
    if (!r.block5b.empty()) row["block5b"] = r.block5b;
    all.push_back(std::move(row));
  }
  std::size_t cut = k.cut;
  json rows = fitted(all, proto::kWorkshopBodyMax - 4096, cut);
  json body{{"columns", json::array({column("persona", "Character"), column("stage", "Stage"), column("item", "Item"),
                                     column("certainty", "Certainty"), column("path", "Path"),
                                     column("own", "Own affairs"), column("block5b", "Block 5b")})},
            {"rows", std::move(rows)}};
  if (cut > 0) body["left_out"] = cut;
  return body;
}

json unknown_body(const std::vector<proto::WorkshopUnknown>& rows) {
  json table = json::array();
  json parts = json::array();
  std::string all;  // every row's TOML, a blank line between: the preview's share, GraceUI's Copy (D263)
  for (const proto::WorkshopUnknown& u : rows) {
    table.push_back(json{{"param", u.param},
                         {"entity", u.entity},
                         {"model", u.model},
                         {"kind", u.kind},
                         {"team", u.team},
                         {"seen", u.seen},
                         {"first", clock_of(u.first_ms)},
                         {"last", clock_of(u.last_ms)},
                         {"nearest", metres(u.nearest_m)}});
    std::string label = "NpcParam " + std::to_string(u.param);
    if (u.entity != 0) label += ", entity " + std::to_string(u.entity);
    parts.push_back(part("u" + std::to_string(u.param) + "_" + std::to_string(u.entity), std::move(label),
                         json::array({field("TOML", u.toml)})));
    all += (all.empty() ? "" : "\n") + u.toml;
  }
  if (table.empty()) return json::object();
  json columns = json::array({column("param", "NpcParam row", true), column("entity", "Entity", true),
                               column("model", "Model", true), column("kind", "Type", true),
                               column("team", "Team", true), column("seen", "Snapshots", true),
                               column("first", "First seen"), column("last", "Last seen"),
                               column("nearest", "Nearest")});
  json toml{{"parts", std::move(parts)}};
  // Copy alone (D263): the text with LF line ends (the rows are ours, written so), within 256 KiB, and a file's name,
  // which D263's share holds with its text (a browser's Save is not offered: copy alone).
  all.erase(std::remove(all.begin(), all.end(), '\r'), all.end());
  if (!all.empty() && all.size() <= kShareMax) toml["share"] = json{{"text", std::move(all)}, {"file", kShareFile}};
  return json{{"rows", {{"columns", std::move(columns)}, {"rows", std::move(table)}}}, {"toml", std::move(toml)}};
}

json status_body(const proto::WorkshopView& v) {
  const proto::WorkshopRecording& r = v.recording;
  json rec = json::array();
  rec.push_back(item("Recording", proto::words(r.why)));
  if (r.file > 0) rec.push_back(item("File this session", std::to_string(r.file)));
  rec.push_back(item("Lines", grouped(r.lines)));
  if (r.refused > 0) rec.push_back(item("Lines refused", grouped(r.refused)));
  if (r.on) rec.push_back(item("This file's size", bytes_of(r.bytes)));
  if (r.rotate_bytes > 0) rec.push_back(item("A new file after", bytes_of(r.rotate_bytes)));
  rec.push_back(item("Where", "The recordings folder, in EldenNet's data folder"));
  json ev = json::array();
  ev.push_back(item("Events since Developer views came on", grouped(v.events.seen)));
  ev.push_back(item("Kept here", "The newest " + std::to_string(proto::kWorkshopEventRing)));
  ev.push_back(item("Dropped this session", grouped(v.events.dropped_t) + " transient, " +
                                                grouped(v.events.dropped_k) + " keyed"));
  return json{{"recording_on", r.on},
              {"recording", {{"items", std::move(rec)}}},
              {"events", {{"items", std::move(ev)}}}};
}

json kinds_body(const proto::WorkshopKinds& k) {
  json kinds = json::array();
  for (const proto::WorkshopKind& x : k.kinds) {
    json row{{"name", x.name}, {"rules", x.rules}};
    if (!x.built_by.empty()) row["built"] = x.built_by;
    if (!x.waits_for.empty()) row["waits"] = x.waits_for;
    if (!x.placeholders.empty()) row["placeholders"] = x.placeholders;
    if (!x.fields.empty()) row["fields"] = x.fields;
    kinds.push_back(std::move(row));
  }
  json rules = json::array();
  for (const proto::WorkshopRule& r : k.rules) {
    json row{{"id", r.id},     {"kind", r.kind}, {"class", r.cls},   {"signal", r.signal}, {"detector", r.detector},
             {"radius", r.radius}, {"ttl", r.ttl_s}, {"cooldown", r.cooldown_s}, {"where", r.where}};
    if (!r.key.empty()) row["key"] = r.key;
    rules.push_back(std::move(row));
  }
  return json{
      {"kinds", {{"columns", json::array({column("name", "Kind"), column("built", "Built by"),
                                          column("waits", "Waits for"), column("placeholders", "Placeholders"),
                                          column("fields", "Payload"), column("rules", "Rules", true)})},
                 {"rows", std::move(kinds)}}},
      {"rules", {{"columns", json::array({column("id", "Rule"), column("kind", "Kind"), column("class", "Class"),
                                          column("signal", "Signal"), column("detector", "Detector"),
                                          column("radius", "Radius"), column("key", "Key"),
                                          column("ttl", "Kept (s)", true), column("cooldown", "Cooldown (s)", true),
                                          column("where", "File")})},
                 {"rows", std::move(rules)}}},
  };
}

}  // namespace

bool Workshop::owns_topic(std::string_view topic) noexcept {
  for (const std::string_view t : kTopics) {
    if (t == topic) return true;
  }
  return false;
}

nlohmann::json Workshop::body(std::string_view topic, const proto::WorkshopView* view) {
  if (view == nullptr) return json::object();
  if (topic == kWorkshopExchange) return exchange_body(view->exchange);
  if (topic == kWorkshopWorld) return world_body(view->world);
  if (topic == kWorkshopKnown) return known_body(view->known);
  if (topic == kWorkshopUnknown) return unknown_body(view->unknown);
  if (topic == kWorkshopStatus) return status_body(*view);
  if (topic == kWorkshopKinds) return kinds_body(view->kinds);
  return json::object();
}

void Workshop::push(const proto::WorkshopEvent& e) {
  rows_.push_back(Row{next_seq_++, e});
  while (rows_.size() > proto::kWorkshopEventRing) rows_.pop_front();
}

json Workshop::row_of(const Row& r) {
  return json{{"seq", r.seq},          {"time", clock_of(r.event.t_ms)}, {"line", r.event.line},
              {"kind", r.event.kind}, {"class", r.event.cls},          {"source", r.event.source}};
}

json Workshop::rows() const {
  json out = json::array();
  for (const Row& r : rows_) out.push_back(row_of(r));
  return out;
}

void Workshop::collect_events(std::vector<std::string>& frames) {
  if (rows_.empty() || rows_.back().seq <= sent_) return;
  std::size_t at = 0;
  while (at < rows_.size() && rows_[at].seq <= sent_) ++at;
  const std::uint64_t expected = sent_ + 1;
  bool first = true;
  while (at < rows_.size()) {
    json rows = json::array();
    const std::uint64_t from = rows_[at].seq;
    for (std::size_t n = 0; n < 100 && at < rows_.size(); ++n, ++at) rows.push_back(row_of(rows_[at]));
    const std::uint64_t dropped = first && from > expected ? from - expected : 0;
    first = false;
    frames.push_back(json{{"v", 1}, {"t", "rows"}, {"space", kSpaceId}, {"topic", kWorkshopEvents},
                          {"from_seq", from}, {"rows", std::move(rows)}, {"dropped", dropped}}
                         .dump(-1, ' ', false, json::error_handler_t::replace));
    sent_ = rows_[at - 1].seq;
  }
}

std::string Workshop::clear_events() {
  rows_.clear();
  // A batch past the next row the view expects starts the stream over there, so an empty one clears it (protocol 1.1).
  next_seq_ += 1;
  const std::uint64_t from = next_seq_;
  next_seq_ += 1;
  sent_ = from;
  return json{{"v", 1}, {"t", "rows"}, {"space", kSpaceId}, {"topic", kWorkshopEvents}, {"from_seq", from},
              {"rows", json::array()}, {"dropped", 0}}
      .dump(-1, ' ', false, json::error_handler_t::replace);
}

}  // namespace en::overlay
