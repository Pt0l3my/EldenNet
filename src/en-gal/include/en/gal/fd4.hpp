#pragma once
// en-gal's FD4 singleton finder (ARCH §6.2; DESIGN §4.2), our own code written from the documented idiom: an accessor
// loads a static (`mov reg, [rip+static]`), tests it (`test reg, reg; jnz short`), and while it is null asks a runtime
// class object for its name (`lea rcx, [rip+class]; call getter`) to log, with a path, that the singleton was used
// before it was set. Each hit of the idiom's pattern is decoded first: the static's, the class object's and the path's
// displacements must be RIP-relative operands and the getter's a relative call, ending where the captures say (a
// pattern that wildcards a register's ModRM also matches other addressing forms, which are not the idiom). Then it is
// checked for placement (the static in .data and aligned to 8, the class object in .data, the getter in .text at the
// start of a function of the exception directory, the path in .rdata, an ASCII string of at most 260 bytes ending with
// the suffix given), named through the class object where its name layout is known, and indexed by name: a name found
// with two statics is ambiguous. Without a path capture, the path is the load whose string ends with the suffix, of
// every load of the block the idiom's jump skips (whichever comes first, in whichever register, before or after the
// getter's call), or of the few instructions before the hit in its function; a load is a RIP-relative address, a
// RIP-relative pointer to the string, or a 64-bit absolute address. It reads an image read once (image_bytes.hpp), offline the file and in
// game the mapped image, and calls nothing. The pattern, the suffix and the layout come from the caller (gal/'s files,
// or the derivation's leads): nothing here knows the game.
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "en/gal/image_bytes.hpp"
#include "en/gal/patterns.hpp"

namespace en::gal::fd4 {

enum class NameEncoding : std::uint8_t { Ascii, Utf16 };

// Where a runtime class object holds a pointer to its name, and how the name is written.
struct NameLayout {
  std::uint32_t offset = 0;
  NameEncoding encoding = NameEncoding::Ascii;
  friend bool operator==(const NameLayout&, const NameLayout&) = default;
};

// The idiom's pattern and where its displacements lie (patterns.toml's `fd4_idiom`, with captures named static, class,
// getter and, when the pattern reaches the path's load, path).
struct Idiom {
  BytePattern bytes;
  Capture static_slot;
  Capture class_object;
  Capture getter;
  std::optional<Capture> path;  // without it, the path's load is the first load from .rdata in the block the jump skips
};

// The idiom of an `idiom` pattern of patterns.toml, from its captures' names; nothing without static, class and getter.
[[nodiscard]] std::optional<Idiom> idiom_from(const PatternSpec& spec);

enum class Refusal : std::uint8_t {
  Unreadable,         // a displacement could not be read as the idiom's (not RIP-relative, or not a call's), or points
                      // outside the image
  StaticSection,      // the static is not in the data section
  StaticAlignment,    // the static is not aligned to 8
  ClassSection,       // the class object is not in the data section
  GetterSection,      // the getter is not in the code section
  GetterNotFunction,  // the getter does not start a function of the exception directory
  NoPath,             // no load of a path follows within reach
  PathSection,        // the path is not in the read-only data section
  PathText,           // the path is not ASCII of at most 260 bytes ending with the suffix
};

[[nodiscard]] const char* to_string(Refusal refusal) noexcept;

// How a site loads its path (without a path capture), for the derivation's diagnostics.
enum class PathForm : std::uint8_t {
  Address,   // a RIP-relative operand addressing the string (lea reg, [rip+path])
  Pointer,   // a RIP-relative load of a pointer to the string (mov reg, [rip+slot])
  Absolute,  // a 64-bit absolute address (mov reg, imm64)
};

struct PathLoad {
  std::uint32_t insn = 0;  // the instruction's RVA
  std::uint8_t length = 0;
  std::uint8_t field_at = 0;  // the address's offset in it: the displacement's, or the 64-bit immediate's
  std::uint8_t map = 0;  // its opcode, as x86.hpp gives it
  std::uint8_t opcode = 0;
  std::uint8_t reg = 0;  // the register it writes (ModRM's reg, or the opcode's for B8+r), 0 to 15
  PathForm form = PathForm::Address;
  std::uint32_t ordinal = 0;  // the other references into the read-only section the search met before it
  bool hoisted = false;       // before the hit, outside the block the jump skips
};

struct Site {
  std::uint32_t hit = 0;
  std::uint32_t static_slot = 0;
  std::uint32_t class_object = 0;
  std::uint32_t getter = 0;
  std::uint32_t path = 0;
  std::optional<PathLoad> path_load;  // where the path was found, without a path capture
  std::optional<Refusal> refused;
  std::optional<std::string> name;  // read through the name layout; kept in memory, never written to a report
};

struct Singleton {
  std::string name;
  std::uint32_t static_slot = 0;  // the first site's, when ambiguous
  std::uint32_t class_object = 0;
  std::uint32_t getter = 0;
  std::uint32_t sites = 0;
  bool ambiguous = false;  // its sites load different statics
};

struct Config {
  std::string_view code = ".text";
  std::string_view data = ".data";
  std::string_view rdata = ".rdata";
  std::string_view path_suffix;
  std::optional<NameLayout> names;  // without it no name is read (in game the getter is called instead, ARCH §6.2)
  std::size_t max_hits = 1u << 16;
  std::uint32_t path_reach = 96;  // without a path capture, and without a jump in the pattern to bound the search
  std::uint32_t back_reach = 64;  // and how far before the hit, within its function, a hoisted load is looked for
  // R0's rule, kept for the derivation's comparison: no decoding of the hit, and the path is the first reference into
  // the read-only section after the pattern, whatever its text.
  bool first_reference = false;
};

struct Result {
  std::uint64_t hits = 0;             // the pattern's matches in the code section
  std::vector<Site> sites;            // the first max_hits of them
  std::vector<Singleton> singletons;  // by name, from the accepted sites whose names read
  std::uint64_t unnamed = 0;          // accepted sites whose names did not read
};

[[nodiscard]] Result find(const ImageBytes& img, const Idiom& idiom, const Config& config);

// The name a class object gives through a layout: printable ASCII of 1 to 128 characters (UTF-16 narrowed alike).
[[nodiscard]] std::optional<std::string> class_name(const ImageBytes& img, std::uint32_t class_object,
                                                    const NameLayout& layout);

// Whether the path at `rva` is ASCII of at most 260 bytes ending with `suffix`.
[[nodiscard]] bool path_matches(const ImageBytes& img, std::uint32_t rva, std::string_view suffix);

}  // namespace en::gal::fd4
