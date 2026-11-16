#pragma once
// galtest.dll's side of the watch block (en-proto's watch.hpp; O71): what EldenNet's DLL will do there (part 6), in
// miniature, so that the launcher's crash watcher is tested end to end: T1's states and the status, a crash filter of
// its own (chained, allocation-free, waiting at most 2 s for the watcher), and a crash on the launcher's request.
// Nothing happens without a block: the launcher makes it before it injects, or the watcher galtest starts when asked.
#include <cstdint>
#include <string>

#include "en/proto/watch.hpp"

namespace galtest::watch {

// Opens the launcher's block for this process, if there is one. Call once, on T1.
bool attach() noexcept;
// No launcher made a block (another loader loaded us): start the watcher, as EldenNet will (O71: "players who start
// the game through their own loader get the watcher started by EldenNet"), and wait at most 3 s for its block.
bool start_watcher(const std::string& launcher, const std::string& args) noexcept;
bool attached() noexcept;
// The crash filter, chained to the one before it. Installed once the game has started (its statics are there), not
// at attach: a DLL loaded into the suspended game runs before the game's C runtime starts, and the runtime's start
// installs a filter of its own, which does not chain, over ours.
void install_filter() noexcept;
void state(en::proto::watch::InitState s) noexcept;
void items(std::uint32_t resolved, std::uint32_t total, std::uint32_t passes) noexcept;
void counts(std::uint64_t snapshots, std::uint64_t faults, std::uint64_t tripped, std::uint32_t task_us_p99) noexcept;
void core(std::uint32_t phase, std::uint32_t level, std::uint32_t banner, std::uint32_t flags) noexcept;
void ready_ms(std::uint32_t ms) noexcept;
// The task's thread, once it has run there: a crash on it is the game thread's.
void game_thread(std::uint32_t tid) noexcept;
// The launcher asked for a crash (s0d-crash).
bool crash_requested() noexcept;
// The Marks the launcher has stamped (O90).
std::uint32_t marks() noexcept;

}  // namespace galtest::watch
