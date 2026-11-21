// gal/recipes.toml: Lodestar's recipes, shapes in our own grammar (recipes.hpp; proposal 0005 §6; O102).
#include "en/gal/recipes.hpp"

#include <algorithm>
#include <optional>
#include <utility>

#include "en/gal/ids.hpp"
#include "toml_reader.hpp"

namespace en::gal {

using proto::Expected;
using proto::Unexpected;
using toml_read::Errors;
using toml_read::Table;

const RecipeSpec* RecipeSet::find(std::string_view id) const noexcept {
  for (const RecipeSpec& r : recipes) {
    if (r.id == id) return &r;
  }
  return nullptr;
}

namespace {

// { method, leads?, evidence, date }, as patterns.toml's.
void read_provenance(const toml::table& t, const std::string& path, Errors& errors, Provenance& out) {
  Table p(t, path, errors);
  if (const auto method = p.text("method", true)) {
    if (*method != "own-derivation") errors.add(p.where("method"), "must be own-derivation");  // O4, O48
    out.method = *method;
  }
  if (const toml::array* leads = p.array("leads", false)) {
    out.leads = toml_read::strings_of(*leads, p.where("leads"), errors, 16, 64).value_or(std::vector<std::string>{});
  }
  if (const auto evidence = p.text("evidence", true)) {
    if (!toml_read::is_relative_path(*evidence)) errors.add(p.where("evidence"), "must be a relative path");
    out.evidence = *evidence;
  }
  if (const auto date = p.text("date", true)) {
    if (!toml_read::is_date(*date)) errors.add(p.where("date"), "must be a date, YYYY-MM-DD");
    out.date = *date;
  }
  p.done();
}

// The step a place is given by: within the shape, and of the form that gives that place.
std::optional<std::uint8_t> read_step(Table& g, std::string_view key, const shape::Shape& shape, shape::Form form,
                                      const char* problem, Errors& errors) {
  const auto step = g.integer(key, true, 0, static_cast<std::int64_t>(shape::kMaxSteps) - 1);
  if (!step) return std::nullopt;
  const auto at = static_cast<std::size_t>(*step);
  if (at >= shape.steps.size() || shape.steps[at] != form) {
    errors.add(g.where(key), problem);
    return std::nullopt;
  }
  return static_cast<std::uint8_t>(at);
}

void read_recipe(const toml::table& t, const std::string& path, Errors& errors, RecipeSpec& out) {
  Table r(t, path, errors);
  if (const auto id = r.text("id", true)) {
    if (!is_id_label(*id)) {
      errors.add(r.where("id"), "must be lower-case letters, digits and _, starting with a letter, 1 to 48 bytes");
    } else {
      out.id = *id;
    }
  }
  if (const auto kind = r.text("kind", true)) {
    if (*kind != "accessor") errors.add(r.where("kind"), "must be accessor");
  }
  if (const auto section = r.text("section", false)) {
    const bool named = !section->empty() && section->size() <= 8 &&
                       std::all_of(section->begin(), section->end(), [](char c) { return c > ' ' && c < 0x7F; });
    if (!named) {
      errors.add(r.where("section"), "must be a section's name of 1 to 8 printable ASCII characters");
    } else {
      out.section = *section;
    }
  }
  bool steps_read = false;
  if (const toml::array* steps = r.array("steps", true)) {
    if (steps->empty() || steps->size() > shape::kMaxSteps) {
      errors.add(r.where("steps"), "must list 1 to 16 forms");
    } else {
      steps_read = true;
      for (std::size_t i = 0; i < steps->size(); ++i) {
        const toml::node* node = steps->get(i);
        const auto* word = node != nullptr ? node->as_string() : nullptr;
        const std::optional<shape::Form> form = word != nullptr ? shape::form_of(word->get()) : std::nullopt;
        if (!form) {
          errors.add(toml_read::indexed(r.where("steps"), i),
                     "must be one of load_slot, test_loaded, branch_set_short, branch_set_near, address_arg1, "
                     "call_relative");
          steps_read = false;
        } else {
          out.shape.steps.push_back(*form);
        }
      }
      if (steps_read && !out.shape.valid()) {
        errors.add(r.where("steps"), "tests a register before any load");
        steps_read = false;
      }
    }
  }
  if (const toml::table* gives = r.table("gives", true)) {
    Table g(*gives, r.where("gives"), errors);
    if (steps_read) {
      if (const auto s = read_step(g, "static", out.shape, shape::Form::LoadSlot, "must be a load_slot step", errors)) {
        out.static_step = *s;
      }
      if (const auto s =
              read_step(g, "class", out.shape, shape::Form::AddressArg1, "must be an address_arg1 step", errors)) {
        out.class_step = *s;
      }
      if (const auto s =
              read_step(g, "getter", out.shape, shape::Form::CallRelative, "must be a call_relative step", errors)) {
        out.getter_step = *s;
      }
    } else {  // without its steps a place cannot be judged: only the keys are read
      (void)g.integer("static", true, 0, static_cast<std::int64_t>(shape::kMaxSteps) - 1);
      (void)g.integer("class", true, 0, static_cast<std::int64_t>(shape::kMaxSteps) - 1);
      (void)g.integer("getter", true, 0, static_cast<std::int64_t>(shape::kMaxSteps) - 1);
    }
    g.done();
  }
  if (const auto suffix = r.text("path_suffix", true)) {
    const bool printable = !suffix->empty() && suffix->size() <= 64 &&
                           std::all_of(suffix->begin(), suffix->end(), [](char c) { return c > ' ' && c < 0x7F; });
    if (!printable) {
      errors.add(r.where("path_suffix"), "must be 1 to 64 printable ASCII characters");
    } else {
      out.path_suffix = *suffix;
    }
  }
  if (const toml::node* expect = r.node("expect_matches", false)) {
    if (const auto range = toml_read::range_of(*expect, r.where("expect_matches"), errors, 1, kMaxRecipeMatches)) {
      out.expect_min = static_cast<std::uint32_t>(range->first);
      out.expect_max = static_cast<std::uint32_t>(range->second);
    }
  }
  if (const toml::array* builds = r.array("builds", false)) {
    if (auto keys = toml_read::strings_of(*builds, r.where("builds"), errors, 32, 32)) {
      for (std::size_t i = 0; i < keys->size(); ++i) {
        if (!toml_read::is_build_key((*keys)[i])) {
          errors.add(toml_read::indexed(r.where("builds"), i), "must be a build key such as 1.2.3.4-ww");
        }
      }
      out.builds = std::move(*keys);
    }
  }
  if (const toml::table* provenance = r.table("provenance", true)) {
    read_provenance(*provenance, r.where("provenance"), errors, out.provenance);
  }
  r.done();
}

}  // namespace

Expected<RecipeSet, SchemaErrors> parse_recipes(std::string_view text, std::span<const std::string_view> required_ids,
                                                std::string_view file) {
  Errors errors{std::string(file)};
  const std::optional<toml::table> doc = toml_read::parse(text, errors);
  if (!doc) return Unexpected{errors.take()};
  Table root(*doc, "", errors);
  (void)root.integer("schema", true, 1, 1);
  RecipeSet set;
  if (const toml::array* recipes = root.array("recipe", false)) {
    if (recipes->size() > 256) errors.add("recipe", "has more than 256 recipes");
    for (std::size_t i = 0; i < recipes->size() && i < 256; ++i) {
      const std::string path = toml_read::indexed("recipe", i);
      const toml::node* node = recipes->get(i);
      const toml::table* t = node != nullptr ? node->as_table() : nullptr;
      if (t == nullptr) {
        errors.add(path, "must be a table");
        continue;
      }
      RecipeSpec spec;
      read_recipe(*t, path, errors, spec);
      if (!spec.id.empty() && set.find(spec.id) != nullptr) errors.add(path + ".id", "is not unique");
      set.recipes.push_back(std::move(spec));
    }
  }
  root.done();
  for (const std::string_view id : required_ids) {
    if (set.find(id) == nullptr) errors.add("", "lacks the required recipe " + toml_read::shown(id));
  }
  if (!errors.empty()) return Unexpected{errors.take()};
  return set;
}

}  // namespace en::gal
