#pragma once
// Quest and world awareness (DESIGN §10.6; MEM §6, §7, §12; S11.3.c): what holds in the land, and what each persona
// knows of it. Pure (ARCH §2.3): the content pack's files were read by the shell; this parses, checks and evaluates.
//
// - **The content.** `flags/catalog.toml` (MEM §6.1: our aliases for the game's flags, each a lead until S6 verifies
//   it), `regions.toml` (NPC §9.5's keys and their Sky column), `world/stages.toml` (the nine states of the land, MEM
//   §12.4) and `world/tracks.toml` (tracks owned by no persona). A pack without them knows nothing of the world.
// - **Predicates** (MEM §6.5, §6.6): `flag('alias')`, `boss('x')` (the alias `boss.x`), `stage('t', 's')` (t has reached
//   s: that stage's `when`), `world('x')`, `region('key')`, `runes >= n` (the great runes, through `runes.ge<n>`),
//   `journey >= n`, `true`, `false`, with `!`, `&&` and `||`, in three values (Kleene's). An alias the catalogue lacks,
//   a raw number, `knows()` (the KB's, v0.3), a cycle of `stage()` or `world()`, or a world stage's `when` with `!`,
//   `stage()`, `region()` or `journey` (W1) is a load error, named by file, line and key, never quoting the text.
// - **The flag state** (`FlagState`): each catalogue alias's bit, whether its group exists and whether it is verified.
//   An alias reads Unknown until the playthrough's first frame, when unverified, or when its id does not exist; so
//   Unknown always hides (MEM §6.4). Built from the snapshot's FlagFrame against the core's watch list, or from a
//   fixture state (tests/fixtures/flagstates/, MEM §7.8: alias values, every one verified).
// - **What holds** (`derive`, MEM §12.3): each world stage T, U or F (at spoiler ≥ 2 a T needs every flag atom, through
//   `world()`, verified and valid, else it is U: MEM §7.4), each track's active step, each persona's active stage
//   (NPC §3.2: the last whose `when` holds; an Unknown one is active only if it only restricts).
// - **Who knows** (`known`, MEM §12.5): for one persona at its active stage, every item that holds and that it knows
//   by a path (Own, Kept, Tie, Sky, Near, Word), at the best certainty. Absence is silence: an item that does not hold,
//   or that the persona knows by no path, is not in the set, and nothing says "not yet".
// - **Positive grounding** (`ground`, MEM §12.6; O36, O86): block 5b's lines, chosen and ordered (own affairs, then tied,
//   then rank), at most four, split into the affairs tied to the persona's road and the land, each grouped by
//   certainty. A line is the persona's tie line, else the world stage's line, else the step's `heard`. What does not fit
//   is kept by id for block 7's topic offers, which wait for the KB (v0.3). The prompt names nothing the persona must
//   not say: there is no runtime filter (MEM §7.3).
//
// Not yet: `knows()` and the KB (v0.3), the since-line and the reaction cue (v0.3, WHY§364 (12)), `world_seen` from
// the store (the Kept path takes it as an argument, empty in v0.2), the lints W2 to W15 beyond these checks (MEM
// §12.10), the alias table's renames and grace flags.
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "en/core/content.hpp"
#include "en/core/identity.hpp"  // Realm
#include "en/core/prompt.hpp"    // Standing
#include "en/proto/expected.hpp"
#include "en/proto/snapshot.hpp"

