#include "Scrivener.h"

#include "Naming.h"
#include "Rtf.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QSet>
#include <QXmlStreamReader>

#include <array>
#include <cmath>

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

namespace {

// The label colours of Codicier, as in Theme.qml, to find the nearest one.
struct Colour
{
    const char *name;
    double red;
    double green;
    double blue;
};
constexpr std::array<Colour, 5> colours { { { "blue", 0x2a / 255.0, 0x78 / 255.0, 0xd6 / 255.0 },
                                            { "orange", 0xeb / 255.0, 0x68 / 255.0, 0x34 / 255.0 },
                                            { "green", 0x1b / 255.0, 0xaf / 255.0, 0x7a / 255.0 },
                                            { "yellow", 0xed / 255.0, 0xa1 / 255.0, 0x00 / 255.0 },
                                            { "pink", 0xe8 / 255.0, 0x7b / 255.0,
                                              0xa4 / 255.0 } } };

QString nearestColour(const Scrivener::Label &label)
{
    if (label.red < 0)
        return { };
    const Colour *best = nullptr;
    double distance = 0;
    for (const Colour &colour : colours) {
        const double d = std::pow(colour.red - label.red, 2)
            + std::pow(colour.green - label.green, 2) + std::pow(colour.blue - label.blue, 2);
        if (best == nullptr || d < distance) {
            best = &colour;
            distance = d;
        }
    }
    return QString::fromLatin1(best->name);
}

// A status of Scrivener by its name: final, revised, or draft.
QString statusFor(const QString &name)
{
    const QString lower = name.toLower();
    if (lower.contains(QLatin1String("final")) || lower.contains(QLatin1String("done")))
        return QStringLiteral("final");
    if (lower.contains(QLatin1String("revised")) || lower.contains(QLatin1String("second")))
        return QStringLiteral("revised");
    return QStringLiteral("draft");
}

class Importer
{
public:
    Importer(const QString &scriv, const Scrivener::Binder &binder, Project &project,
             Scrivener::Skipped &skipped)
        : data(QDir(scriv).filePath(QStringLiteral("Files/Data"))), binder(binder),
          project(project), skipped(skipped)
    { }

    // The files to write: the path in the project and its bytes, or the
    // path of a media file to copy.
    QList<std::pair<QString, QByteArray>> texts;
    QList<std::pair<QString, QString>> copies;

    void manuscript(const Scrivener::Item &draft)
    {
        for (const Scrivener::Item &item : draft.children)
            draftItem(item, -1);
    }

    // Puts a top folder into the notes: folders stay folders, texts become
    // notes, and media files go to the research section.
    void notes(const Scrivener::Item &folder)
    {
        const int at = add(QStringLiteral("folder"), folder, -1, QStringLiteral("notes"));
        for (const Scrivener::Item &item : folder.children)
            noteItem(item, at);
    }

    void trash(const Scrivener::Item &item)
    {
        for (const Scrivener::Item &child : item.children) {
            ++skipped.trash;
            trash(child);
        }
    }

private:
    QString data;
    const Scrivener::Binder &binder;
    Project &project;
    Scrivener::Skipped &skipped;
    QSet<QString> ids;
    QSet<QString> files;

    QString folderOf(const Scrivener::Item &item) const { return QDir(data).filePath(item.uuid); }

    QString markdownOf(const Scrivener::Item &item, const char *name) const
    {
        QFile file(QDir(folderOf(item)).filePath(QString::fromLatin1(name)));
        return file.open(QIODevice::ReadOnly) ? Rtf::toMarkdown(file.readAll()) : QString();
    }

    static bool isContainer(const Scrivener::Item &item)
    {
        return !item.children.isEmpty() || item.type.endsWith(QLatin1String("Folder"));
    }

    static bool isText(const Scrivener::Item &item)
    {
        return item.type == QLatin1String("Text") || isContainer(item);
    }

