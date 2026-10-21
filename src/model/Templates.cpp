#include "Templates.h"

#include "Naming.h"

#include <QDateTime>
#include <QDir>
#include <QSaveFile>
#include <QSet>

namespace {

// Builds the items of a new project and keeps the ids and file names unique.
class Builder
{
public:
    explicit Builder(Project &project) : project(project) { }

    // Adds an item at the top level of a section.
    int top(const QString &kind, const QString &section, const QString &title)
    {
        const int index = add(kind, title);
        project.items[index].section = section;
        project.roots.append(index);
        return index;
    }

    // Adds an item below parent.
    int below(int parent, const QString &kind, const QString &title)
    {
        const int index = add(kind, title);
        project.items[index].parent = parent;
        project.items[index].section = project.items.at(parent).section;
        project.items[parent].children.append(index);
        return index;
    }

private:
    Project &project;
    QSet<QString> ids;
    QSet<QString> files;

    int add(const QString &kind, const QString &title)
    {
        ProjectItem item;
        item.id = Naming::newItemId(ids);
        ids.insert(item.id);
        item.kind = kind;
        item.title = title;
        const QString folder = kind == QLatin1String("scene") ? QStringLiteral("manuscript/")
            : kind == QLatin1String("page")                   ? QStringLiteral("matter/")
            : kind == QLatin1String("note")                   ? QStringLiteral("notes/")
                                                              : QString();
        if (!folder.isEmpty()) {
            item.file = folder + Naming::fileNameFor(title, files);
            files.insert(item.file.mid(folder.size()));
        }
        project.items.append(item);
        return static_cast<int>(project.items.size() - 1);
    }
};

// The front matter that a book has.
void titlePage(Builder &b)
{
    b.top(QStringLiteral("page"), QStringLiteral("front"), QStringLiteral("Title page"));
}

// Fills the binder for the kind. Returns the word goal, or 0 for none.
int fill(Builder &b, const QString &kind)
{
    const QString manuscript = QStringLiteral("manuscript");
    const QString scene = QStringLiteral("scene");
    const QString chapter = QStringLiteral("chapter");
    if (kind == QLatin1String("blank")) {
        b.top(scene, manuscript, QStringLiteral("Untitled"));
        return 0;
    }
    if (kind == QLatin1String("short-story")) {
        b.top(scene, manuscript, QStringLiteral("Opening"));
        return 5000;
    }
    if (kind == QLatin1String("novella")) {
        b.below(b.top(chapter, manuscript, QStringLiteral("Chapter 1")), scene,
                QStringLiteral("Scene 1"));
        return 30000;
    }
    if (kind == QLatin1String("novel")) {
        titlePage(b);
        b.top(QStringLiteral("page"), QStringLiteral("front"), QStringLiteral("Dedication"));
        const int part = b.top(QStringLiteral("part"), manuscript, QStringLiteral("Part One"));
        b.below(b.below(part, chapter, QStringLiteral("Chapter 1")), scene,
                QStringLiteral("Scene 1"));
        b.top(QStringLiteral("page"), QStringLiteral("back"), QStringLiteral("Acknowledgements"));
        return 80000;
    }
    if (kind == QLatin1String("series")) {
        const int book = b.top(QStringLiteral("folder"), manuscript, QStringLiteral("Book One"));
        b.below(b.below(book, chapter, QStringLiteral("Chapter 1")), scene,
                QStringLiteral("Scene 1"));
        return 0;
    }
    if (kind == QLatin1String("essay")) {
        b.top(scene, manuscript, QStringLiteral("Draft"));
        return 3000;
    }
    if (kind == QLatin1String("essay-collection")) {
        titlePage(b);
        b.below(b.top(chapter, manuscript, QStringLiteral("First essay")), scene,
                QStringLiteral("Draft"));
        return 50000;
    }
    // non-fiction
    titlePage(b);
    b.top(QStringLiteral("page"), QStringLiteral("front"), QStringLiteral("Preface"));
    const int part = b.top(QStringLiteral("part"), manuscript, QStringLiteral("Part One"));
    b.below(b.below(part, chapter, QStringLiteral("Chapter 1")), scene,
            QStringLiteral("Section 1"));
    b.top(QStringLiteral("page"), QStringLiteral("back"), QStringLiteral("Acknowledgements"));
    b.top(QStringLiteral("page"), QStringLiteral("back"), QStringLiteral("Bibliography"));
    return 60000;
}

toml::date_time nowUtc()
{
    const QDateTime now = QDateTime::currentDateTimeUtc();
    const QDate d = now.date();
    const QTime t = now.time();
    return { toml::date { d.year(), d.month(), d.day() },
             toml::time { t.hour(), t.minute(), t.second() }, toml::time_offset { } };
}

} // namespace

namespace Templates {

QList<LoadError> create(const QString &folder, const QString &kind, const QString &title,
                        const QString &author)
{
    if (!ProjectStore::isProjectKind(kind))
        return { { folder, 0, QStringLiteral("\"%1\" is not a kind of project").arg(kind) } };
    QDir dir(folder);
    if (dir.exists() && !dir.isEmpty(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot))
        return { { folder, 0, QStringLiteral("the folder is not empty") } };
    if (!dir.mkpath(QStringLiteral(".")))
        return { { folder, 0, QStringLiteral("cannot make the folder") } };

    Project project;
    project.folder = folder;
    project.title = title;
    project.kind = kind;
    Builder builder(project);
    const int words = fill(builder, kind);
    if (!author.isEmpty())
        project.data.insert("author", author.toStdString());
    project.data.insert("created", nowUtc());
    if (words > 0)
        project.data.insert("goals", toml::table { { "words", words } });

    // Each text item starts as an empty file.
    for (const ProjectItem &item : std::as_const(project.items)) {
        if (item.file.isEmpty())
            continue;
        const QString path = dir.filePath(item.file);
        QSaveFile file(path);
        if (!dir.mkpath(item.file.section(u'/', 0, 0)) || !file.open(QIODevice::WriteOnly)
            || !file.commit())
            return { { item.file, 0, QStringLiteral("cannot make the file") } };
    }
    return ProjectStore::save(project);
}

} // namespace Templates
