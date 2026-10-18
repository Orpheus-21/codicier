#include "ProjectList.h"

#include "Index.h"
#include "Naming.h"
#include "ProjectStore.h"
#include "Templates.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>

#include <algorithm>

namespace {

QString readText(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readAll()) : QString();
}

// The last line of prose, as plain text: no footnote lines, and no marks for
// headings, emphasis, or footnotes.
QString lastLineOf(const QString &text)
{
    const QStringList lines = text.split(u'\n', Qt::SkipEmptyParts);
    for (auto it = lines.crbegin(); it != lines.crend(); ++it) {
        QString line = it->trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1String("[^")))
            continue;
        line.remove(QRegularExpression(QStringLiteral("\\[\\^[^\\]]*\\]")));
        line.remove(QRegularExpression(QStringLiteral("^#+\\s*")));
        line.remove(u'*');
        line.remove(QRegularExpression(QStringLiteral("(?<!\\w)_|_(?!\\w)")));
        return line.trimmed();
    }
    return { };
}

} // namespace

ProjectList::ProjectList(QObject *parent) : QAbstractListModel(parent)
{
    connect(&watcher, &QFileSystemWatcher::directoryChanged, this, &ProjectList::refresh);
}

void ProjectList::setFolder(const QString &folder)
{
    if (folder == workspace)
        return;
    if (!watcher.directories().isEmpty())
        watcher.removePaths(watcher.directories());
    workspace = folder;
    if (!workspace.isEmpty() && QFileInfo(workspace).isDir())
        watcher.addPath(workspace);
    emit folderChanged();
    refresh();
}

int ProjectList::totalWords() const
{
    int total = 0;
    for (const Entry &entry : entries)
        total += entry.words;
    return total;
}

int ProjectList::fictionCount() const
{
    return static_cast<int>(std::ranges::count_if(entries, [](const Entry &entry) {
        return entry.error.isEmpty() && isFiction(entry.kind);
    }));
}

int ProjectList::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(entries.size());
}

QVariant ProjectList::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= entries.size())
        return { };
    const Entry &entry = entries.at(index.row());
    switch (role) {
    case TitleRole:
        return entry.title;
    case KindRole:
        return entry.kind;
    case KindLabelRole:
        return kindLabel(entry.kind);
    case FictionRole:
        return isFiction(entry.kind);
    case WordsRole:
        return entry.words;
    case GoalRole:
        return entry.goal;
    case EditedRole:
        return entry.edited;
    case EditedLabelRole:
        return editedLabel(entry.edited, QDateTime::currentDateTime());
    case LastLineRole:
        return entry.lastLine;
    case FolderRole:
        return entry.folder;
    case ErrorRole:
        return entry.error;
    default:
        return { };
    }
}

QHash<int, QByteArray> ProjectList::roleNames() const
{
    return {
        { TitleRole, "title" },         { KindRole, "kind" },
        { KindLabelRole, "kindLabel" }, { FictionRole, "fiction" },
        { WordsRole, "words" },         { GoalRole, "goal" },
        { EditedRole, "edited" },       { EditedLabelRole, "editedLabel" },
        { LastLineRole, "lastLine" },   { FolderRole, "folder" },
        { ErrorRole, "error" },
    };
}

QVariantMap ProjectList::get(int row) const
{
    QVariantMap map;
    if (row < 0 || row >= entries.size())
        return map;
    const QHash<int, QByteArray> roles = roleNames();
    for (auto it = roles.cbegin(); it != roles.cend(); ++it)
        map.insert(QString::fromUtf8(it.value()), data(index(row), it.key()));
    return map;
}