    // Adds an item of kind with the data of item, below parent or at the top
    // of section, and returns its index.
    int add(const QString &kind, const Scrivener::Item &item, int parent, const QString &section)
    {
        ProjectItem made;
        made.id = Naming::newItemId(ids);
        ids.insert(made.id);
        made.kind = kind;
        made.title = item.title.isEmpty() ? QStringLiteral("Untitled") : item.title;
        made.parent = parent;
        made.section = parent < 0 ? section : QString();
        // The scan of the snapshots and the linked comments, for the report.
        const QDir folder(folderOf(item));
        skipped.snapshots += static_cast<int>(
            QDir(folder.filePath(QStringLiteral("snapshots"))).entryList(QDir::Files).size());
        if (folder.exists(QStringLiteral("content.comments")))
            ++skipped.comments;
        QFile synopsis(folder.filePath(QStringLiteral("synopsis.txt")));
        if (synopsis.open(QIODevice::ReadOnly)) {
            const QString text = QString::fromUtf8(synopsis.readAll()).trimmed();
            if (!text.isEmpty())
                made.data.insert("synopsis", text.toStdString());
        }
        if (const QString notes = markdownOf(item, "notes.rtf"); !notes.isEmpty())
            made.data.insert("notes", notes.toStdString());
        QStringList tags;
        if (item.label >= 0 && binder.labels.contains(item.label)) {
            const Scrivener::Label label = binder.labels.value(item.label);
            if (const QString colour = nearestColour(label); !colour.isEmpty())
                made.data.insert("label", colour.toStdString());
            tags.append(QStringLiteral("label: ") + label.name);
        }
        if (item.status >= 0 && binder.statuses.contains(item.status)) {
            const QString name = binder.statuses.value(item.status);
            if (kind == QLatin1String("scene") && statusFor(name) != QLatin1String("draft"))
                made.data.insert("status", statusFor(name).toStdString());
            tags.append(QStringLiteral("status: ") + name);
        }
        if (!tags.isEmpty()) {
            toml::array list;
            for (const QString &tag : std::as_const(tags))
                list.push_back(tag.toStdString());
            made.data.insert("tags", std::move(list));
        }
        if (!item.include && kind == QLatin1String("scene"))
            made.data.insert("include", false);
        project.items.append(made);
        const int index = static_cast<int>(project.items.size() - 1);
        if (parent < 0)
            project.roots.append(index);
        else
            project.items[parent].children.append(index);
        return index;
    }

    // Gives the item a Markdown file in folder with text.
    void giveFile(int index, const QString &folder, const QString &text)
    {
        const QString name = Naming::fileNameFor(project.items.at(index).title, files);
        files.insert(name);
        project.items[index].file = folder + name;
        texts.append({ project.items.at(index).file, text.toUtf8() });
    }

    void draftItem(const Scrivener::Item &item, int parent)
    {
        if (!isText(item)) {
            // Scrivener keeps media out of the Draft; one that is there stays out.
            ++skipped.media;
            return;
        }
        if (!isContainer(item)) {
            const int at = add(QStringLiteral("scene"), item, parent, QStringLiteral("manuscript"));
            giveFile(at, QStringLiteral("manuscript/"), markdownOf(item, "content.rtf"));
            return;
        }
        const bool deep = std::any_of(item.children.cbegin(), item.children.cend(), isContainer);
        const int at = add(deep ? QStringLiteral("part") : QStringLiteral("chapter"), item, parent,
                           QStringLiteral("manuscript"));
        // The own text of a text with children becomes its first scene.
        if (const QString text = markdownOf(item, "content.rtf"); !text.isEmpty()) {
            Scrivener::Item own;
            own.title = item.title;
            own.uuid = QStringLiteral("-");
            const int scene = add(QStringLiteral("scene"), own, at, QString());
            giveFile(scene, QStringLiteral("manuscript/"), text);
        }
        for (const Scrivener::Item &child : item.children)
            draftItem(child, at);
    }

