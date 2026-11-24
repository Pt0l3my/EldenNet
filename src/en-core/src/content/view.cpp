// The Content packs page's projection of each load (content_view.hpp; PAGES §4.6.9): counts, relative paths, lines and
// the content layer's fixed words, never a value of a file.
#include "en/core/content_view.hpp"

#include <algorithm>
#include <limits>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "en/core/core.hpp"
#include "en/core/profile.hpp"

namespace en::core {

namespace {

namespace pr = en::proto;

pr::ContentProblem problem_of(const ContentError& e, bool error) {
  const std::size_t line = std::min<std::size_t>(e.line, std::numeric_limits<std::uint32_t>::max());
  return pr::ContentProblem{static_cast<std::uint32_t>(line), error, e.problem};
}

void add_problem(pr::OverrideFile& f, pr::ContentProblem p) {
  if (f.problems.size() < pr::kContentProblemsMax) f.problems.push_back(std::move(p));
}

bool layer_file(std::string_view path) { return path.rfind(kLayerFolder, 0) == 0; }

// The rows by path, the row of the rest (its path empty) last.
std::vector<pr::OverrideFile> rows_of(std::map<std::string, pr::OverrideFile>& by_path) {
  std::vector<pr::OverrideFile> out;
  for (auto& [path, row] : by_path) {
    if (!path.empty()) out.push_back(std::move(row));
  }
  if (const auto rest = by_path.find(""); rest != by_path.end()) out.push_back(std::move(rest->second));
  return out;
}

}  // namespace

void record_load(proto::ContentView& view, std::span<const ContentFile> files, std::span<const ContentError> errors,
                 const Core& core, Realm realm, std::uint64_t at_ms, proto::ReloadLayer layer) {
  const bool ok = errors.empty();
  const bool earlier = view.loaded;

  // The shipped content: its cast in this realm, whatever the overrides hold; knowledge and rules as included.
  const std::vector<PersonaRow> shipped = shipped_index(files);
  view.personas = static_cast<std::uint32_t>(std::count_if(shipped.begin(), shipped.end(), [&](const PersonaRow& r) {
    return !r.test_only || realm == Realm::Eval;
  }));
  view.cards = 0;  // v0.2 ships no card
  view.knowledge = std::any_of(files.begin(), files.end(), [](const ContentFile& f) {
    return f.origin == ContentOrigin::Shipped && (f.path.rfind("flags/", 0) == 0 || f.path.rfind("world/", 0) == 0);
  });
  view.rules = !builtin_rule_files().empty();

  // The overrides folder's files, by path, in the load's state.
  std::map<std::string, pr::OverrideFile> by_path;
  for (const ContentFile& f : files) {
    if (f.origin != ContentOrigin::Override || !pr::shown_path(f.path)) continue;
    by_path[f.path] = pr::OverrideFile{f.path, ok ? pr::OverrideState::Loaded : pr::OverrideState::Waiting, {}};
  }
  const auto place = [&](const ContentError& e, bool error) {
    auto it = e.file.empty() ? by_path.end() : by_path.find(e.file);
    if (it == by_path.end()) {
      const pr::OverrideState state = ok ? pr::OverrideState::Loaded : pr::OverrideState::Refused;
      it = by_path.try_emplace("", pr::OverrideFile{"", state, {}}).first;
    }
    add_problem(it->second, problem_of(e, error));
    if (error) it->second.state = pr::OverrideState::Refused;
  };
  if (ok) {
    // The load's warnings, but a layer's: those come from its findings, an error among them refusing that layer alone.
    for (const ContentError& w : core.content_warnings()) {
      if (!layer_file(w.file)) place(w, false);
    }
    for (const LayerFinding& f : core.layer_findings()) place(to_content_error(f), f.error);
    view.loaded = true;
  } else {
    for (const ContentError& e : errors) place(e, true);
  }
  view.overrides = rows_of(by_path);

  // What disable.toml turns off in the content in use: the last load that passed (none before one).
  view.off_personas = core.directory().pack().disabled_personas;
  view.off_cards.clear();  // v0.2's content layer turns off personas alone
  view.off_knowledge = 0;
  view.off_triggers = 0;
  view.off_rules = 0;

  pr::ContentReload r;
  r.at_ms = at_ms;
  r.layer = layer;
  r.ok = ok;
  r.earlier = !ok && earlier;
  r.errors = static_cast<std::uint32_t>(errors.size());
  r.warnings = ok ? static_cast<std::uint32_t>(core.content_warnings().size()) : 0;
  view.reloads.insert(view.reloads.begin(), r);
  if (view.reloads.size() > pr::kContentReloadsMax) view.reloads.resize(pr::kContentReloadsMax);
  ++view.revision;
}

void record_left_out(proto::ContentView& view, std::span<const ContentError> left_out) {
  std::map<std::string, pr::OverrideFile> by_path;
  for (const ContentError& e : left_out) {
    const std::string path = pr::shown_path(e.file) ? e.file : std::string();
    auto& row = by_path.try_emplace(path, pr::OverrideFile{path, pr::OverrideState::LeftOut, {}}).first->second;
    add_problem(row, problem_of(e, true));
  }
  view.left_out = rows_of(by_path);
  ++view.revision;
}

}  // namespace en::core
