#include "ProjectSession.h"

#include "Naming.h"
#include "ProjectStore.h"

#include <QDir>
#include <QSaveFile>
#include <QSet>

ProjectSession::ProjectSession(QObject *parent) : QObject(parent)
{
    connect(&text, &Document::saved, this, [this](const QString &file) {
        if (watcher)
            watcher->noteSaved(file);
    });
    connect(&text, &Document::dirtyChanged, this, [this] {
        if (watcher && !text.file().isEmpty())
            watcher->setUnsaved(text.file(), text.dirty());
    });
}

ProjectSession::~ProjectSession()
{
    text.save();
}

void ProjectSession::setFolder(const QString &folder)
{
    if (folder == project.folder && loadError.isEmpty())
        return;
    project = Project();
    project.folder = folder;
    reload();
}

void ProjectSession::reload()
{
    const QString folder = project.folder;
    if (folder.isEmpty()) {
        setCurrentId(QString());
        project = Project();
        loadError.clear();
        binderModel.setProject(project);
        watcher.reset();
        emit loaded();
        return;
    }
    const QString previous = current;
    setCurrentId(QString());
    LoadResult result = ProjectStore::load(folder);
    loadError = result.ok() ? QString() : result.errors.first().text();
    project = std::move(result.project);
    project.folder = folder;
    binderModel.setProject(project);
    watch();
    emit loaded();
    // After a reload the same item stays selected, if it is still there.
    setCurrentId(previous);
}

// Outside edits: a clean text reloads, and unsaved text gets one conflict.
void ProjectSession::watch()
{
    watcher.reset();
    if (!loadError.isEmpty())
        return;
    watcher = std::make_unique<Watcher>(project);
    connect(watcher.get(), &Watcher::textChanged, this, [this](const QString &file) {
        if (file == text.file())
            text.reloadIfClean();
    });
    connect(watcher.get(), &Watcher::conflict, this, [this](const QString &file) {
        if (file == text.file())
            text.markConflict();
    });
    // Queued: reload() deletes this watcher, and the watcher must first leave
    // the scan that sent the signal.
    connect(watcher.get(), &Watcher::projectChanged, this, &ProjectSession::reload,
            Qt::QueuedConnection);
}

void ProjectSession::setCurrentId(const QString &id)
{
    if (id == current)
        return;
    // Only a known item can be selected.
    QString file;
    bool known = id.isEmpty();
    for (const ProjectItem &item : std::as_const(project.items)) {
        if (item.id != id)
            continue;
        known = true;
        if (item.kind != QLatin1String("file") && !item.missingFile)
            file = item.file;
    }
    if (!known)
        return;
    current = id;
    text.open(project.folder, file);
    emit currentIdChanged();
}

namespace {

// What each kind can hold, and where a new item of a kind goes.
bool canHold(const QString &parentKind, const QString &kind)
{
    if (parentKind == QLatin1String("folder"))
        return true;
    if (parentKind == QLatin1String("part"))
        return kind == QLatin1String("chapter") || kind == QLatin1String("scene");
    if (parentKind == QLatin1String("chapter"))
        return kind == QLatin1String("scene");
    return false;
}

QString homeSection(const QString &kind)
{
    if (kind == QLatin1String("page"))
        return QStringLiteral("front");
    if (kind == QLatin1String("note"))
        return QStringLiteral("notes");
    return QStringLiteral("manuscript");
}

QString folderOf(const QString &kind)
{
    if (kind == QLatin1String("scene"))
        return QStringLiteral("manuscript/");
    if (kind == QLatin1String("page"))
        return QStringLiteral("matter/");
    if (kind == QLatin1String("note"))
        return QStringLiteral("notes/");
    return { };
}

QString newTitle(const QString &kind)
{
    return QStringLiteral("New ") + kind;
}

// The sections where an item of kind can be, as docs/format.md says.
bool fitsSection(const QString &kind, const QString &section)
{
    if (kind == QLatin1String("folder"))
        return true;
    if (kind == QLatin1String("page"))
        return section == QLatin1String("front") || section == QLatin1String("back");
    return section == homeSection(kind);
}

} // namespace