    void noteItem(const Scrivener::Item &item, int parent)
    {
        if (!isText(item)) {
            const QString extension
                = item.extension.isEmpty() ? QStringLiteral("pdf") : item.extension;
            const QString source
                = QDir(folderOf(item)).filePath(QStringLiteral("content.") + extension);
            if (!QFileInfo::exists(source)) {
                ++skipped.media;
                return;
            }
            const int at = add(QStringLiteral("file"), item, -1, QStringLiteral("research"));
            QString name = Naming::fileNameFor(project.items.at(at).title, files);
            files.insert(name);
            name.chop(3);
            project.items[at].file = QStringLiteral("research/") + name + u'.' + extension;
            copies.append({ project.items.at(at).file, source });
            return;
        }
        if (!isContainer(item)) {
            const int at = add(QStringLiteral("note"), item, parent, QStringLiteral("notes"));
            giveFile(at, QStringLiteral("notes/"), markdownOf(item, "content.rtf"));
            return;
        }
        const int at = add(QStringLiteral("folder"), item, parent, QStringLiteral("notes"));
        if (const QString text = markdownOf(item, "content.rtf"); !text.isEmpty()) {
            Scrivener::Item own;
            own.title = item.title;
            own.uuid = QStringLiteral("-");
            giveFile(add(QStringLiteral("note"), own, at, QString()), QStringLiteral("notes/"),
                     text);
        }
        for (const Scrivener::Item &child : item.children)
            noteItem(child, at);
    }
};

} // namespace

namespace Scrivener {

QList<LoadError> importProject(const QString &scriv, const QString &target, const QString &title,
                               Skipped *skipped)
{
    QString error;
    const std::optional<Binder> binder = readBinder(scriv, &error);
    if (!binder)
        return { { scriv, 0, error } };
    QDir dir(target);
    if (dir.exists() && !dir.isEmpty(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot))
        return { { target, 0, QStringLiteral("the folder is not empty") } };
    if (!dir.mkpath(QStringLiteral(".")))
        return { { target, 0, QStringLiteral("cannot make the folder") } };

    Project project;
    project.folder = target;
    project.title = title;
    project.kind = QStringLiteral("novel");
    if (!binder->author.isEmpty())
        project.data.insert("author", binder->author.toStdString());
    const QDateTime now = QDateTime::currentDateTimeUtc();
    project.data.insert(
        "created",
        toml::date_time {
            toml::date { now.date().year(), now.date().month(), now.date().day() },
            toml::time { now.time().hour(), now.time().minute(), now.time().second() },
            toml::time_offset { } });
    Skipped counts;
    Importer importer(scriv, *binder, project, counts);
    for (const Item &root : binder->roots) {
        if (root.type == QLatin1String("DraftFolder"))
            importer.manuscript(root);
    }
    for (const Item &root : binder->roots) {
        if (root.type == QLatin1String("TrashFolder"))
            importer.trash(root);
        else if (root.type != QLatin1String("DraftFolder"))
            importer.notes(root);
    }
    for (const auto &[path, bytes] : std::as_const(importer.texts)) {
        QSaveFile file(dir.filePath(path));
        if (!dir.mkpath(path.section(u'/', 0, 0)) || !file.open(QIODevice::WriteOnly)
            || file.write(bytes) != bytes.size() || !file.commit())
            return { { path, 0, QStringLiteral("cannot write the file") } };
    }
    for (const auto &[path, source] : std::as_const(importer.copies)) {
        if (!dir.mkpath(path.section(u'/', 0, 0)) || !QFile::copy(source, dir.filePath(path)))
            return { { path, 0, QStringLiteral("cannot copy the file") } };
    }
    if (skipped != nullptr)
        *skipped = counts;
    return ProjectStore::save(project);
}

} // namespace Scrivener
