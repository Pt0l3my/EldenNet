# The link rules' own test (cmake/link_rules.cmake): a project of interface targets alone, configured once for each
# case by CTest (the top-level CMakeLists.txt), that stands in for the launcher's graph. `clean` configures; every other
# case links the stand-in for en-core in one way the rule must see, and the configuration stops with its chain.
cmake_minimum_required(VERSION 3.20)
project(EnLinkRules LANGUAGES NONE)
include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/link_rules.cmake)

add_library(proto INTERFACE)
add_library(gal INTERFACE)
target_link_libraries(gal INTERFACE proto)
add_library(core INTERFACE)
add_library(shell INTERFACE)
target_link_libraries(shell INTERFACE core gal)
add_library(launcher_core INTERFACE)
target_link_libraries(launcher_core INTERFACE gal proto)
add_library(launcher INTERFACE)
target_link_libraries(launcher INTERFACE launcher_core)
en_link_rule(launcher ALLOW launcher_core gal proto)

if(EN_CASE STREQUAL "through")      # the way it was: the launcher linked en_shell, which links en-core
  target_link_libraries(launcher INTERFACE shell)
elseif(EN_CASE STREQUAL "deep")     # a module the launcher may link starts to link en-core, after the rule
  target_link_libraries(gal INTERFACE core)
elseif(EN_CASE STREQUAL "link_only")  # a static library's private link, as the program's link line holds it
  set_property(TARGET launcher_core APPEND PROPERTY INTERFACE_LINK_LIBRARIES "$<LINK_ONLY:core>")
elseif(EN_CASE STREQUAL "condition")  # behind a condition that is false in this build: refused all the same
  target_link_libraries(launcher INTERFACE "$<$<BOOL:0>:core>")
elseif(EN_CASE STREQUAL "alias")    # under another name
  add_library(en::core ALIAS core)
  target_link_libraries(launcher INTERFACE en::core)
elseif(NOT EN_CASE STREQUAL "clean")
  message(FATAL_ERROR "EN_CASE: clean, through, deep, link_only, condition or alias")
endif()
