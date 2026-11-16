// en-prompt's errors: what they say, and how a render error gathers the calls that led to it.
#include "en/prompt/errors.hpp"

#include <utility>

#include "detail.hpp"

namespace en::prompt {

std::string to_string(const Site& site) {
  std::string out = site.template_name.empty() ? std::string("<template>") : site.template_name;
  if (site.line > 0) {
    out += ':' + std::to_string(site.line);
    if (site.column > 0) out += ':' + std::to_string(site.column);
  }
  return out;
}

std::string to_string(const Finding& finding) {
  return to_string(finding.site) + ": " + finding.message + " [" + finding.rule + "]";
}

namespace {

std::string describe_error(const std::string& message, const Site& site, const std::vector<Finding>& findings,
                           const std::vector<Site>& trace) {
  std::string out;
  if (findings.size() > 1) {
    out = std::to_string(findings.size()) + " problems in " + site.template_name + ":";
    for (const Finding& finding : findings) out += "\n  " + to_string(finding);
  } else {
    out = to_string(site) + ": " + message;
  }
  for (const Site& caller : trace) out += "\n  rendered from " + to_string(caller);
  return out;
}

}  // namespace

TemplateError::TemplateError(const std::string& message, Site site, std::vector<Finding> findings,
                             std::vector<Site> trace)
    : std::runtime_error(describe_error(message, site, findings, trace)),
      message_(message),
      site_(std::move(site)),
      findings_(std::move(findings)),
      trace_(std::move(trace)) {}

TemplateError TemplateError::rendered_from(const Site& caller) const {
  std::vector<Site> trace = trace_;
  trace.push_back(caller);
  return TemplateError(message_, site_, findings_, std::move(trace));
}

namespace detail {

TemplateError error_from(const std::vector<Finding>& findings) {
  const Finding& first = findings.front();
  std::string message = first.message;
  if (findings.size() > 1) message += " (and " + std::to_string(findings.size() - 1) + " more)";
  return TemplateError(message, first.site, findings);
}

}  // namespace detail
}  // namespace en::prompt