int ProjectSession::indexOf(const QString &id) const
{
    for (qsizetype i = 0; i < project.items.size(); ++i) {
        if (project.items.at(i).id == id)
            return static_cast<int>(i);
    }
    return -1;
}

void ProjectSession::setSection(int index, const QString &section)
{
    project.items[index].section = section;
    for (const int child : std::as_const(project.items.at(index).children))
        setSection(child, section);
}

// Takes the item out of its parent or out of the top level.
void ProjectSession::detach(int index)
{
    ProjectItem &item = project.items[index];
    if (item.parent >= 0)
        project.items[item.parent].children.removeOne(index);
    else
        project.roots.removeOne(index);
    item.parent = -1;
}

// Puts the item under parent (or at the top level for -1) at position.
void ProjectSession::place(int index, int parent, int position, const QString &section)
{
    QList<int> &list = parent >= 0 ? project.items[parent].children : project.roots;
    list.insert(std::clamp<qsizetype>(position, 0, list.size()), index);
    project.items[index].parent = parent;
    setSection(index, parent >= 0 ? project.items.at(parent).section : section);
}

bool ProjectSession::commit(const QStringList &newFiles)
{
    const QList<LoadError> errors = ProjectStore::save(project);
    editError = errors.isEmpty() ? QString() : errors.first().text();
    if (watcher) {
        watcher->track(project);
        watcher->noteSaved(QStringLiteral("binder.toml"));
        for (const QString &file : newFiles)
            watcher->noteSaved(file);
    }
    binderModel.setProject(project);
    emit edited();
    return errors.isEmpty();
}

QString ProjectSession::add(const QString &kind)
{
    editError.clear();
    if (project.readOnly || !loadError.isEmpty()) {
        editError = QStringLiteral("This project cannot change.");
        emit edited();
        return { };
    }
    // Into the selected item if it can hold the kind, else after it.
    int parent = -1;
    int position = static_cast<int>(project.roots.size());
    QString section = homeSection(kind);
    const int selected = indexOf(current);
    if (selected >= 0 && project.items.at(selected).section != QLatin1String("trash")) {
        const ProjectItem &item = project.items.at(selected);
        if (canHold(item.kind, kind)) {
            parent = selected;
            position = static_cast<int>(item.children.size());
        } else if (item.parent >= 0 && canHold(project.items.at(item.parent).kind, kind)) {
            parent = item.parent;
            position = static_cast<int>(project.items.at(parent).children.indexOf(selected)) + 1;
        } else if (item.parent < 0 && fitsSection(kind, item.section)) {
            section = item.section;
            position = static_cast<int>(project.roots.indexOf(selected)) + 1;
        }
    }

    QSet<QString> ids;
    for (const ProjectItem &item : std::as_const(project.items))
        ids.insert(item.id);
    ProjectItem item;
    item.id = Naming::newItemId(ids);
    item.kind = kind;
    item.title = newTitle(kind);
    QStringList newFiles;
    const QString folder = folderOf(kind);
    if (!folder.isEmpty()) {
        const QDir dir(project.folder);
        dir.mkpath(folder);
        const QStringList names = QDir(dir.filePath(folder)).entryList(QDir::Files);
        item.file
            = folder + Naming::fileNameFor(item.title, QSet<QString>(names.begin(), names.end()));
        QSaveFile file(dir.filePath(item.file));
        if (!file.open(QIODevice::WriteOnly) || !file.commit()) {
            editError = QStringLiteral("Cannot make %1.").arg(item.file);
            emit edited();
            return { };
        }
        newFiles.append(item.file);
    }
    project.items.append(item);
    place(static_cast<int>(project.items.size() - 1), parent, position, section);
    commit(newFiles);
    setCurrentId(item.id);
    return item.id;
}

bool ProjectSession::rename(const QString &id, const QString &title)
{
    const int index = indexOf(id);
    const QString name = title.trimmed();
    if (index < 0 || name.isEmpty() || project.readOnly)
        return false;
    if (project.items.at(index).title == name)
        return true;
    project.items[index].title = name;
    return commit({ });
}

