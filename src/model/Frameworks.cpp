#include "Frameworks.h"

#include <QFile>

#include <toml++/toml.hpp>

namespace {

QString text(const toml::table &table, const char *key)
{
    return QString::fromStdString(table[key].value_or(std::string()));
}

QList<Frameworks::Framework> load()
{
    QList<Frameworks::Framework> list;
    QFile file(QStringLiteral(":/data/frameworks.toml"));
    if (!file.open(QIODevice::ReadOnly))
        return list;
    const QByteArray bytes = file.readAll();
    toml::table table;
    try {
        table = toml::parse(std::string_view(bytes.constData(), bytes.size()));
    } catch (const toml::parse_error &) {
        // The file comes with the app, and a test checks that it parses.
        return list;
    }
    const toml::array *frameworks = table.get_as<toml::array>("framework");
    if (frameworks == nullptr)
        return list;
    for (const toml::node &node : *frameworks) {
        const toml::table *entry = node.as_table();
        if (entry == nullptr)
            continue;
        Frameworks::Framework framework { text(*entry, "id"), text(*entry, "name"), { } };
        if (const toml::array *beats = entry->get_as<toml::array>("beat")) {
            for (const toml::node &beat : *beats) {
                if (const toml::table *each = beat.as_table())
                    framework.beats.append({ text(*each, "id"), text(*each, "name"),
                                             text(*each, "description"),
                                             (*each)["at"].value_or(0.0) });
            }
        }
        list.append(framework);
    }
    return list;
}

} // namespace

namespace Frameworks {

const QList<Framework> &all()
{
    static const QList<Framework> frameworks = load();
    return frameworks;
}

} // namespace Frameworks
