#pragma once
// en-gal's RTTI walk (ARCH §6.2), over an image read once (image_bytes.hpp): MSVC's run-time type information for x64,
// read from its documented layouts.
// - A type descriptor: a pointer (the type_info vtable), a spare pointer, then the decorated name, ".?AV" (a class) or
//   ".?AU" (a struct), the name, its enclosing scopes innermost first, each followed by '@', and a closing '@'.
// - A complete-object locator: signature 1 (x64), the subobject's offset in the complete object, the constructor
//   displacement's offset, and the image-relative offsets of the type descriptor, of the class hierarchy descriptor and
//   of the locator itself, which is how a locator is told from other data.
// - A class hierarchy descriptor: signature 0, attributes, the number of bases, and the offset of the base class array,
//   whose entries are the offsets of base class descriptors, each starting with its type descriptor's offset; the
//   class itself comes first.
// - A vtable's first slot lies one pointer after a pointer to its locator; its slots are pointers into code.
// The walk finds every locator and vtable; classes are then found by their decorated names. Nothing here knows the
// game: the classes come from the caller (the derivation's leads, the layout's `rtti`).
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "en/gal/image_bytes.hpp"

namespace en::gal::rtti {

struct Locator {
  std::uint32_t rva = 0;
  std::uint32_t offset = 0;           // the subobject's offset in the complete object; 0 for the primary vtable
  std::uint32_t type_descriptor = 0;  // the complete object's class
  std::uint32_t hierarchy = 0;
  std::vector<std::uint32_t> bases;  // the base class array's type descriptors, the class itself first
};

struct Vtable {
  std::uint32_t rva = 0;    // its first slot
  std::size_t locator = 0;  // into Walk::locators
  std::uint32_t slots = 0;  // consecutive pointers into executable sections
};

struct Walk {
  std::vector<Locator> locators;  // by RVA
  std::vector<Vtable> vtables;    // by RVA
  std::uint64_t rejected = 0;     // places with a locator's signature and self offset whose other parts did not hold
};

struct Limits {
  std::uint32_t max_bases = 1024;
  std::uint32_t max_slots = 2048;
  std::size_t max_locators = 1u << 20;
};

[[nodiscard]] Walk walk(const ImageBytes& img, const Limits& limits = {});

// The decorated name of "Space::Inner::Class": ".?AVClass@Inner@Space@@", or ".?AU…" for a struct; nothing for a name
// that is not identifiers joined by "::".
[[nodiscard]] std::optional<std::string> decorate(std::string_view cls, bool structure = false);

// A class's type descriptor, found by its decorated name (as a class or a struct) in the image's initialised data,
// with a pointer into the image and a null spare pointer before it. `count` is how many there were: one is found,
// none is missing, more is ambiguous (and `rva` the first).
struct Found {
  std::uint32_t rva = 0;
  std::size_t count = 0;
};
[[nodiscard]] Found find_type_descriptor(const ImageBytes& img, std::string_view cls);

// The vtables of the classes that list `base` among their bases (the class `base` names itself included, unless
// `strict`), primary ones (offset 0) and the others apart, each by RVA.
struct Set {
  std::vector<std::uint32_t> primary;
  std::vector<std::uint32_t> subobject;
  std::size_t classes = 0;  // distinct classes
};
[[nodiscard]] Set derived_from(const Walk& walk, std::uint32_t base, bool strict);

// The vtables of one class: those whose locator's type descriptor is `type_descriptor`.
[[nodiscard]] std::vector<const Vtable*> vtables_of(const Walk& walk, std::uint32_t type_descriptor);

}  // namespace en::gal::rtti
