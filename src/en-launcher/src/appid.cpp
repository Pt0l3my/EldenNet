// The appid file and the launcher's record of it (appid.hpp; ARCH §4.5).
#include "en/launcher/appid.hpp"

#include <fstream>
#include <system_error>
#include <toml.hpp>

#include "en/launcher/steam.hpp"
#include "en/proto/toml_check.hpp"

namespace en::launcher {

namespace fs = std::filesystem;

const char* to_string(AppidMethod m) noexcept { return m == AppidMethod::File ? "file" : "env"; }

const char* to_string(RemoveAfter r) noexcept {
  switch (r) {
    case RemoveAfter::Window:
      return "window";
    case RemoveAfter::Exit:
      return "exit";
    case RemoveAfter::Never:
      return "never";
  }
  return "window";
}

std::optional<AppidMethod> appid_method_named(std::string_view s) noexcept {
  if (s == "file") return AppidMethod::File;
  if (s == "env") return AppidMethod::Env;
  return std::nullopt;
}

std::optional<RemoveAfter> remove_after_named(std::string_view s) noexcept {
  if (s == "window") return RemoveAfter::Window;
  if (s == "exit") return RemoveAfter::Exit;
  if (s == "never") return RemoveAfter::Never;
  return std::nullopt;
}

const char* to_string(AppidStep s) noexcept {
  switch (s) {
    case AppidStep::Created:
      return "created";
    case AppidStep::Used:
      return "used";
    case AppidStep::Foreign:
      return "foreign";
    case AppidStep::Unwritable:
      return "unwritable";
  }
  return "unwritable";
}

const char* to_string(AppidRemoval r) noexcept {
  switch (r) {
    case AppidRemoval::Removed:
      return "removed";
    case AppidRemoval::Gone:
      return "gone";
    case AppidRemoval::NotOurs:
      return "not_ours";
    case AppidRemoval::Changed:
      return "changed";
    case AppidRemoval::Failed:
      return "failed";
  }
  return "failed";
}

namespace {

std::string toml_string(std::string_view s) {
  std::string out = "\"";
  for (const char c : s) {
    switch (c) {
      case '"':
        out += "\\\"";
        break;
      case '\\':
        out += "\\\\";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        if (static_cast<unsigned char>(c) < 0x20) {
          static constexpr char kHex[] = "0123456789ABCDEF";
          out += "\\u00";
          out.push_back(kHex[(c >> 4) & 0xF]);
          out.push_back(kHex[c & 0xF]);
        } else {
          out.push_back(c);
        }
    }
  }
  out.push_back('"');
  return out;
}

}  // namespace

std::string state_text(const std::optional<AppidRecord>& record) {
  std::string out = "# EldenNet's launcher: what it must tidy up (ARCH 4.5). Written by the launcher.\nschema = 1\n";
  if (record) {
    out += "\n[appid]\nfile = " + toml_string(path_to_utf8(record->file)) +
           "\ncreated_by_us = " + (record->created_by_us ? "true" : "false") +
           "\ncreated_at = " + toml_string(record->created_at) + "\n";
  }
  return out;
}

std::optional<AppidRecord> read_state(std::string_view text) {
  if (proto::unsafe_for_toml(text, proto::TomlText::Player)) return std::nullopt;
  toml::parse_result parsed = toml::parse(text);
  if (!parsed) return std::nullopt;
  const toml::table& t = parsed.table();
  const toml::table* appid = t["appid"].as_table();
  if (appid == nullptr) return std::nullopt;
  const auto file = (*appid)["file"].value<std::string>();
  const auto ours = (*appid)["created_by_us"].value<bool>();
  const auto at = (*appid)["created_at"].value<std::string>();
  if (!file || file->empty() || !ours || !at) return std::nullopt;
  return AppidRecord{path_from_utf8(*file), *ours, *at};
}

AppidFile classify(const std::optional<std::string>& content, std::uint32_t appid) noexcept {
  if (!content) return AppidFile::Absent;
  std::string_view v = *content;
  const auto blank = [](char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; };
  while (!v.empty() && blank(v.front())) v.remove_prefix(1);
  while (!v.empty() && blank(v.back())) v.remove_suffix(1);
  return v == std::to_string(appid) ? AppidFile::Ours : AppidFile::Other;
}

AppidPrepared prepare_appid_file(const fs::path& game_dir, std::uint32_t appid, const std::string& stamp) {
  const fs::path file = game_dir / path_from_utf8(kAppidFileName);
  std::error_code ec;
  const bool exists = fs::exists(file, ec);
  const auto content = exists ? read_text(file, 4096) : std::nullopt;
  if (exists) {
    // A file we cannot read, or one with other text, is someone else's: left alone.
    return AppidPrepared{classify(content, appid) == AppidFile::Ours ? AppidStep::Used : AppidStep::Foreign,
                         std::nullopt};
  }
  {
    std::ofstream out(file, std::ios::binary | std::ios::trunc);
    out << appid;
    out.flush();
    if (!out) {
      out.close();
      fs::remove(file, ec);
      return AppidPrepared{AppidStep::Unwritable, std::nullopt};
    }
  }
  return AppidPrepared{AppidStep::Created, AppidRecord{file, true, stamp}};
}

AppidRemoval remove_if_ours(const AppidRecord& record, std::uint32_t appid) {
  if (!record.created_by_us) return AppidRemoval::NotOurs;
  std::error_code ec;
  if (!fs::exists(record.file, ec)) return AppidRemoval::Gone;
  if (classify(read_text(record.file, 4096), appid) != AppidFile::Ours) return AppidRemoval::Changed;
  return fs::remove(record.file, ec) && !ec ? AppidRemoval::Removed : AppidRemoval::Failed;
}

}  // namespace en::launcher
