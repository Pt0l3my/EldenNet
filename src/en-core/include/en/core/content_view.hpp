#pragma once
// The Content packs page's projection of each load of the content (PAGES §4.6.9; roadmap S11.4.a): en-proto's
// ContentView, which en-overlay's space publishes, built from what the load was given and what it answered, and from
// the core after it. Pure (ARCH §2.3): the DLL calls it on T6 after each Core::load_content, with the time.
//
// - **Shipped content.** The shipped index's personas the realm voices (test personas only in the tests' realm), read
//   whatever the overrides hold; cards (v0.2 ships none); and whether the shipped pack holds knowledge (the flag
//   catalogue or the world files) and event rules (the rule files built in), never their counts (SP5).
// - **Your overrides.** Every file of the overrides folder the load was given, by its relative path: after a load that
//   passed, Loaded, or Refused for a player's layer whose errors refused it alone, each with the load's warnings and
//   findings about it; after a refused load, Refused for a file the errors name and Waiting for the rest. The errors
//   that name no file of the folder (a shipped file's, or the count past the content layer's 64) go on a row of their
//   own, its path empty, and so do those of a path that is not fit to show (en-proto's shown_path, PV5), which is
//   never put on a row.
// - **Turned off.** disable.toml's, from the content in use (the last load that passed).
// - **Reloads.** Each load, newest first: its time, what it read, its result and its counts.
// Only the content layer's fixed words are kept (ContentError::problem), never a value of a file.
#include <cstdint>
#include <span>

#include "en/core/content.hpp"
#include "en/core/identity.hpp"
#include "en/proto/content_view.hpp"

namespace en::core {

class Core;

// Records one load into `view`: the files it was given (the shipped pack's, then the overrides folder's), the errors
// Core::load_content answered (none: it loaded), the core after it, the realm, the time since T1's start and what the
// load read. `view.left_out` is the caller's. The revision changes.
void record_load(proto::ContentView& view, std::span<const ContentFile> files, std::span<const ContentError> errors,
                 const Core& core, Realm realm, std::uint64_t at_ms, proto::ReloadLayer layer);

// The files the shell left out of the overrides folder, each an error on its relative path (line 0, fixed words), as
// the rows of `view.left_out`, by path (one not fit to show, PV5, on a row of its own, its path empty). The revision
// changes.
void record_left_out(proto::ContentView& view, std::span<const ContentError> left_out);

}  // namespace en::core