namespace en::core {

enum class Tri : std::uint8_t { F, U, T };
[[nodiscard]] const char* to_string(Tri v) noexcept;  // "F", "U", "T"

inline constexpr std::string_view kCataloguePath = "flags/catalog.toml";
inline constexpr std::string_view kRegionsPath = "regions.toml";
inline constexpr std::string_view kWorldStagesPath = "world/stages.toml";
inline constexpr std::string_view kTracksPath = "world/tracks.toml";

// ---- flags/catalog.toml (MEM §6.1) ----

enum class Ng : std::uint8_t { Reset, Keep, Unknown };

struct CatalogueBlock {
  std::string alias;
  std::uint32_t base = 0;  // 0: unknown
  bool verified = false;
};

struct CatalogueFlag {
  std::string alias;
  std::uint32_t id = 0;     // its own id, or its block's base + offset; 0: unresolved (never watched, always Unknown)
  std::string block;        // a block flag's block, else empty
  std::uint8_t offset = 0;  // in the block, 0 to 19
  bool outcome = true;      // kind: outcome | state
  Ng ng = Ng::Unknown;
  bool verified = false;    // as the file says
  bool ci = false;          // its evidence is a `ci:` record: test content, verified in the eval's realm alone
  std::uint8_t spoiler = 0;
  bool dlc = false;
  bool core = false;
  std::vector<std::string> match;  // boss aliases: the words that offer slew:<boss> (MEM §2.5)
};

struct Catalogue {
  std::vector<CatalogueBlock> blocks;
  std::vector<CatalogueFlag> flags;

  [[nodiscard]] std::optional<std::size_t> find(std::string_view alias) const noexcept;
  [[nodiscard]] const CatalogueBlock* block(std::string_view alias) const noexcept;
  // The alias's verification in a realm: the file's `verified`, and a `ci:` evidence counts only in the eval's.
  [[nodiscard]] bool verified(std::size_t flag, Realm realm) const noexcept;
};

// ---- regions.toml and the world files (MEM §12.2) ----

struct Region {
  std::string key;
  bool sky = false;
};

enum class Reach : std::uint8_t { Everywhere, Region, Linked };
enum class Certainty : std::uint8_t { Fact, Rumour, Belief };  // the strongest first
enum class Path : std::uint8_t { Own, Kept, Tie, Sky, Near, Word };  // a tie between certainties is broken in this order
[[nodiscard]] const char* to_string(Certainty v) noexcept;  // "fact", "rumour", "belief"
[[nodiscard]] const char* to_string(Path v) noexcept;       // "own", "kept", "tie", "sky", "near", "word"

struct WorldStage {
  std::string id;  // the item is "w:<id>"
  std::string when;
  std::string boss;  // links slew:<boss> claims (MEM §12.7); empty: none
  Reach reach = Reach::Linked;
  std::vector<std::string> regions;
  Certainty near_certainty = Certainty::Rumour;  // the file's `near` (windows.h defines `near` as nothing)
  std::uint8_t rank = 0;
  std::uint8_t spoiler = 0;
  std::vector<std::string> supersedes;  // item ids whose line this one makes redundant
  std::string short_text;
  std::string line;
  std::vector<std::string> topics;
  std::size_t source_line = 0;
};

struct TrackStep {
  std::string id;  // the item is "q:<track>.<id>"
  std::string when;
  std::uint8_t spoiler = 0;
  std::string short_text;
  std::string heard;
};

struct Track {
  std::string id;
  std::string block;
  std::vector<TrackStep> steps;
};

// ---- A persona's awareness (MEM §12.2; NPC §3.1), as the record holds it ----

struct Tie {
  std::string item;                     // "w:<id>" or "q:<track>"
  std::optional<Certainty> certainty;   // a certainty adds the Tie path; none: prominence and its own line only
  std::string when;                     // empty: always (Unlock mode, MEM §6.4)
  std::string from;                     // a track tie's earliest step
  std::string line;                     // a world-stage tie's own line
  std::uint8_t spoiler = 0;
  bool played = false;                  // basis = played
  bool evidence = false;                // it cites an S6 record: a played tie without one is inert (W8)
};

// ---- The flag state (MEM §6.6) ----

struct FlagState {
  bool have_frame = false;               // false until the playthrough's first frame: every alias is Unknown
  std::vector<std::uint8_t> bits;        // per catalogue flag, 0 or 1
  std::vector<std::uint8_t> valid;       // its id exists (the descriptor map, MEM §6.2)
  std::vector<std::uint8_t> verified;    // in this realm
  std::uint8_t journey = 1;              // the journey (NG+ cycle), 1 to 8
  std::optional<std::string> region;     // the Tarnished's region key, once the place group maps one (not in v0.2)

