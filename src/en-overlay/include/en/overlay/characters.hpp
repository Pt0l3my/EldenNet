#pragma once
// EldenNet's Characters pages, Named NPCs and its profile editor (PAGES §4.6.4; NPC §4.7; S7.2.b; GraceUI's D215 to
// D218): the roster as the page's master list, each profile's general profile and the player's own layer as an editor
// document, and the editor's five commands, validated here at the space's one entry point and handed to the DLL, which
// checks the text with en-core, writes the player's file and answers. Portable, on nlohmann/json: no Win32, no call to
// GraceUI, no en-core (ARCH §2.3). The space (space.hpp) registers them: its topics, its commands, its answers.
//
// - **Topics.** `named.list`, the master list: a table of every profile EldenNet voices, a row each, by its key
//   (WHY§387: `persona.<id>`, a slot's colon a dot), its name as the game shows it or its first-meeting name (SP3) and
//   its role. `named/<key>`, a family: the row's general profile, what a first meeting shows and nothing else (SP1:
//   `public.summary`, `public.role` and `subjects`; our labels for its way of speaking and its news temperament).
//   `profiles/<key>`, a family: the editor's document over the player's own layer (GraceUI D217): the four fields a
//   layer may set (NPC §4.5), each with the shipped value beside it as its reference, null where the layer leaves it to
//   the shipped profile; a state line; the draft's help or why it cannot run now. Never a private field, a stage, a
//   road, a gist, knowledge, a tie, a stir, a seed or a forbid list (SP1; PAGES §3).
// - **Commands** (GraceUI D217, D216): `profile.check` and `profile.save` {target, rev, doc}, `profile.reset` {target,
//   all, confirm}, `profile.restore` {target} and `profile.draft` {target, notes}, the last a slow command. Each is
//   checked here (a row's key, the revision a number, the doc's known fields of their kinds, the notes within 400
//   characters, a reset confirmed) and, if it passes, handed to the DLL as a ProfileRequest; its answer comes back by
//   `answer`, when the DLL has it (a save once the file is written, a draft when the model answers), and goes to GraceUI
//   with the request's reply handle.
// - **Words.** Every word a player reads here is a draft for the owner's review (the slice's notes list them).
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

namespace en::overlay {

inline constexpr std::string_view kNamedList = "named.list";  // the master list
inline constexpr std::string_view kNamedFamily = "named";     // its general profiles: named/<key>
inline constexpr std::string_view kProfilesFamily = "profiles";  // the editor's documents: profiles/<key>

// A message about a field, an item or the whole file (GraceUI's {field, item, level, words}).
struct ProfileMessage {
  std::string field;  // empty: the whole file
  int item = -1;
  bool error = false;
  std::string words;

  bool operator==(const ProfileMessage&) const = default;
};

// One profile EldenNet voices, as the DLL knows it from the loaded content (en-core's), in en-overlay's own types.
struct ProfileRow {
  std::string key;      // "persona.kale"
  std::string name;     // the first-meeting name (the record's fallback; the runtime name once text is read, SP3)
  // What a first meeting shows (SP1), reviewed when written (PL11):
  std::string role;     // public.role
  std::string summary;  // public.summary
  std::vector<std::string> subjects;
  std::string register_id = "formal";  // the shipped way of speaking (NPC §3.3's register)
  int reply_words_max = 40;            // the shipped longest reply
  std::string news;                    // awareness.news: close, plain or eager; empty when unset
  // The player's own layer (NPC §4.7):
  bool layer = false;          // a layer file of the player's is loaded and merged
  std::uint32_t rev = 0;       // its revision; 0 with none
  nlohmann::json fields = nlohmann::json::object();  // what it sets, by field id (instructions, subjects, register,
                                                     // reply_words_max); for a refused file, what of it could be read
  bool refused = false;        // the player's file holds an error, so it is not loaded
  bool newer = false;          // written by a newer EldenNet: read-only here
  bool updated = false;        // the shipped profile changed since the layer was saved (its base_hash)
  std::uint32_t warnings = 0;  // the load's warnings about it

