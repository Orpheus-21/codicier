#pragma once

#include <toml++/toml.hpp>

#include <functional>
#include <span>
#include <string>
#include <string_view>

// Writes TOML in one canonical layout, so the same data always gives the same
// bytes. The keys of a table come in a given order, then the other keys in
// alphabetical order. Plain values come first, then tables and arrays of
// tables as sections. A string with a line break is a multi-line string.
namespace TomlWriter {

using Order = std::span<const std::string_view>;
// Gives the key order for the table at a path, such as "goals" or "item".
using OrderFor = std::function<Order(std::string_view path)>;

std::string key(std::string_view key);
std::string value(const toml::node &node);
std::string document(const toml::table &table, const OrderFor &orderFor);

} // namespace TomlWriter
