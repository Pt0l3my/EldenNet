#pragma once
// en-prompt's errors. Every failed load or render names the template, the line and the column, and the chain of
// renders that led to it, so that a half-built prompt never goes out silently (NPC §7.1, WHY§99).
#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

namespace en::prompt {

// A place in a template. Line and column count from 1; 0 means the place is not known more closely.
struct Site {
  std::string template_name;
  std::size_t line = 0;
  std::size_t column = 0;
};

// "dialogue/10_identity.j2:3:7", shortened when the line or the column is not known.
std::string to_string(const Site& site);

// One problem found when loading or linting a template.
struct Finding {
  Site site;
  std::string rule;     // a short, stable id, such as "unknown-function"
  std::string message;  // one line, in plain words
};

// "dialogue/10_identity.j2:3:7: message [rule]".
std::string to_string(const Finding& finding);

// Raised by every failed load or render. A failed render returns nothing, so nothing half-built reaches a model.
class TemplateError : public std::runtime_error {
 public:
  TemplateError(const std::string& message, Site site, std::vector<Finding> findings = {},
                std::vector<Site> trace = {});

  const std::string& message() const noexcept { return message_; }
  const Site& site() const noexcept { return site_; }
  // Every problem a failed load found (the first is also message() and site()); empty for a render error.
  const std::vector<Finding>& findings() const noexcept { return findings_; }
  // The calls that rendered the failing template, innermost first.
  const std::vector<Site>& trace() const noexcept { return trace_; }

  // The same error, seen from the call that rendered its template.
  TemplateError rendered_from(const Site& caller) const;

 private:
  std::string message_;
  Site site_;
  std::vector<Finding> findings_;
  std::vector<Site> trace_;
};

}  // namespace en::prompt
