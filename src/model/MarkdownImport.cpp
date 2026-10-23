#include "MarkdownImport.h"

#include "Naming.h"

#include <QCollator>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QSet>

#include <algorithm>

namespace {

// The entries of a folder in natural order, as a person reads them.
QFileInfoList sorted(const QDir &dir, QDir::Filters filters, const QStringList &names = { })
{
    QFileInfoList list = dir.entryInfoList(names, filters | QDir::NoDotAndDotDot);
    QCollator collator;
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    std::ranges::sort(list, [&collator](const QFileInfo &a, const QFileInfo &b) {
        return collator.compare(a.fileName(), b.fileName()) < 0;
    });
    return list;
}

class Builder
{
public:
    explicit Builder(Project &project) : project(project) { }

    QList<std::pair<QString, QByteArray>> texts;

    // Adds the .md files of dir below parent (-1 for the top), and returns
    // how many it added.
    int addFiles(const QDir &dir, int parent)
    {
        const QFileInfoList files
            = sorted(dir, QDir::Files, { QStringLiteral("*.md"), QStringLiteral("*.markdown") });
        for (const QFileInfo &info : files)
            addScene(info, parent);
        return static_cast<int>(files.size());
    }

    int addChapter(const QString &name) { return add(QStringLiteral("chapter"), name, -1); }

    // Takes back a chapter with no scenes, the last item added.
    void dropLast()
    {
        project.roots.removeLast();
        project.items.removeLast();
    }

private:
    Project &project;
    QSet<QString> ids;
    QSet<QString> files;

    int add(const QString &kind, const QString &title, int parent)
    {
        ProjectItem item;
        item.id = Naming::newItemId(ids);
        ids.insert(item.id);
        item.kind = kind;
        item.title = title;
        item.parent = parent;
        item.section = parent < 0 ? QStringLiteral("manuscript") : QString();
        project.items.append(item);
        const int index = static_cast<int>(project.items.size() - 1);
        if (parent < 0)
            project.roots.append(index);
        else
            project.items[parent].children.append(index);
        return index;
    }

    void addScene(const QFileInfo &info, int parent)
    {
        QFile file(info.absoluteFilePath());
        QString text
            = file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readAll()) : QString();
        text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
        // A first "# " heading is the title of the scene.
        QString title = info.completeBaseName().replace(u'-', u' ').replace(u'_', u' ').trimmed();
        const QString start = text.trimmed();
        if (start.startsWith(QLatin1String("# "))) {
            const qsizetype end = start.indexOf(u'\n');
            title = start.mid(2, end < 0 ? -1 : end - 2).trimmed();
            text = end < 0 ? QString() : start.mid(end + 1).trimmed() + u'\n';
        }
        const int at = add(QStringLiteral("scene"),
                           title.isEmpty() ? QStringLiteral("Untitled") : title, parent);
        const QString name = Naming::fileNameFor(project.items.at(at).title, files);
        files.insert(name);
        project.items[at].file = QStringLiteral("manuscript/") + name;
        texts.append({ project.items.at(at).file, text.toUtf8() });
    }
};

} // namespace

namespace MarkdownImport {

QList<LoadError> importFolder(const QString &source, const QString &target, const QString &title)
{
    const QDir from(source);
    if (!from.exists())
        return { { source, 0, QStringLiteral("the folder does not exist") } };
    QDir dir(target);
    if (dir.exists() && !dir.isEmpty(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot))
        return { { target, 0, QStringLiteral("the folder is not empty") } };

    Project project;
    project.folder = target;
    project.title = title;
    project.kind = QStringLiteral("novel");
    const QDateTime now = QDateTime::currentDateTimeUtc();
    project.data.insert(
        "created",
        toml::date_time {
            toml::date { now.date().year(), now.date().month(), now.date().day() },
            toml::time { now.time().hour(), now.time().minute(), now.time().second() },
            toml::time_offset { } });
    Builder builder(project);
    int scenes = builder.addFiles(from, -1);
    for (const QFileInfo &folder : sorted(from, QDir::Dirs)) {
        const int chapter = builder.addChapter(folder.fileName());
        const int added = builder.addFiles(QDir(folder.absoluteFilePath()), chapter);
        if (added == 0)
            builder.dropLast();
        scenes += added;
    }
    if (scenes == 0)
        return { { source, 0, QStringLiteral("the folder has no Markdown files (.md)") } };
    if (!dir.mkpath(QStringLiteral("manuscript")))
        return { { target, 0, QStringLiteral("cannot make the folder") } };
    for (const auto &[path, bytes] : std::as_const(builder.texts)) {
        QSaveFile file(dir.filePath(path));
        if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit())
            return { { path, 0, QStringLiteral("cannot write the file") } };
    }
    return ProjectStore::save(project);
}

} // namespace MarkdownImport