bool ProjectSession::moveToTrash(const QString &id)
{
    const int index = indexOf(id);
    if (index < 0 || project.readOnly || project.items.at(index).section == QLatin1String("trash"))
        return false;
    text.save();
    ProjectItem &item = project.items[index];
    if (item.parent >= 0)
        item.data.insert_or_assign("restore", project.items.at(item.parent).id.toStdString());
    else
        item.data.erase("restore");
    detach(index);
    place(index, -1, static_cast<int>(project.roots.size()), QStringLiteral("trash"));
    // The selection clears if the selected item went to the trash with the
    // branch.
    const int selected = indexOf(current);
    if (selected >= 0 && project.items.at(selected).section == QLatin1String("trash"))
        setCurrentId(QString());
    return commit({ });
}

bool ProjectSession::restore(const QString &id)
{
    const int index = indexOf(id);
    if (index < 0 || project.readOnly || project.items.at(index).section != QLatin1String("trash"))
        return false;
    ProjectItem &item = project.items[index];
    const int parent
        = indexOf(QString::fromStdString(item.data["restore"].value_or(std::string())));
    item.data.erase("restore");
    detach(index);
    if (parent >= 0 && project.items.at(parent).section != QLatin1String("trash"))
        place(index, parent, static_cast<int>(project.items.at(parent).children.size()), { });
    else
        place(index, -1, static_cast<int>(project.roots.size()), homeSection(item.kind));
    return commit({ });
}

bool ProjectSession::resolveMove(int index, int target, const QString &position, int *parent,
                                 int *at, QString *section) const
{
    if (index < 0 || target < 0 || index == target || project.readOnly)
        return false;
    const QString trash = QStringLiteral("trash");
    if (project.items.at(index).section == trash || project.items.at(target).section == trash)
        return false;
    // An item cannot go into its own branch.
    for (int up = target; up >= 0; up = project.items.at(up).parent) {
        if (up == index)
            return false;
    }
    const QString &kind = project.items.at(index).kind;
    const ProjectItem &other = project.items.at(target);
    if (position == QLatin1String("inside")) {
        if (!canHold(other.kind, kind))
            return false;
        *parent = target;
        *at = static_cast<int>(other.children.size());
        *section = other.section;
        return true;
    }
    if (position != QLatin1String("before") && position != QLatin1String("after"))
        return false;
    *parent = other.parent;
    *section = other.section;
    if (*parent >= 0 ? !canHold(project.items.at(*parent).kind, kind)
                     : !fitsSection(kind, *section))
        return false;
    const QList<int> &siblings = *parent >= 0 ? project.items.at(*parent).children : project.roots;
    *at = static_cast<int>(siblings.indexOf(target)) + (position == QLatin1String("after") ? 1 : 0);
    // The item leaves its old place first, so a later place moves up by one.
    const qsizetype old = siblings.indexOf(index);
    if (old >= 0 && old < *at)
        --*at;
    return true;
}

bool ProjectSession::canMove(const QString &id, const QString &target,
                             const QString &position) const
{
    int parent = -1;
    int at = 0;
    QString section;
    return resolveMove(indexOf(id), indexOf(target), position, &parent, &at, &section);
}

bool ProjectSession::move(const QString &id, const QString &target, const QString &position)
{
    const int index = indexOf(id);
    int parent = -1;
    int at = 0;
    QString section;
    if (!resolveMove(index, indexOf(target), position, &parent, &at, &section))
        return false;
    detach(index);
    place(index, parent, at, section);
    return commit({ });
}

bool ProjectSession::moveBy(const QString &id, int delta)
{
    const int index = indexOf(id);
    if (index < 0 || (delta != -1 && delta != 1))
        return false;
    const ProjectItem &item = project.items.at(index);
    const QList<int> &siblings
        = item.parent >= 0 ? project.items.at(item.parent).children : project.roots;
    // Among the top level items, only the items of the same section count.
    qsizetype at = siblings.indexOf(index) + delta;
    while (at >= 0 && at < siblings.size()
           && project.items.at(siblings.at(at)).section != item.section)
        at += delta;
    if (at < 0 || at >= siblings.size())
        return false;
    return move(id, project.items.at(siblings.at(at)).id,
                delta < 0 ? QStringLiteral("before") : QStringLiteral("after"));
}
