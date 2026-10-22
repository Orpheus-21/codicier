#include "Canvas.h"

#include "TomlWriter.h"

#include <QSaveFile>
#include <QVariantList>

#include <array>

namespace {

constexpr std::array<std::string_view, 1> topOrder { "format" };
constexpr std::array<std::string_view, 5> nodeOrder { "id", "x", "y", "item", "text" };
constexpr std::array<std::string_view, 2> edgeOrder { "from", "to" };

QString text(const toml::table &table, std::string_view key)
{
    return QString::fromStdString(table[key].value_or(std::string()));
}

} // namespace

namespace Canvas {

QVariantMap read(const QString &path)
{
    QVariantList nodes;
    QVariantList edges;
    try {
        const toml::table file = toml::parse_file(path.toStdString());
        if (const toml::array *list = file["node"].as_array()) {
            for (const toml::node &each : *list) {
                if (const toml::table *node = each.as_table())
                    nodes.append(QVariantMap {
                        { QStringLiteral("id"), text(*node, "id") },
                        { QStringLiteral("x"),
                          static_cast<int>((*node)["x"].value_or(std::int64_t { 0 })) },
                        { QStringLiteral("y"),
                          static_cast<int>((*node)["y"].value_or(std::int64_t { 0 })) },
                        { QStringLiteral("text"), text(*node, "text") },
                        { QStringLiteral("item"), text(*node, "item") } });
            }
        }
        if (const toml::array *list = file["edge"].as_array()) {
            for (const toml::node &each : *list) {
                if (const toml::table *edge = each.as_table())
                    edges.append(QVariantMap { { QStringLiteral("from"), text(*edge, "from") },
                                               { QStringLiteral("to"), text(*edge, "to") } });
            }
        }
    } catch (const toml::parse_error &) {
        // A broken file shows as an empty canvas, and the next save writes it anew.
        return { { QStringLiteral("nodes"), QVariantList() },
                 { QStringLiteral("edges"), QVariantList() } };
    }
    return { { QStringLiteral("nodes"), nodes }, { QStringLiteral("edges"), edges } };
}

QString write(const QString &path, const QVariantMap &canvas)
{
    toml::table file { { "format", 1 } };
    toml::array nodes;
    for (const QVariant &each : canvas.value(QStringLiteral("nodes")).toList()) {
        const QVariantMap node = each.toMap();
        toml::table table { { "id", node.value(QStringLiteral("id")).toString().toStdString() },
                            { "x", node.value(QStringLiteral("x")).toInt() },
                            { "y", node.value(QStringLiteral("y")).toInt() } };
        const QString item = node.value(QStringLiteral("item")).toString();
        if (!item.isEmpty())
            table.insert("item", item.toStdString());
        else
            table.insert("text", node.value(QStringLiteral("text")).toString().toStdString());
        nodes.push_back(std::move(table));
    }
    toml::array edges;
    for (const QVariant &each : canvas.value(QStringLiteral("edges")).toList()) {
        const QVariantMap edge = each.toMap();
        edges.push_back(
            toml::table { { "from", edge.value(QStringLiteral("from")).toString().toStdString() },
                          { "to", edge.value(QStringLiteral("to")).toString().toStdString() } });
    }
    if (!nodes.empty())
        file.insert("node", std::move(nodes));
    if (!edges.empty())
        file.insert("edge", std::move(edges));
    const std::string bytes
        = TomlWriter::document(file, [](std::string_view at) -> TomlWriter::Order {
              if (at == "node")
                  return nodeOrder;
              if (at == "edge")
                  return edgeOrder;
              return topOrder;
          });
    QSaveFile out(path);
    if (!out.open(QIODevice::WriteOnly))
        return out.errorString();
    out.write(bytes.data(), static_cast<qint64>(bytes.size()));
    return out.commit() ? QString() : out.errorString();
}

} // namespace Canvas
