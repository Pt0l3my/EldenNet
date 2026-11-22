#pragma once
// The dialogue prompt, part A's (NPC §7.1 to §7.3; DESIGN §8.3): one exchange's messages, rendered by en-prompt from
// the content pack's templates (prompts/dialogue/, the registers, the world frame) and what the conversation hands
// over, with the cache marks on their parts (cache A after block 1, one after block 5, B after block 5b). A render
// resolves every name or fails, and a failed render sends nothing: the conversation takes the path of a failed request
// (NPC §7.1, the "…" page, §6.2). Records enter as quoted data, never as template source.
//
// Part A implements PromptRenderer in src/prompt/, on en-prompt's MemorySource over the templates the content loader
// read (en-core does no I/O; ARCH §2.3); stubs.hpp has a trivial stub. A context field may be added with a default that
// changes nothing; anything else is agreed between A and B (src/en-core/README.md).
#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "en/core/speech.hpp"  // HardList, for the reminder
#include "en/proto/expected.hpp"
#include "en/proto/llm.hpp"

namespace en::core {

// Every dialogue request's stop sequences (NPC §7.3): the model starting the Tarnished's turn. The sanitizer's step 3
// cuts the same run-on where a provider takes none (a stripped retry, en-llm's request.hpp).
inline constexpr std::array<std::string_view, 2> kDialogueStops = {"\nThe Tarnished:", "\nTarnished:"};

// A line of a conversation, as it was committed: the Tarnished's as sent (sanitized and capped), the persona's as shown.
struct Turn {
  enum class Who : std::uint8_t { Tarnished, Persona };
  Who who = Who::Tarnished;
  std::string text;

  friend bool operator==(const Turn&, const Turn&) = default;
};

// Block 9, here and now: words the conversation worked out from the newest snapshot (NPC §7.2's `observed` and `brief`
// modes, §7.3). An empty field leaves its clause out whole (§7.1).
struct Scene {
  std::string place;                  // the place's name as the game shows it
  std::string time_word;              // "at dusk"
  std::string weather_word;           // verified weather only
  std::vector<std::string> nearby;    // at most four others, a brief line each
  std::string tarnished;              // the Tarnished as observed: body pronoun, worn gear, condition band
  std::vector<std::string> just_now;  // at most three lines this persona saw or heard
  std::string you_now;                // the "you, now" line, when the persona is hurt, fighting or afflicted (O35)

  friend bool operator==(const Scene&, const Scene&) = default;
};

// What one exchange's prompt is rendered from. The pointers and spans are the caller's, read during render() only.
struct DialogueContext {
  std::string_view persona;                // the profile's id, for errors and the cache key: never its name
  // Its record as the content pack holds it (NPC §3.1), with its stage resolved (§3.2): blocks 2 to 5b and block 7's
  // stage fields. Null refuses (NoRecord).
  const nlohmann::json* record = nullptr;
  std::string_view name;                   // its name as the game shows it, for block 11
  std::span<const Turn> earlier;           // block 6 (a): the previous conversation's last six shown lines at most
  std::span<const Turn> history;           // block 6 (b): this conversation's completed exchanges, oldest first
  Scene scene;                             // block 9
  std::span<const std::string> recap;      // block 10: the newest three player lines at most since the persona last
                                           // spoke, oldest first; the last is the line just sent
  std::optional<HardList> reminder;        // block 11's note after a hard lint hit (§8.4), never the word that hit
  // Blocks 7 and 8's other sources (the KB, the journal, keepsakes, facts and disposition, the told name, the
  // since-line, the lived layer) come with the store and memory (v0.3) and join this context then.
};

struct RenderedPrompt {
  std::vector<proto::Message> messages;  // in order, consecutive roles merged, the cache marks on their parts
  std::uint32_t tokens = 0;              // the estimate, ceil(bytes / 3.6 × 1.15) (§7.1), after any trim

  friend bool operator==(const RenderedPrompt&, const RenderedPrompt&) = default;
};

enum class PromptRefusal : std::uint8_t {
  NoRecord,  // the context holds no record
  Template,  // a template failed: an unknown name, a null printed, a condition that is no boolean, a missing part
  OverCap,   // past the hard cap of 3,500 tokens after every trim (§7.3)
};

struct PromptError {
  PromptRefusal why = PromptRefusal::Template;
  std::string detail;  // the template's name, line and column and the problem: never a field's value or a record's text

  friend bool operator==(const PromptError&, const PromptError&) = default;
};

class PromptRenderer {
 public:
  virtual ~PromptRenderer() = default;
  [[nodiscard]] virtual proto::Expected<RenderedPrompt, PromptError> render(const DialogueContext& context) const = 0;
};

// The chat request for a rendered dialogue prompt: the dialogue slot, a reply, the two stop sequences; max_tokens (140)
// and the temperature (0.8) are left to the slot's budget (en-llm's config.hpp).
[[nodiscard]] inline proto::ChatRequest dialogue_request(RenderedPrompt prompt) {
  proto::ChatRequest request;
  request.slot = std::string(proto::kDialogue);
  request.purpose = proto::Purpose::Reply;
  request.messages = std::move(prompt.messages);
  for (const std::string_view stop : kDialogueStops) request.stop.emplace_back(stop);
  return request;
}

}  // namespace en::core
