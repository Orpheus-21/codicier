#include "History.h"

#include <QDir>
#include <QFile>
#include <QSaveFile>

#include <toml++/toml.hpp>

#include <utility>

namespace {

QString pathOf(const QString &folder)
{
    return QDir(folder).filePath(QStringLiteral("history.toml"));
}

} // namespace

History::History(QString projectFolder) : folder(std::move(projectFolder))
{
    QFile file(pathOf(folder));
    if (!file.open(QIODevice::ReadOnly))
        return;
    const QByteArray bytes = file.readAll();
    try {
        const toml::table table = toml::parse(std::string_view(bytes.constData(), bytes.size()));
        if (const toml::table *list = table.get_as<toml::table>("days")) {
            for (const auto &[key, value] : *list) {
                const QDate day = QDate::fromString(
                    QString::fromUtf8(key.str().data(), static_cast<qsizetype>(key.str().size())),
                    Qt::ISODate);
                if (day.isValid() && value.is_integer())
                    days.insert(day, static_cast<int>(value.value_or(std::int64_t { 0 })));
            }
        }
    } catch (const toml::parse_error &) {
        // A file that does not parse is an empty history.
        days.clear();
    }
}

void History::add(const QDate &day, int words)
{
    if (words == 0 || !day.isValid())
        return;
    days[day] += words;
    changed = true;
}

int History::streak(const QDate &today) const
{
    QDate day = days.value(today) > 0 ? today : today.addDays(-1);
    int count = 0;
    while (days.value(day) > 0) {
        ++count;
        day = day.addDays(-1);
    }
    return count;
}

bool History::save()
{
    if (!changed)
        return true;
    // One line for each day, in date order, so a diff shows one line a day.
    QByteArray out = "[days]\n";
    for (auto it = days.cbegin(); it != days.cend(); ++it)
        out += it.key().toString(Qt::ISODate).toUtf8() + " = " + QByteArray::number(it.value())
            + '\n';
    QSaveFile file(pathOf(folder));
    if (!file.open(QIODevice::WriteOnly) || file.write(out) != out.size() || !file.commit())
        return false;
    changed = false;
    return true;
}
