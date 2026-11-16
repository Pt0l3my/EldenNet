# EldenNet: our own engine core (O84), built outside the game first (roadmap S8.1.a).
cmake_minimum_required(VERSION 3.20)
project(EldenNet VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(EN_BUILD_TESTS "Build the unit tests" ON)
option(EN_WITH_CURL "Build the libcurl transport (Linux and test builds)" ON)

# Header-only libraries (THIRD_PARTY.md). inja and doctest are vendored with their licences; nlohmann/json and toml++
# are downloaded at configure time from their original projects, pinned by SHA-256, into the build tree, so that the
# repository holds no third-party author's contact details (the privacy sweep's rule).
set(EN_DEPS ${CMAKE_BINARY_DIR}/_deps)
function(en_fetch url path sha256)
  if(NOT EXISTS ${EN_DEPS}/${path})
    file(DOWNLOAD ${url} ${EN_DEPS}/${path} EXPECTED_HASH SHA256=${sha256} TLS_VERIFY ON STATUS st)
    list(GET st 0 code)
    if(NOT code EQUAL 0)
      message(FATAL_ERROR "Could not download ${url}: ${st}")
    endif()
  endif()
endfunction()
en_fetch(https://raw.githubusercontent.com/nlohmann/json/v3.11.3/single_include/nlohmann/json.hpp
  nlohmann/json.hpp 9bea4c8066ef4a1c206b2be5a36302f8926f7fdc6087af5d20b417d0cf103ea6)
en_fetch(https://raw.githubusercontent.com/nlohmann/json/v3.11.3/LICENSE.MIT
  nlohmann/LICENSE.MIT 86b998c792894ccb911a1cb7994f7a9652894e7a094c0b5e45be2f553f45cf14)
en_fetch(https://raw.githubusercontent.com/marzer/tomlplusplus/v3.4.0/toml.hpp
  tomlplusplus/toml.hpp 6b5172ad4dd6519aec67b919181fa7a38a2234131e5b2afa232dfe444819783e)
en_fetch(https://raw.githubusercontent.com/marzer/tomlplusplus/v3.4.0/LICENSE
  tomlplusplus/LICENSE 529bc3900a9571e49db285b0df432397e70b881cc3bf48de6667ae74ff4b06d8)

add_library(en_third_party INTERFACE)
target_include_directories(en_third_party SYSTEM INTERFACE
  ${EN_DEPS}
  ${EN_DEPS}/tomlplusplus
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/inja)

if(MSVC)
  add_compile_options(/W4 /permissive- /utf-8)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# The modules the DLL links hold no static object with a dynamic initialiser (it would run under the loader lock) or
# with a destructor that runs at exit (it would run at DLL_PROCESS_DETACH): ARCH §3.1 and §1.7. Clang checks both.
function(en_dll_module target)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(${target} PRIVATE -Werror=exit-time-destructors -Werror=global-constructors)
  endif()
endfunction()

if(EN_BUILD_TESTS)
  enable_testing()
  add_library(en_doctest INTERFACE)
  target_include_directories(en_doctest SYSTEM INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/third_party/doctest)
endif()

add_subdirectory(src/en-prompt)
add_subdirectory(src/en-proto)
add_subdirectory(src/en-llm)
add_subdirectory(src/en-tools)
