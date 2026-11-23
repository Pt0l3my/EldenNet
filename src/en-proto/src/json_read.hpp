#pragma once
// How en-proto reads its JSON forms, shared by json.cpp (the model client's messages) and game_json.cpp (the game's):
// a path kept on the stack for the errors, a reader per type that refuses what is not its form, and an object's fields
// read by name, none unknown. Internal to en-proto. Every reader throws ProtoError naming the path and the problem,
// never a value.
#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <type_traits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "en/proto/json.hpp"

namespace en::proto::detail {

using nlohmann::json;

// Where a value sits in the document being read: kept on the stack, spelled out only for an error. The chain is as
// deep as the message types nest, never as deep as the text.
struct At {
  const At* parent = nullptr;
  std::string_view name;  // the message type at the root, a field below it; empty for an array's element
  std::size_t index = 0;  // an element's
};

std::string path_of(const At& at);
[[noreturn]] void fail(const At& at, const std::string& problem);

// A field's name as an error may show it: short, printable ASCII only. Values are never shown.
std::string shown_name(const std::string& name);

// The readers of plain values (json.cpp). Each type's own reader is declared beside its writer; the templates below
// find them at instantiation, through At's namespace.
void read(const json& j, const At& at, bool& out);
void read(const json& j, const At& at, int& out);
void read(const json& j, const At& at, std::uint64_t& out);
void read(const json& j, const At& at, double& out);
void read(const json& j, const At& at, std::string& out);
void read(const json& j, const At& at, std::chrono::milliseconds& out);
void read(const json& j, const At& at, json& out);

// Every integer type but bool and the two json.cpp reads (int, std::uint64_t), refusing what does not fit: the game
// messages' (game_json.cpp) and the UI's (ui_json.cpp).
template <class I>
  requires(std::is_integral_v<I> && !std::is_same_v<I, bool> && !std::is_same_v<I, int> &&
           !std::is_same_v<I, std::uint64_t>)
void read(const json& j, const At& at, I& out) {
  if constexpr (std::is_unsigned_v<I>) {
    std::uint64_t value = 0;
    if (j.is_number_unsigned()) {
      value = j.get<std::uint64_t>();
    } else if (j.is_number_integer() && j.get<std::int64_t>() >= 0) {
      value = static_cast<std::uint64_t>(j.get<std::int64_t>());
    } else {
      fail(at, "must be a whole number of 0 or more");
    }
    if (!std::in_range<I>(value)) fail(at, "is out of range");
    out = static_cast<I>(value);
  } else {
    std::int64_t value = 0;
    if (j.is_number_unsigned()) {
      const auto u = j.get<std::uint64_t>();
      if (!std::in_range<std::int64_t>(u)) fail(at, "is out of range");
      value = static_cast<std::int64_t>(u);
    } else if (j.is_number_integer()) {
      value = j.get<std::int64_t>();
    } else {
      fail(at, "must be a whole number");
    }
    if (!std::in_range<I>(value)) fail(at, "is out of range");
    out = static_cast<I>(value);
  }
}

template <class T>
void read(const json& j, const At& at, std::optional<T>& out) {
  if (j.is_null()) {
    out.reset();
    return;
  }
  T value{};
  read(j, at, value);
  out = std::move(value);
}

template <class T>
void read(const json& j, const At& at, std::vector<T>& out) {
  if (!j.is_array()) fail(at, "must be an array");
  out.clear();
  out.reserve(j.size());
  for (std::size_t i = 0; i < j.size(); ++i) {
    T value{};
    read(j[i], At{&at, {}, i}, value);
    out.push_back(std::move(value));
  }
}

// One object as one message: each field read by its own reader, none unknown; a field left out keeps its default.
class Fields {
 public:
  Fields(const json& j, const At& at) : j_(j), at_(at) {
    if (!j.is_object()) fail(at, "must be an object");
  }

  template <class T>
  Fields& operator()(const char* name, T& out) {
    names_.emplace_back(name);
    if (const auto it = j_.find(name); it != j_.end()) read(*it, At{&at_, name, 0}, out);
    return *this;
  }

  // Refuses a field that no call above named.
  void done() const {
    for (auto it = j_.begin(); it != j_.end(); ++it) {
      if (std::find(names_.begin(), names_.end(), it.key()) == names_.end()) {
        fail(at_, "has an unknown field " + shown_name(it.key()));
      }
    }
  }

 private:
  const json& j_;
  const At& at_;
  std::vector<std::string_view> names_;
};

// An enum by its name, one of `all`.
template <class E, std::size_t N>
void read_enum(const json& j, const At& at, const std::array<E, N>& all, E& out) {
  if (j.is_string()) {
    const std::string& name = j.get_ref<const std::string&>();
    for (const E e : all) {
      if (name == to_string(e)) {
        out = e;
        return;
      }
    }
  }
  std::string names;
  for (const E e : all) {
    if (!names.empty()) names += ", ";
    names += to_string(e);
  }
  fail(at, "must be one of " + names);
}

// The root of a document read as one message type.
template <class T>
void read_root(const json& j, const char* type, T& out) {
  read(j, At{nullptr, type, 0}, out);
}

}  // namespace en::proto::detail
