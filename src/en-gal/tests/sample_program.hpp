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

}  // namespace en::gal::test