ProjectList::Entry ProjectList::read(const QString &projectFolder)
{
    Entry entry;
    entry.folder = projectFolder;
    entry.edited
        = QFileInfo(QDir(projectFolder).filePath(QStringLiteral("binder.toml"))).lastModified();
    const LoadResult result = ProjectStore::load(projectFolder);
    if (!result.ok()) {
        entry.title = QFileInfo(projectFolder).fileName();
        entry.error = result.errors.first().text();
        return entry;
    }
    const Project &project = result.project;
    entry.title = project.title;
    entry.kind = project.kind;
    entry.goal = static_cast<int>(project.data["goals"]["words"].value_or(std::int64_t { 0 }));
    // ponytail: reads every text file of every project; read the word counts
    // from the search index when a workspace gets large.
    const QDir dir(projectFolder);
    QDateTime newestText;
    for (const ProjectItem &item : project.items) {
        if (item.file.isEmpty() || item.kind == QLatin1String("file")
            || item.section == QLatin1String("trash"))
            continue;
        const QString path = dir.filePath(item.file);
        const QString text = readText(path);
        entry.words += Index::countWords(text);
        const QDateTime modified = QFileInfo(path).lastModified();
        if (!text.trimmed().isEmpty() && (!newestText.isValid() || modified > newestText)) {
            newestText = modified;
            entry.lastLine = lastLineOf(text);
        }
        entry.edited = std::max(entry.edited, modified);
    }
    return entry;
}

void ProjectList::refresh()
{
    QList<Entry> found;
    const QDir dir(workspace);
    if (!workspace.isEmpty()) {
        for (const QFileInfo &info : dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            if (QFileInfo::exists(QDir(info.filePath()).filePath(QStringLiteral("project.toml"))))
                found.append(read(info.filePath()));
        }
    }
    std::ranges::sort(found, [](const Entry &a, const Entry &b) { return a.edited > b.edited; });
    beginResetModel();
    entries = found;
    endResetModel();
    emit refreshed();
}

QString ProjectList::create(const QString &title, const QString &kind, const QString &author)
{
    const QString name = title.trimmed();
    if (name.isEmpty())
        return QStringLiteral("The project needs a title.");
    if (workspace.isEmpty())
        return QStringLiteral("Choose a workspace first.");
    const QDir dir(workspace);
    QSet<QString> taken;
    for (const QString &entry : dir.entryList(QDir::AllEntries | QDir::NoDotAndDotDot))
        taken.insert(entry + QStringLiteral(".md"));
    // The file name rule gives a readable folder name: drop its ".md".
    QString folderName = Naming::fileNameFor(name, taken);
    folderName.chop(3);
    const QList<LoadError> errors
        = Templates::create(dir.filePath(folderName), kind, name, author.trimmed());
    if (!errors.isEmpty())
        return errors.first().text();
    refresh();
    return { };
}

QString ProjectList::kindLabel(const QString &kind)
{
    static const QHash<QString, QString> labels {
        { "short-story", "Short story" },
        { "novella", "Novella" },
        { "novel", "Novel" },
        { "series", "Series" },
        { "essay", "Essay" },
        { "essay-collection", "Essay collection" },
        { "non-fiction", "Non-fiction book" },
    };
    return labels.value(kind);
}

bool ProjectList::isFiction(const QString &kind)
{
    return kind == QLatin1String("short-story") || kind == QLatin1String("novella")
        || kind == QLatin1String("novel") || kind == QLatin1String("series");
}

QString ProjectList::editedLabel(const QDateTime &edited, const QDateTime &now)
{
    if (!edited.isValid())
        return { };
    const qint64 days = edited.date().daysTo(now.date());
    if (days <= 0)
        return QStringLiteral("Today");
    if (days == 1)
        return QStringLiteral("Yesterday");
    if (days < 7)
        return QStringLiteral("%1 days ago").arg(days);
    if (days < 14)
        return QStringLiteral("1 week ago");
    if (days < 35)
        return QStringLiteral("%1 weeks ago").arg(days / 7);
    return QLocale(QLocale::English).toString(edited.date(), QStringLiteral("MMMM yyyy"));
}
