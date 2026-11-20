// The golden runner (golden_run.hpp; ARCH §8.12).
#include "golden_run.hpp"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>

#include <nlohmann/json.hpp>

#include "en/augur/golden.hpp"
#include "en/gal/report.hpp"

namespace en::tools {

std::optional<std::string> read_file(const std::string& path, std::uintmax_t max_bytes) {
  std::error_code ec;
  if (!std::filesystem::is_regular_file(path, ec)) return std::nullopt;
  const std::uintmax_t size = std::filesystem::file_size(path, ec);
  if (ec || size > max_bytes) return std::nullopt;
  std::ifstream in(path, std::ios::binary);
  std::string text(static_cast<std::size_t>(size), '\0');
  if (!in || (size != 0 && !in.read(text.data(), static_cast<std::streamsize>(size)))) return std::nullopt;
  return text;
}

std::optional<std::vector<shell::RecordedLine>> load_recording(const std::string& path, const char* cmd) {
  namespace report = en::gal::report;
  const auto text = read_file(path, kMaxRecordingBytes);
  if (!text) {
    std::fprintf(stderr, "%s: a recording cannot be read, or is larger than 512 MiB\n", cmd);
    return std::nullopt;
  }
  const report::Vocabulary* kinds[] = {&shell::recording_vocabulary()};
  if (const auto linted = report::lint_lines(*text, kinds); !linted) {
    for (const auto& p : linted.error()) std::fprintf(stderr, "%s: %s\n", cmd, report::describe(p).c_str());
    std::fprintf(stderr, "%s: the recording is refused by the lint\n", cmd);
    return std::nullopt;
  }
  std::vector<shell::RecordedLine> out;
  std::size_t at = 0;
  const std::string_view all(*text);
  while (at < all.size()) {
    std::size_t end = all.find('\n', at);
    if (end == std::string_view::npos) end = all.size();
    std::string_view line = all.substr(at, end - at);
    at = end + 1;
    if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
    if (line.empty()) continue;
    auto r = shell::read_recorded_line(line);
    if (!r) {
      std::fprintf(stderr, "%s: a line of the recording could not be read back\n", cmd);
      return std::nullopt;
    }
    out.push_back(std::move(*r));
  }
  return out;
}

proto::Expected<Run, std::vector<augur::RuleError>> run_core(std::vector<Step>& steps,
                                                            const std::vector<augur::RuleFile>* rules) {
  core::Core core;
  if (rules != nullptr) {
    auto errors = core.load_rules(*rules);
    if (!errors.empty()) return proto::Unexpected{std::move(errors)};
  } else if (!core.rule_errors().empty()) {
    return proto::Unexpected{core.rule_errors()};
  }
  Run run;
  run.rules_hash = core.augur().rules() ? core.augur().rules()->hash() : 0;
  for (Step& s : steps) {
    core::Outputs out = core.step(std::move(s.input), s.now);
    ++run.inputs;
    run.changes.insert(run.changes.end(), out.changes.begin(), out.changes.end());
    for (augur::Event& e : out.events) run.events.push_back(std::move(e));
  }
  return run;
}

std::vector<Step> steps_of(const std::vector<shell::RecordedLine>& lines) {
  std::vector<Step> steps;
  std::uint64_t now = 0;
  for (const shell::RecordedLine& l : lines) {
    now = std::max(now, l.t);  // a snapshot's line carries its own time, the others the core's step's
    switch (l.kind) {
      case shell::RecordedLine::Kind::Snap:
        steps.push_back(Step{now, core::CoreInput{std::make_unique<proto::WorldSnapshot>(*l.snap)}});
        break;
      case shell::RecordedLine::Kind::Life:
        steps.push_back(Step{now, core::CoreInput{l.life}});
        break;
      case shell::RecordedLine::Kind::Banner:
        steps.push_back(Step{now, core::CoreInput{core::BootBanner{l.banner, {}}}});
        break;
      default:
        break;
    }
  }
  return steps;
}

std::string golden_text(const std::string& route, const std::string& source, const Run& run) {
  nlohmann::ordered_json header;
  header["golden"] = 1;
  header["route"] = route;
  header["source"] = source;
  header["rules_hash"] = augur::hash_text(run.rules_hash);
  header["events"] = run.events.size();
  std::string out = header.dump() + "\n";
  augur::GoldenNames names;
  const augur::HandleNamer namer = [&](augur::ChrHandle h) { return names(h); };
  for (const augur::Event& e : run.events) out += augur::golden_line(e, namer) + "\n";
  return out;
}

namespace {

std::vector<std::string> lines_of(const std::string& text) {
  std::vector<std::string> out;
  std::size_t at = 0;
  while (at < text.size()) {
    std::size_t end = text.find('\n', at);
    if (end == std::string::npos) end = text.size();
    out.push_back(text.substr(at, end - at));
    at = end + 1;
  }
  return out;
}

}  // namespace

std::string line_diff(const std::string& want, const std::string& got, std::size_t max_lines) {
  if (want == got) return {};
  const auto a = lines_of(want);
  const auto b = lines_of(got);
  std::size_t pre = 0;
  while (pre < a.size() && pre < b.size() && a[pre] == b[pre]) ++pre;
  std::size_t suf = 0;
  while (suf < a.size() - pre && suf < b.size() - pre && a[a.size() - 1 - suf] == b[b.size() - 1 - suf]) ++suf;
  std::string out = "@@ golden line " + std::to_string(pre + 1) + ", run line " + std::to_string(pre + 1) + " @@\n";
  std::size_t n = 0;
  const auto put = [&](char sign, const std::string& line) {
    if (n++ < max_lines) out += std::string(1, sign) + line + "\n";
  };
  if (pre > 0) put(' ', a[pre - 1]);
  for (std::size_t i = pre; i < a.size() - suf; ++i) put('-', a[i]);
  for (std::size_t i = pre; i < b.size() - suf; ++i) put('+', b[i]);
  if (suf > 0) put(' ', a[a.size() - suf]);
  if (n > max_lines) out += "... and " + std::to_string(n - max_lines) + " more lines\n";
  return out;
}

}  // namespace en::tools
