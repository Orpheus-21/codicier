#include "MarkdownImport.h"

#include "Naming.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QSet>

#include <algorithm>

namespace {

// Natural order: runs of digits compare as numbers, so "2" comes before
// "10", and letters compare without case. QCollator does this only with
// the locale data of ICU, which some systems lack.
bool naturalLess(const QString &a, const QString &b)
{
    qsizetype i = 0;
    qsizetype j = 0;
    while (i < a.size() && j < b.size()) {
        if (a.at(i).isDigit() && b.at(j).isDigit()) {
            qsizetype endA = i;
            qsizetype endB = j;
            while (endA < a.size() && a.at(endA).isDigit())
                ++endA;
            while (endB < b.size() && b.at(endB).isDigit())
                ++endB;
            const qulonglong x = a.mid(i, endA - i).toULongLong();
            const qulonglong y = b.mid(j, endB - j).toULongLong();
            if (x != y)
                return x < y;
            i = endA;
            j = endB;
            continue;
        }
        const QChar x = a.at(i).toCaseFolded();
        const QChar y = b.at(j).toCaseFolded();
        if (x != y)
            return x < y;
        ++i;
        ++j;
    }
    return a.size() - i < b.size() - j;
}

// The entries of a folder in natural order, as a person reads them.
QFileInfoList sorted(const QDir &dir, QDir::Filters filters, const QStringList &names = { })
{
    QFileInfoList list = dir.entryInfoList(names, filters | QDir::NoDotAndDotDot);
    std::ranges::sort(list, [](const QFileInfo &a, const QFileInfo &b) {
        return naturalLess(a.fileName(), b.fileName());
    });
    return list;
}

// A scene from a Markdown file: a first "# " heading is its title.
MarkdownImport::Scene sceneOf(const QFileInfo &info)
{
    QFile file(info.absoluteFilePath());
    QString text = file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readAll()) : QString();
    text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    QString title = info.completeBaseName().replace(u'-', u' ').replace(u'_', u' ').trimmed();
    const QString start = text.trimmed();
    if (start.startsWith(QLatin1String("# "))) {
        const qsizetype end = start.indexOf(u'\n');
        title = start.mid(2, end < 0 ? -1 : end - 2).trimmed();
        text = end < 0 ? QString() : start.mid(end + 1).trimmed() + u'\n';
    }
    return { title, text };
}

QList<MarkdownImport::Scene> scenesIn(const QDir &dir)
{
    QList<MarkdownImport::Scene> scenes;
    for (const QFileInfo &info :
         sorted(dir, QDir::Files, { QStringLiteral("*.md"), QStringLiteral("*.markdown") }))
        scenes.append(sceneOf(info));
    return scenes;
}

} // namespace

namespace MarkdownImport {

QList<LoadError> build(const QString &target, const QString &title, const QList<Chapter> &chapters)
{
    QDir dir(target);
    if (dir.exists() && !dir.isEmpty(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot))
        return { { target, 0, QStringLiteral("the folder is not empty") } };
    if (!dir.mkpath(QStringLiteral("manuscript")))
        return { { target, 0, QStringLiteral("cannot make the folder") } };
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
    QSet<QString> ids;
    QSet<QString> files;
    const auto add = [&](const QString &kind, const QString &name, int parent) {
        ProjectItem item;
        item.id = Naming::newItemId(ids);
        ids.insert(item.id);
        item.kind = kind;
        item.title = name.trimmed().isEmpty() ? QStringLiteral("Untitled") : name.trimmed();
        item.parent = parent;
        item.section = parent < 0 ? QStringLiteral("manuscript") : QString();
        project.items.append(item);
        const int index = static_cast<int>(project.items.size() - 1);
        if (parent < 0)
            project.roots.append(index);
        else
            project.items[parent].children.append(index);
        return index;
    };
    for (const Chapter &chapter : chapters) {
        const int parent
            = chapter.title.isEmpty() ? -1 : add(QStringLiteral("chapter"), chapter.title, -1);
        for (const Scene &scene : chapter.scenes) {
            const int at = add(QStringLiteral("scene"), scene.title, parent);
            const QString name = Naming::fileNameFor(project.items.at(at).title, files);
            files.insert(name);
            project.items[at].file = QStringLiteral("manuscript/") + name;
            const QByteArray bytes = scene.text.toUtf8();
            QSaveFile file(dir.filePath(project.items.at(at).file));
            if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size()
                || !file.commit())
                return { { project.items.at(at).file, 0,
                           QStringLiteral("cannot write the file") } };
        }
    }
    return ProjectStore::save(project);
}

QList<LoadError> importFolder(const QString &source, const QString &target, const QString &title)
{
    const QDir from(source);
    if (!from.exists())
        return { { source, 0, QStringLiteral("the folder does not exist") } };
    QList<Chapter> chapters { { QString(), scenesIn(from) } };
    qsizetype scenes = chapters.first().scenes.size();
    for (const QFileInfo &folder : sorted(from, QDir::Dirs)) {
        Chapter chapter { folder.fileName(), scenesIn(QDir(folder.absoluteFilePath())) };
        scenes += chapter.scenes.size();
        if (!chapter.scenes.isEmpty())
            chapters.append(chapter);
    }
    if (scenes == 0)
        return { { source, 0, QStringLiteral("the folder has no Markdown files (.md)") } };
    return build(target, title, chapters);
}

} // namespace MarkdownImport