  [[nodiscard]] Tri flag(std::size_t i) const noexcept;
  friend bool operator==(const FlagState&, const FlagState&) = default;
};

// The ids the core asks the game task to read (MEM §6.2): resolved and verified aliases only in v0.2, sorted by id, at
// most kMaxWatchFlags. Bit i of a FlagFrame answers ids[i], alias flags[i].
struct WatchList {
  std::uint32_t version = 0;
  std::vector<std::uint32_t> ids;
  std::vector<std::size_t> flags;
};

// ---- What holds, and who knows it ----

struct WorldView {
  bool have_frame = false;
  std::vector<Tri> stages;                         // per world stage, in file order
  std::vector<std::optional<std::size_t>> tracks;  // per track: the active step, if any holds
  std::vector<std::size_t> personas;               // per persona: the active stage

  friend bool operator==(const WorldView&, const WorldView&) = default;
};

struct Known {
  std::string item;  // "w:<id>" or "q:<track>.<step>"
  Certainty certainty = Certainty::Rumour;
  Path path = Path::Word;
  bool own_affairs = false;  // in its stage's `knows`, or under an active tie with a certainty (MEM §12.5)

  friend bool operator==(const Known&, const Known&) = default;
};

// What the persona held at an earlier meeting (the fact `world_seen`, MEM §12.7): the Kept path. From the store (v0.3).
struct Seen {
  std::string item;
  Certainty certainty = Certainty::Rumour;
};

// One persona's grounding at its active stage: its known set, and block 5b's lines (MEM §12.6).
struct Grounding {
  std::size_t stage = 0;
  std::vector<Known> known;
  Standing tied;                  // the affairs tied to its own road
  Standing land;                  // the land's
  std::vector<std::string> shown;    // block 5b's items, in its order
  std::vector<std::string> offered;  // known items left out of block 5b, for block 7's topic offers (v0.3)

  friend bool operator==(const Grounding&, const Grounding&) = default;
};

// The whole view for one flag state: what the core hands the conversation.
struct KnowledgeView {
  WorldView world;
  std::vector<Grounding> personas;  // per persona of the pack, in its order

  friend bool operator==(const KnowledgeView&, const KnowledgeView&) = default;
};

// What Knowledge compiles (knowledge.cpp): a predicate as a program in postfix order, and each persona's stages and ties.
struct KnowledgeOp {
  enum class Code : std::uint8_t { Const, Flag, Runes, Journey, Region, Stage, World, Not, And, Or };
  Code code = Code::Const;
  std::uint32_t a = 0;  // Const: 0 or 1; Flag: the alias; Runes, Journey: n; Region: the region; Stage: its program;
                        // World: the world stage
};

struct KnowledgePersona {
  std::string id;
  struct StageInfo {
    std::string id;
    std::size_t when = 0;  // its program
    bool shows = false;    // NPC §3.2: it unlocks or shows something of its own
    std::string at;        // inherited
    std::string hears;     // inherited: all | sky | none
    std::string road;      // inherited
    std::string heard;     // its line for other personas' ties (never inherits)
    std::vector<std::string> knows;  // inherited, replaced when given
  };
  std::vector<StageInfo> stages;
  std::vector<std::string> home;
  struct TieInfo {
    Tie tie;
    std::optional<std::size_t> when;  // its program; none: always
  };
  std::vector<TieInfo> ties;
};

// A known item with what block 5b needs of it: whether an active tie covers it (with or without a certainty), its rank,
// its line (the tie's own, else the world stage's, else the step's `heard`) and what it supersedes.
struct KnownLine {
  Known known;
  bool tied = false;
  int rank = 0;
  std::string line;
  std::vector<std::string> supersedes;
};

inline constexpr std::size_t kKnownMax = 24;         // MEM §12.5
inline constexpr std::size_t kStandingLinesMax = 4;  // MEM §12.6
inline constexpr std::size_t kStandingTokensMax = 200;

class Knowledge {
 public:
  Knowledge() = default;  // knows nothing: no catalogue, no world, no persona

