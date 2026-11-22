#pragma once
// The dialogue prompt's renderer, part A's (NPC §7.1 to §7.3; DESIGN §8.3): prompt.hpp's PromptRenderer, for real, on
// en-prompt over the content pack's templates held in memory (MemorySource: en-core does no I/O, ARCH §2.3).
//
// The prompt is the folder prompts/dialogue/, one numbered part per block, rendered block by block in order: 01 the
// world frame (system, cache A after it), 02 identity, 03 the private self, 04 the voice (with prompts/registers/<r>),
// 05 the contract (a breakpoint after it), 05b as things stand now (cache B after it), 06 earlier and now (history as
// user and assistant messages), 07 what you know now, 08 the Tarnished as you know them, 09 here and now, 10 the recap
// and 11 the task (user). Consecutive messages of one role are merged with a blank line, keeping the breakpoints.
//
// The template data is built here from the context, with every name always present (null, "" or [] when unset), so a
// template's names are fixed and checked when the pack loads (src/en-core/src/prompt/README.md lists them). The
// record's fields are read with their types checked: a field of the wrong type refuses the render, naming the field,
// never its value.
// The Tarnished's words enter quoted; no input carries the player's name, which appears in no prompt until told
// (DESIGN §8.8; the told-name line of block 8 comes with memory, v0.3).
//
// Budgets (§7.3): each block's estimate against its cap; blocks 6 and 9 are trimmed to their caps (900 and 210), and
// then, while the whole is over the 2,800 target: block 6's oldest first (the previous conversation's lines, then this
// conversation's oldest exchanges, always keeping the last two), then block 9's nearby others (the last listed first).
// Keepsakes (block 8) and topic entries (block 7) join the order when they exist. Past the hard cap of 3,500 after
// every trim the render is refused (OverCap). The other blocks are never trimmed; one over its cap is reported, not
// refused. Block 2 is checked whole when the pack loads instead (WHY§388; speech_content.hpp): past its 120 an error for
// shipped and test content, a warning for the player's own files. Block 11 carries one tic note at most, the newest.
// The same context renders byte for byte the same (T5).
#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

#include "en/core/prompt.hpp"

namespace en::core {

enum class Block : std::uint8_t {
  Frame,      // 1
  Identity,   // 2
  Private,    // 3
  Voice,      // 4
  Contract,   // 5
  Standing,   // 5b
  History,    // 6
  Knowledge,  // 7
  Tarnished,  // 8
  Scene,      // 9
  Recap,      // 10
  Task,       // 11
};
inline constexpr std::size_t kBlockCount = 12;

struct BlockSpec {
  Block block;
  std::string_view number;  // the part's prefix in prompts/dialogue/, and the block's name in the design: "05b"
  std::uint16_t cap;        // tokens (§7.3)
};
inline constexpr std::array<BlockSpec, kBlockCount> kBlocks = {{
    {Block::Frame, "01", 450},
    {Block::Identity, "02", 120},
    {Block::Private, "03", 350},
    {Block::Voice, "04", 230},
    {Block::Contract, "05", 110},
    {Block::Standing, "05b", 200},
    {Block::History, "06", 900},
    {Block::Knowledge, "07", 280},
    {Block::Tarnished, "08", 450},
    {Block::Scene, "09", 210},
    {Block::Recap, "10", 120},
    {Block::Task, "11", 70},  // 40 in NPC §7.3 until WHY§388: the ask, one tic note and a reminder
}};
inline constexpr std::uint32_t kPromptTarget = 2800;
inline constexpr std::uint32_t kPromptHardCap = 3500;
// The caps' sum stays within the hard cap (NPC §4.5; 3,490 since WHY§388).
inline constexpr std::uint32_t kBlockCapsSum = [] {
  std::uint32_t sum = 0;
  for (const BlockSpec& b : kBlocks) sum += b.cap;
  return sum;
}();
static_assert(kBlockCapsSum == 3490 && kBlockCapsSum <= kPromptHardCap);
inline constexpr std::size_t kKeptExchanges = 2;
inline constexpr std::array<std::string_view, 5> kRegisters = {"archaic-regal", "formal", "colloquial", "broken",
                                                               "chant"};

struct BlockReport {
  Block block = Block::Frame;
  std::uint32_t tokens = 0;  // its own messages' estimate, after any trim
  bool over_cap = false;
};

// What a render did, for the tools, the eval and the debug view.
struct PromptReport {
  RenderedPrompt prompt;
  std::array<BlockReport, kBlockCount> blocks{};
  std::uint32_t earlier_trimmed = 0;    // lines of 6 (a) left out
  std::uint32_t exchanges_trimmed = 0;  // exchanges of 6 (b) left out
  std::uint32_t nearby_trimmed = 0;     // nearby others of block 9 left out
};

class DialoguePrompt final : public PromptRenderer {
 public:
  // The pack's templates by name from the pack's root ("prompts/dialogue/01_world_frame.md",
  // "prompts/registers/formal.md", ...); names outside prompts/ are ignored. Every block's part must be there and no
  // other numbered part, each register's file, and the dialogue folder must lint clean against the data's names.
  [[nodiscard]] static proto::Expected<std::unique_ptr<DialoguePrompt>, PromptError> load(
      const std::map<std::string, std::string>& templates);
  ~DialoguePrompt() override;
  DialoguePrompt(const DialoguePrompt&) = delete;
  DialoguePrompt& operator=(const DialoguePrompt&) = delete;

  [[nodiscard]] proto::Expected<RenderedPrompt, PromptError> render(const DialogueContext& context) const override;
  [[nodiscard]] proto::Expected<PromptReport, PromptError> report(const DialogueContext& context) const;
  // One block's estimate, rendered alone: the content's load checks each persona's block 2 against its cap with it
  // (speech_content.hpp; WHY§388).
  [[nodiscard]] proto::Expected<std::uint32_t, PromptError> block_tokens(const DialogueContext& context,
                                                                         Block block) const;

  struct Impl;

 private:
  explicit DialoguePrompt(std::unique_ptr<Impl> impl);
  std::unique_ptr<Impl> impl_;
};

// The template data a context gives, before any trim (src/en-core/src/prompt/README.md lists its names).
[[nodiscard]] proto::Expected<nlohmann::json, PromptError> dialogue_data(const DialogueContext& context);

}  // namespace en::core
