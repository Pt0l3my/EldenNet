#pragma once
// The derivation's invented test program (FakeProgram): FD4-style accessors of a shape of their own, runtime class
// objects that name them at +0x10, RTTI for an actor-like hierarchy (with a second base and a same-named class in
// another space), task classes, a registration function with its own argument order (the instance in r9, the task in
// rdx, the group in ecx) and two call sites, a table of UTF-16 group names, and functions that read two statics.
// Every name, number and byte is made up here.
#include <string>

#include "fake_program.hpp"

namespace en::gal::test {

struct Sample {
  FakeProgram program;
  BuiltImage built;
};

// The leads the tests derive with: the singletons with three decoys and one absent name, two vtable sets, the task
// base, the group and two statics.
inline constexpr const char* kSampleLeads = R"(schema = 1
product_name = "INVENTED GAME"

[fd4]
path_suffix = "Lone.h"

[singletons]
hub = "Hub"
roster = "Roster"
clock = "Clock"
dup = "Dup"
stray = "Stray"
rock = "Rock"
absent = "Nowhere"

[register]
instance = "hub"

[task]
group = "Gr_Settle"
base = "Tasks::Base"

[vtable_sets]
body = "Kin::Body"
beast = "Kin::Beast"

[statics]
ledger = { rtti = "Store::Ledger" }
module_base = { module = true }
)";

// The same without the decoys and the absent name: everything it asks for is there.
inline constexpr const char* kSampleLeadsClean = R"(schema = 1
product_name = "INVENTED GAME"

[fd4]
path_suffix = "Lone.h"

[singletons]
hub = "Hub"
roster = "Roster"
clock = "Clock"

[register]
instance = "hub"

[task]
group = "Gr_Settle"
base = "Tasks::Base"

[vtable_sets]
body = "Kin::Body"

[statics]
ledger = { rtti = "Store::Ledger" }
)";

Sample build_sample();

// The variants (R0c), each an accessor of a name of its own with the head shared (mov reg, [rip+static] in rdi, rbx
// or r12; test; jnz; lea rcx, [rip+class]; call getter): Anvil loads its path as the sample does; Bellows loads a
// format string first; Crucible loads the path into rdx; Dross through a pointer to it; Ember through its absolute
// address; Forge into rsi before the head; and a decoy that loads its "static" through rbx. A group table of 16-byte
// entries { index (its place + 10), flags, name } that code addresses at its first entry; two statics whose classes
// have no RTTI of that name, one with a class of its name in another namespace; and a trademark sign after the product
// name.
inline constexpr const char* kVariantLeads = R"(schema = 1
product_name = "VARIANT GAME"

[fd4]
path_suffix = "Lone.h"

[singletons]
anvil = "Anvil"
bellows = "Bellows"
crucible = "Crucible"
dross = "Dross"
ember = "Ember"
forge = "Forge"

[register]
instance = "anvil"

[task]
group = "Va_Settle"
base = "Va::Task"

[statics]
plain = { rtti = "Va::Plain" }
moved = { rtti = "Va::Moved" }
)";

Sample build_variants();

// Three accessors that load the path before the getter's call (lea rsi, [rip+path] right after the jump), so that the
// idiom cut from them captures it.
inline constexpr const char* kPathEarlyLeads = R"(schema = 1

[fd4]
path_suffix = "Lone.h"

[singletons]
gorge = "Gorge"
heath = "Heath"
isle = "Isle"

[register]
instance = "gorge"

[task]
group = "Ea_None"
base = "Ea::Task"
)";

Sample build_path_early();

}  // namespace en::gal::test
