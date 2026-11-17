// The safety checks' file names (names.hpp).
#include "en/shell/names.hpp"

namespace en::shell {

namespace {

wchar_t lower(wchar_t c) noexcept { return c >= L'A' && c <= L'Z' ? static_cast<wchar_t>(c - L'A' + L'a') : c; }

bool starts_with_nocase(std::wstring_view s, std::wstring_view prefix) noexcept {
  if (s.size() < prefix.size()) return false;
  for (std::size_t i = 0; i < prefix.size(); ++i) {
    if (lower(s[i]) != lower(prefix[i])) return false;
  }
  return true;
}

}  // namespace

bool anticheat_module(std::wstring_view f) noexcept { return starts_with_nocase(f, L"easyanticheat"); }

bool protected_launcher(std::wstring_view f) noexcept {
  return f.size() == 24 && starts_with_nocase(f, L"start_protected_game.exe");
}

bool ersc_module(std::wstring_view f) noexcept { return f.size() == 8 && starts_with_nocase(f, L"ersc.dll"); }

std::wstring_view file_name_of(std::wstring_view path) noexcept {
  const std::size_t at = path.find_last_of(L"\\/");
  return at == std::wstring_view::npos ? path : path.substr(at + 1);
}

}  // namespace en::shell