  bool operator==(const ProfileRow&) const = default;
};

struct CharacterFacts {
  std::uint64_t revision = 0;    // changes whenever a row does (a load, a save)
  std::vector<ProfileRow> rows;  // in the order the list shows them: by name
  // The draft helper (NPC §4.7): the provider its requests go to, and why one cannot run now (empty: it can).
  std::string draft_provider;
  std::string draft_blocked;
};

// A command the space took and the DLL makes. Its reply handle and id answer it, once (GraceUI's invariant 3).
struct ProfileRequest {
  enum class Kind : std::uint8_t { Check, Save, Reset, Restore, Draft };
  Kind kind = Kind::Check;
  std::uint64_t reply = 0;  // GraceUI's handle for `answer`
  std::uint64_t id = 0;     // the command's id, which the result names
  std::string key;          // the row's key: "persona.kale"
  std::uint32_t rev = 0;    // check, save: the revision the player's first unsaved edit was made on
  // check, save: the four fields, each null (left to the shipped profile) or of its kind: a string, a list of strings
  // (a list's items as their text), a register's id, a whole number.
  nlohmann::json doc = nlohmann::json::object();
  std::string notes;        // draft: the player's rough notes, at most 400 characters

  bool operator==(const ProfileRequest&) const = default;
};

// What the DLL answers a request with: ok, or an error code with its words; a check's or a refused save's messages; a
// save's chosen row (GraceUI D218); a draft's proposals and notice.
struct ProfileAnswer {
  std::string error;   // "" for ok; else GraceUI's code: invalid_value, conflict, busy, not_now, internal
  std::string detail;  // the words under the control, with an error
  std::vector<ProfileMessage> messages;
  std::string chosen;  // an ok save: the row the list chooses next
  nlohmann::json draft = nlohmann::json::object();  // a draft: {field: value}
  std::string notice;  // a draft: what was left out, in words
};

class Characters {
 public:
  // The topics' bodies (null facts: the list is empty, and a member has no body).
  [[nodiscard]] static nlohmann::json list_body(const CharacterFacts* facts);
  [[nodiscard]] static nlohmann::json general_body(const CharacterFacts* facts, std::string_view key);
  [[nodiscard]] static nlohmann::json document_body(const CharacterFacts* facts, std::string_view key);
  // Whether a topic is one of these pages' (the list, or a member of either family), and the member's key.
  [[nodiscard]] static bool owns_topic(std::string_view topic) noexcept;
  [[nodiscard]] static bool owns_command(std::string_view name) noexcept;

  // A view's command, checked at the entry point: the result frame to answer at once (a refusal), or empty when the
  // request went to the DLL, which answers it later through `answer`.
  [[nodiscard]] std::string command(std::string_view name, std::uint64_t id, const nlohmann::json& args,
                                    std::uint64_t reply, const CharacterFacts* facts, std::string& result_word);

  [[nodiscard]] std::vector<ProfileRequest> take_requests();
  // The DLL's answer to a request, queued as a result frame for its reply handle.
  void answer(const ProfileRequest& request, const ProfileAnswer& a);
  // The answers queued, each with its reply handle, for the Overlay to hand to GraceUI.
  [[nodiscard]] std::vector<std::pair<std::uint64_t, std::string>> take_answers();
  [[nodiscard]] bool answers_waiting() const noexcept { return !answers_.empty(); }

 private:
  std::vector<ProfileRequest> requests_;
  std::vector<std::pair<std::uint64_t, std::string>> answers_;
};

// A result frame of EldenNet's space: ok with a body, or an error with its words and body.
[[nodiscard]] std::string result_frame(std::uint64_t id, std::string_view error, std::string_view detail,
                                       const nlohmann::json& body);

}  // namespace en::overlay
