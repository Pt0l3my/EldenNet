#pragma once
// The file names the safety checks look for (ARCH §5.3, §1.6), compared without case and with ASCII rules only: a
// module whose file name starts with "EasyAntiCheat", a parent process "start_protected_game.exe", and Seamless
// Co-op's "ersc.dll" (C14). Also the reduction of a module's path to its file name, which is all a record keeps.
#include <string_view>

namespace en::shell {

[[nodiscard]] bool anticheat_module(std::wstring_view file_name) noexcept;
[[nodiscard]] bool protected_launcher(std::wstring_view file_name) noexcept;
[[nodiscard]] bool ersc_module(std::wstring_view file_name) noexcept;
// What follows the last '\' or '/'.
[[nodiscard]] std::wstring_view file_name_of(std::wstring_view path) noexcept;

}  // namespace en::shell
