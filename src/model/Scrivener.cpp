#include "Scrivener.h"

#include <QDir>
#include <QFile>
#include <QXmlStreamReader>

namespace {

// Reads one BinderItem, the reader at its start element.
Scrivener::Item readItem(QXmlStreamReader &xml)
{
    Scrivener::Item item;
    item.uuid = xml.attributes().value(QLatin1String("UUID")).toString();
    item.type = xml.attributes().value(QLatin1String("Type")).toString();
    while (xml.readNextStartElement()) {
        const QStringView name = xml.name();
        if (name == QLatin1String("Title")) {
            item.title = xml.readElementText();
        } else if (name == QLatin1String("MetaData")) {
            while (xml.readNextStartElement()) {
                const QStringView key = xml.name();
                const QString value = xml.readElementText().trimmed();
                if (key == QLatin1String("IncludeInCompile"))
                    item.include = value.compare(QLatin1String("No"), Qt::CaseInsensitive) != 0;
                else if (key == QLatin1String("LabelID"))
                    item.label = value.toInt();
                else if (key == QLatin1String("StatusID"))
                    item.status = value.toInt();
                else if (key == QLatin1String("FileExtension"))
                    item.extension = value;
            }
        } else if (name == QLatin1String("Children")) {
            while (xml.readNextStartElement()) {
                if (xml.name() == QLatin1String("BinderItem"))
                    item.children.append(readItem(xml));
                else
                    xml.skipCurrentElement();
            }
        } else {
            xml.skipCurrentElement();
        }
    }
    return item;
}

// Reads a list of labels or statuses: every element under it with an ID.
void readList(QXmlStreamReader &xml, Scrivener::Binder &binder, bool labels)
{
    while (xml.readNextStartElement()) {
        const auto attributes = xml.attributes();
        if (!attributes.hasAttribute(QLatin1String("ID"))) {
            readList(xml, binder, labels);
            continue;
        }
        const int id = attributes.value(QLatin1String("ID")).toInt();
        const QStringList rgb = attributes.value(QLatin1String("Color")).toString().split(u' ');
        const QString name = xml.readElementText().trimmed();
        if (!labels) {
            binder.statuses.insert(id, name);
            continue;
        }
        Scrivener::Label label { name };
        if (rgb.size() >= 3) {
            label.red = rgb.at(0).toDouble();
            label.green = rgb.at(1).toDouble();
            label.blue = rgb.at(2).toDouble();
        }
        binder.labels.insert(id, label);
    }
}

} // namespace

namespace Scrivener {

std::optional<Binder> readBinder(const QString &folder, QString *error)
{
    const auto fail = [error](const QString &text) {
        if (error != nullptr)
            *error = text;
        return std::nullopt;
    };
    const QStringList found = QDir(folder).entryList({ QStringLiteral("*.scrivx") }, QDir::Files);
    if (found.isEmpty())
        return fail(
            QStringLiteral("This folder is not a Scrivener project: it has no .scrivx file."));
    QFile file(QDir(folder).filePath(found.first()));
    if (!file.open(QIODevice::ReadOnly))
        return fail(QStringLiteral("Cannot read %1: %2").arg(found.first(), file.errorString()));
    QXmlStreamReader xml(&file);
    Binder binder;
    if (!xml.readNextStartElement() || xml.name() != QLatin1String("ScrivenerProject"))
        return fail(QStringLiteral("%1 is not a Scrivener binder.").arg(found.first()));
    binder.author = xml.attributes().value(QLatin1String("Author")).toString();
    while (xml.readNextStartElement()) {
        const QStringView name = xml.name();
        if (name == QLatin1String("Binder")) {
            while (xml.readNextStartElement()) {
                if (xml.name() == QLatin1String("BinderItem"))
                    binder.roots.append(readItem(xml));
                else
                    xml.skipCurrentElement();
            }
        } else if (name == QLatin1String("LabelSettings")
                   || name == QLatin1String("StatusSettings")) {
            readList(xml, binder, name == QLatin1String("LabelSettings"));
        } else {
            xml.skipCurrentElement();
        }
    }
    if (xml.hasError())
        return fail(QStringLiteral("%1 is broken at line %2: %3")
                        .arg(found.first())
                        .arg(xml.lineNumber())
                        .arg(xml.errorString()));
    return binder;
}

} // namespace Scrivener
