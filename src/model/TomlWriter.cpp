#include "TomlWriter.h"

#include <algorithm>
#include <array>
#include <sstream>
#include <vector>

namespace {

bool isSection(const toml::node &node)
{
    if (node.is_table())
        return true;
    const toml::array *array = node.as_array();
    return array != nullptr && !array->empty() && array->is_array_of_tables();
}

void escape(std::string &out, unsigned char c)
{
    constexpr std::string_view hex = "0123456789ABCDEF";
    out += "\\u00";
    out += hex[c >> 4];
    out += hex[c & 0xF];
}

std::string basicString(std::string_view text)
{
    std::string out = "\"";
    for (const char ch : text) {
        const auto c = static_cast<unsigned char>(ch);
        switch (c) {
        case '"':
            out += "\\\"";
            break;
        case '\\':
            out += "\\\\";
            break;
        case '\b':
            out += "\\b";
            break;
        case '\t':
            out += "\\t";
            break;
        case '\n':
            out += "\\n";
            break;
        case '\f':
            out += "\\f";
            break;
        case '\r':
            out += "\\r";
            break;
        default:
            if (c < 0x20 || c == 0x7F)
                escape(out, c);
            else
                out += ch;
        }
    }
    return out + "\"";
}

// A multi-line string keeps its line breaks as they are. A quote gets a
// backslash only before another quote or at the end, so three quotes never
// meet and the closing quotes stay clear.
std::string multiLineString(std::string_view text)
{
    std::string out = "\"\"\"\n";
    for (std::size_t i = 0; i < text.size(); ++i) {
        const auto c = static_cast<unsigned char>(text[i]);
        if (c == '"' && (i + 1 == text.size() || text[i + 1] == '"'))
            out += "\\\"";
        else if (c == '\\')
            out += "\\\\";
        else if (c == '\r')
            out += "\\r";
        else if ((c < 0x20 && c != '\n' && c != '\t') || c == 0x7F)
            escape(out, c);
        else
            out += text[i];
    }
    return out + "\"\"\"";
}

// The keys of a table: the ordered keys that the table has, then the rest in
// alphabetical order. toml::table keeps its keys sorted already.
std::vector<std::string_view> sortedKeys(const toml::table &table, TomlWriter::Order order)
{
    std::vector<std::string_view> keys;
    for (const std::string_view key : order) {
        if (table.contains(key))
            keys.push_back(key);
    }
    for (const auto &[key, node] : table) {
        if (std::ranges::find(order, key.str()) == order.end())
            keys.push_back(key.str());
    }
    return keys;
}

std::string joinPath(const std::string &path, std::string_view key)
{
    return path.empty() ? TomlWriter::key(key) : path + "." + TomlWriter::key(key);
}

void writeTable(std::string &out, const toml::table &table, const std::string &path,
                std::string_view orderPath, const TomlWriter::OrderFor &orderFor)
{
    const std::vector<std::string_view> keys = sortedKeys(table, orderFor(orderPath));
    for (const std::string_view key : keys) {
        const toml::node &node = *table.get(key);
        if (!isSection(node))
            out += TomlWriter::key(key) + " = " + TomlWriter::value(node) + "\n";
    }
    for (const std::string_view key : keys) {
        const toml::node &node = *table.get(key);
        if (!isSection(node))
            continue;
        const std::string childPath = joinPath(path, key);
        const std::string childOrder = orderPath.empty()
            ? std::string(key)
            : std::string(orderPath) + "." + std::string(key);
        if (const toml::table *child = node.as_table()) {
            out += "\n[" + childPath + "]\n";
            writeTable(out, *child, childPath, childOrder, orderFor);
            continue;
        }
        for (const toml::node &element : *node.as_array()) {
            out += "\n[[" + childPath + "]]\n";
            writeTable(out, *element.as_table(), childPath, childOrder, orderFor);
        }
    }
}

} // namespace

namespace TomlWriter {

std::string key(std::string_view key)
{
    const bool bare = !key.empty() && std::ranges::all_of(key, [](char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')
            || c == '_' || c == '-';
    });
    return bare ? std::string(key) : basicString(key);
}

std::string value(const toml::node &node)
{
    if (const toml::value<std::string> *text = node.as_string()) {
        const std::string &string = text->get();
        return string.find('\n') == std::string::npos ? basicString(string)
                                                      : multiLineString(string);
    }
    if (const toml::array *array = node.as_array()) {
        std::string out = "[";
        for (std::size_t i = 0; i < array->size(); ++i)
            out += (i > 0 ? ", " : "") + value(*array->get(i));
        return out + "]";
    }
    if (const toml::table *table = node.as_table()) {
        if (table->empty())
            return "{}";
        std::string out = "{ ";
        bool first = true;
        for (const auto &[name, child] : *table) {
            out += (first ? "" : ", ") + key(name.str()) + " = " + value(child);
            first = false;
        }
        return out + " }";
    }
    // Numbers, booleans, dates, and times: toml++ prints them correctly.
    std::ostringstream stream;
    node.visit([&stream](const auto &leaf) { stream << toml::toml_formatter { leaf }; });
    return stream.str();
}

std::string document(const toml::table &table, const OrderFor &orderFor)
{
    std::string out;
    writeTable(out, table, std::string(), std::string_view(), orderFor);
    // A document with no plain values starts with a section, not a blank line.
    if (out.starts_with('\n'))
        out.erase(0, 1);
    return out;
}

} // namespace TomlWriter