  // Reads the pack's world files (absent ones are empty) and compiles every predicate: the world stages', the tracks',
  // each persona's stages' and ties'. Checks the catalogue's and the world's schemas, ids unique across world stages,
  // tracks and personas, the region keys, the personas' `at`, `home`, `knows` and tie items, and each row's
  // `flag_block` against the catalogue's block. Any error refuses (at most 64, then a count).
  [[nodiscard]] static proto::Expected<Knowledge, std::vector<ContentError>> build(const ContentPack& pack);

  [[nodiscard]] const Catalogue& catalogue() const noexcept { return catalogue_; }
  [[nodiscard]] std::span<const Region> regions() const noexcept { return regions_; }
  [[nodiscard]] std::span<const WorldStage> stages() const noexcept { return stages_; }
  [[nodiscard]] std::span<const Track> tracks() const noexcept { return tracks_; }
  [[nodiscard]] std::size_t personas() const noexcept { return personas_.size(); }

  // A flag state with no frame: every alias Unknown.
  [[nodiscard]] FlagState unread() const;
  // A fixture state (MEM §7.8; tests/fixtures/flagstates/): `[flags]` aliases set true, `[blocks]` offsets set and
  // cleared in a catalogue block (a block the catalogue lacks is a persona of a later version, and is left), `[tracks]`
  // steps whose `when`'s aliases are set; every alias verified and valid, an alias not listed false. An alias the
  // catalogue lacks is an error.
  [[nodiscard]] proto::Expected<FlagState, std::string> fixture(std::string_view toml) const;
  // The watch list for a realm (WatchList), and the flag state a FlagFrame gives against it: an alias not watched is
  // Unknown; a frame of another version leaves nothing read.
  [[nodiscard]] WatchList watch(Realm realm, std::uint32_t version) const;
  [[nodiscard]] FlagState from_frame(const WatchList& watch, const proto::FlagFrame& frame, Realm realm,
                                     std::uint8_t journey) const;

  [[nodiscard]] WorldView derive(const FlagState& fs) const;
  [[nodiscard]] std::size_t active_stage(std::size_t persona, const FlagState& fs) const;
  [[nodiscard]] std::vector<Known> known(std::size_t persona, const FlagState& fs, const WorldView& view,
                                         std::span<const Seen> seen = {}) const;
  [[nodiscard]] std::vector<KnownLine> known_lines(std::size_t persona, const FlagState& fs, const WorldView& view,
                                                   std::span<const Seen> seen = {}) const;
  [[nodiscard]] Grounding ground(std::size_t persona, const FlagState& fs, const WorldView& view,
                                 std::span<const Seen> seen = {}) const;
  [[nodiscard]] KnowledgeView view(const FlagState& fs) const;

  // A predicate's value over a flag state and the view (for tests and the debug view); an expression that does not
  // compile against this content is Unknown.
  [[nodiscard]] Tri eval(std::string_view expr, const FlagState& fs, const WorldView& view) const;

 private:
  friend struct KnowledgeBuilder;
  [[nodiscard]] std::optional<std::size_t> step_of(std::string_view track, std::string_view step) const;

  Catalogue catalogue_;
  std::vector<Region> regions_;
  std::vector<WorldStage> stages_;
  std::vector<Track> tracks_;
  std::vector<std::vector<KnowledgeOp>> programs_;           // compiled predicates, by id
  std::vector<std::size_t> stage_when_;             // per world stage: its program
  std::vector<std::vector<std::size_t>> step_when_;  // per track, per step
  std::vector<KnowledgePersona> personas_;
};

// The "you, now" line of block 9 (NPC §7.3; O35): the words after "You, now:", or empty when the persona is unhurt,
// not fighting and not afflicted, so a calm prompt pays nothing. The wounds from its HP by DESIGN §5.5's bands (at 75,
// 50 and 25 per cent: "unhurt", "bloodied", "badly hurt", "barely standing"), "in the thick of a fight" when it fights
// or was struck in the last 8 s, and its statuses' words, which wait for S8. With `self_awareness` off, nothing.
[[nodiscard]] std::string you_now(const proto::Meter& hp, bool fighting, bool self_awareness);

}  // namespace en::core
