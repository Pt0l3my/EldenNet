// EldenNet's Characters pages and its profile editor (characters.hpp; PAGES §4.6.4; NPC §4.7; S7.2.b): the bodies its
// topics publish, projections of what a first meeting shows and of the player's own layer, and its commands checked at
// the space's one entry point.
#include "en/overlay/characters.hpp"

#include <algorithm>
#include <array>

namespace en::overlay {

namespace {

using nlohmann::json;

constexpr std::size_t kNotesMax = 400;  // NPC §4.7: the draft's notes, in characters
constexpr std::array<std::string_view, 4> kFields = {"instructions", "subjects", "register", "reply_words_max"};

// ---- The words players read (drafts for the owner's review, British spelling).

struct RegisterWords {
  std::string_view id;
  std::string_view label;
};
// Our labels for NPC §3.3's registers, as behaviour.
constexpr std::array<RegisterWords, 5> kRegisters = {{
    {"archaic-regal", "Old and stately"},
    {"formal", "Formal and measured"},
    {"colloquial", "Everyday, with old idiom"},
    {"broken", "Broken, in fragments"},
    {"chant", "Chanting, in refrains"},
}};

std::string_view register_label(std::string_view id) {
  for (const RegisterWords& r : kRegisters) {
    if (r.id == id) return r.label;
  }
  return {};
}

// awareness.news (O30): which news the character raises unasked, in words.
std::string_view news_words(std::string_view news) {
  if (news == "close") return "Speaks of news only where it touches them";
  if (news == "plain") return "Speaks of news that touches them or you";
  if (news == "eager") return "Passes on news and local talk readily";
  return {};
}

std::size_t chars_of(std::string_view s) {
  return static_cast<std::size_t>(
      std::count_if(s.begin(), s.end(), [](char c) { return (static_cast<unsigned char>(c) & 0xC0u) != 0x80u; }));
}

const ProfileRow* row_of(const CharacterFacts* facts, std::string_view key) {
  if (facts == nullptr) return nullptr;
  for (const ProfileRow& r : facts->rows) {
    if (r.key == key) return &r;
  }
  return nullptr;
}

std::string_view member_key(std::string_view topic, std::string_view family) {
  if (topic.size() <= family.size() + 1 || topic.substr(0, family.size()) != family || topic[family.size()] != '/') {
    return {};
  }
  return topic.substr(family.size() + 1);
}

json state_of(const ProfileRow& r) {
  if (r.newer) {
    return {{"word", "Newer file"},
            {"tone", "rot"},
            {"line", "Your file was written by a newer EldenNet, so it is shown here read-only."}};
  }
  if (r.refused) {
    return {{"word", "Not loaded"},
            {"tone", "rot"},
            {"line", "Your file holds a problem, so EldenNet uses its own profile alone. Check shows what to fix."}};
  }
  if (!r.layer) {
    return {{"word", "Not written yet"},
            {"tone", "glint"},
            {"line", "Your notes go in a file of your own; EldenNet's own profile is never changed."}};
  }
  if (r.updated) {
    return {{"word", "Updated since you wrote this"},
            {"tone", "ember"},
            {"line", "EldenNet's own profile changed in an update: compare yours with the shipped text beside it."}};
  }
  if (r.warnings > 0) {
    return {{"word", "Saved, with warnings"},
            {"tone", "ember"},
            {"line", "Your notes are used from their next line. Check shows the warnings."}};
  }
  return {{"word", "Saved"}, {"tone", "moss"}, {"line", "Your notes are used from their next line."}};
}

// A field's value as the layer sets it, or null: left to the shipped profile.
json value_of(const ProfileRow& r, std::string_view id) {
  const auto it = r.fields.find(std::string(id));
  return it == r.fields.end() ? json(nullptr) : *it;
}

}  // namespace

std::string result_frame(std::uint64_t id, std::string_view error, std::string_view detail, const json& body) {
  json r{{"v", 1}, {"t", "result"}, {"space", "eldennet"}, {"id", id}};
  if (error.empty()) {
    r["ok"] = true;
  } else {
    r["error"] = error;
    if (!detail.empty()) r["detail"] = detail;
  }
  if (body.is_object() && !body.empty()) r["body"] = body;
  return r.dump(-1, ' ', false, json::error_handler_t::replace);
}

bool Characters::owns_topic(std::string_view topic) noexcept {
  return topic == kNamedList || !member_key(topic, kNamedFamily).empty() || !member_key(topic, kProfilesFamily).empty();
}

bool Characters::owns_command(std::string_view name) noexcept {
  return name == "profile.check" || name == "profile.save" || name == "profile.reset" || name == "profile.restore" ||
         name == "profile.draft";
}

json Characters::list_body(const CharacterFacts* facts) {
  json rows = json::array();
  if (facts != nullptr) {
    for (const ProfileRow& r : facts->rows) rows.push_back(json{{"id", r.key}, {"name", r.name}, {"role", r.role}});
  }
  return json{{"columns", json::array({json{{"id", "name"}, {"label", "Name"}}, json{{"id", "role"}, {"label", "What they do"}}})},
              {"rows", std::move(rows)}};
}

json Characters::general_body(const CharacterFacts* facts, std::string_view key) {
  const ProfileRow* r = row_of(facts, key);
  if (r == nullptr) return json::object();
  // What a first meeting shows (SP1), and our labels; a field the profile lacks is left out (absence is silent).
  json items = json::array();
  const auto item = [&](std::string_view label, const std::string& value) {
    if (!value.empty()) items.push_back(json{{"label", label}, {"value", value}});
  };
  item("Who they appear to be", r->summary);
  item("What they do", r->role);
  std::string subjects;
  for (const std::string& s : r->subjects) subjects += (subjects.empty() ? "" : "; ") + s;
  item("What they like to talk about", subjects);
  item("How they speak", std::string(register_label(r->register_id)));
  item("News", std::string(news_words(r->news)));
  return json{{"items", std::move(items)}};
}

json Characters::document_body(const CharacterFacts* facts, std::string_view key) {
  const ProfileRow* r = row_of(facts, key);
  if (r == nullptr) return json::object();
  json choices = json::array();
  for (const RegisterWords& w : kRegisters) choices.push_back(json{{"value", w.id}, {"label", w.label}});
  json fields = json::array({
      json{{"id", "instructions"},
           {"label", "Your notes on how they speak"},
           {"kind", "prose"},
           {"help", "One paragraph in your own words: how they speak and carry themselves. It joins the end of their "
                    "voice in every conversation."},
           {"value", value_of(*r, "instructions")},
           {"max_words", 35},
           {"max_chars", 210},
           {"nullable", true}},
      json{{"id", "subjects"},
           {"label", "What they like to talk about"},
           {"kind", "list"},
           {"help", "Short phrases, not sentences or questions. Yours take the place of the shipped list."},
           {"value", value_of(*r, "subjects")},
           {"reference", r->subjects},
           {"reference_label", "As shipped"},
           {"max_items", 8},
           {"max_words", 10},
           {"max_chars", 50},
           {"nullable", true}},
      json{{"id", "register"},
           {"label", "How they speak"},
           {"kind", "choice"},
           {"help", "The manner of their speech. Left unset, they keep their own."},
           {"value", value_of(*r, "register")},
           {"choices", std::move(choices)},
           {"reference", std::string(register_label(r->register_id))},
           {"reference_label", "As shipped"},
           {"nullable", true}},
      json{{"id", "reply_words_max"},
           {"label", "Longest reply, in words"},
           {"kind", "number"},
           {"help", "The most words a reply of theirs may have."},
           {"value", value_of(*r, "reply_words_max")},
           {"min", 10},
           {"max", 60},
           {"reference", std::to_string(r->reply_words_max)},
           {"reference_label", "As shipped"},
           {"nullable", true}},
  });
  if (r->newer) {
    for (json& f : fields) f["locked"] = "Written by a newer EldenNet: change it in its own file.";
  }
  json doc{{"target", r->key}, {"rev", r->rev}, {"state", state_of(*r)}, {"fields", std::move(fields)}};
  if (facts != nullptr && !facts->draft_blocked.empty()) {
    doc["draft_blocked"] = facts->draft_blocked;
  } else if (facts != nullptr && !facts->draft_provider.empty()) {
    doc["draft_help"] = "Sends your rough notes, and your notes here when you revise, to " + facts->draft_provider +
                        ". Nothing of the game goes with them.";
  }
  return doc;
}

std::string Characters::command(std::string_view name, std::uint64_t id, const json& args, std::uint64_t reply,
                                const CharacterFacts* facts, std::string& result_word) {
  const auto refuse = [&](std::string_view code, std::string_view detail) {
    result_word = std::string(code);
    return result_frame(id, code, detail, json::object());
  };
  if (!args.is_object()) return refuse("invalid_value", "");
  const auto target = args.find("target");
  if (target == args.end() || !target->is_string()) return refuse("invalid_value", "");
  const std::string key = target->get<std::string>();
  const ProfileRow* row = row_of(facts, key);
  if (row == nullptr) return refuse("invalid_value", "That character has no profile here.");
  if (reply == 0) return refuse("internal", "");  // never: GraceUI gives every command its handle
  ProfileRequest r;
  r.reply = reply;
  r.id = id;
  r.key = key;
  if (name == "profile.check" || name == "profile.save") {
    r.kind = name == "profile.check" ? ProfileRequest::Kind::Check : ProfileRequest::Kind::Save;
    const auto rev = args.find("rev");
    const auto doc = args.find("doc");
    if (rev == args.end() || !rev->is_number_unsigned() || rev->get<std::uint64_t>() > 0xFFFFFFFFull) {
      return refuse("invalid_value", "");
    }
    if (doc == args.end() || !doc->is_object()) return refuse("invalid_value", "");
    r.rev = static_cast<std::uint32_t>(rev->get<std::uint64_t>());
    // The doc's fields: the four a layer may set, each null or of its kind; anything else is refused whole.
    for (const auto& [k, v] : doc->items()) {
      if (std::find(kFields.begin(), kFields.end(), k) == kFields.end()) return refuse("invalid_value", "");
      if (v.is_null()) {
        r.doc[k] = nullptr;
      } else if (k == "instructions" || k == "register") {
        if (!v.is_string()) return refuse("invalid_value", "");
        r.doc[k] = v;
      } else if (k == "reply_words_max") {
        if (!v.is_number()) return refuse("invalid_value", "");
        r.doc[k] = v;
      } else {  // subjects: items of words, or {text, key, ...} with their text
        if (!v.is_array()) return refuse("invalid_value", "");
        json items = json::array();
        for (const json& item : v) {
          if (item.is_string()) {
            items.push_back(item);
          } else if (item.is_object() && item.contains("text") && item["text"].is_string()) {
            items.push_back(item["text"]);
          } else {
            return refuse("invalid_value", "");
          }
        }
        r.doc[k] = std::move(items);
      }
    }
    if (row->newer) return refuse("not_now", "This file was written by a newer EldenNet: change it in its own file.");
  } else if (name == "profile.reset") {
    r.kind = ProfileRequest::Kind::Reset;
    const auto all = args.find("all");
    const auto confirm = args.find("confirm");
    if (all == args.end() || !all->is_boolean() || !all->get<bool>()) return refuse("invalid_value", "");
    if (confirm == args.end() || !confirm->is_boolean() || !confirm->get<bool>()) return refuse("confirm_required", "");
    if (!row->layer && !row->refused && !row->newer) return refuse("not_now", "There are no notes of yours to reset.");
  } else if (name == "profile.restore") {
    r.kind = ProfileRequest::Kind::Restore;
  } else {
    r.kind = ProfileRequest::Kind::Draft;
    const auto notes = args.find("notes");
    if (notes == args.end() || !notes->is_string()) return refuse("invalid_value", "");
    r.notes = notes->get<std::string>();
    if (chars_of(r.notes) > kNotesMax) return refuse("invalid_value", "At most 400 characters of notes.");
    if (r.notes.find_first_not_of(" \t\r\n") == std::string::npos) {
      return refuse("invalid_value", "Write a few rough notes first.");
    }
    if (facts != nullptr && !facts->draft_blocked.empty()) return refuse("not_now", facts->draft_blocked);
  }
  result_word = "taken";
  requests_.push_back(std::move(r));
  return {};
}

std::vector<ProfileRequest> Characters::take_requests() { return std::exchange(requests_, {}); }

void Characters::answer(const ProfileRequest& request, const ProfileAnswer& a) {
  json body = json::object();
  if (!a.messages.empty()) {
    json messages = json::array();
    for (const ProfileMessage& m : a.messages) {
      json one{{"level", m.error ? "error" : "warning"}, {"words", m.words}};
      if (!m.field.empty()) one["field"] = m.field;
      if (m.item >= 0) one["item"] = m.item;
      messages.push_back(std::move(one));
    }
    body["messages"] = std::move(messages);
  }
  if (a.error.empty() && request.kind == ProfileRequest::Kind::Save && !a.chosen.empty()) body["chosen"] = a.chosen;
  if (a.error.empty() && request.kind == ProfileRequest::Kind::Draft) {
    body["draft"] = a.draft.is_object() ? a.draft : json::object();
    if (!a.notice.empty()) body["notice"] = a.notice;
  }
  answers_.emplace_back(request.reply, result_frame(request.id, a.error, a.detail, body));
}

std::vector<std::pair<std::uint64_t, std::string>> Characters::take_answers() { return std::exchange(answers_, {}); }

}  // namespace en::overlay
