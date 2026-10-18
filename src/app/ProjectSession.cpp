#include "ProjectSession.h"

#include "Naming.h"
#include "ProjectStore.h"

#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QSet>

#include <functional>

ProjectSession::ProjectSession(QObject *parent) : QObject(parent)
{
    connect(&text, &Document::saved, this, [this](const QString &file) {
        if (watcher)
            watcher->noteSaved(file);
    });
    // The live count of the open text goes into the binder, so the counts of
    // the chapter and the book follow it.
    connect(&text, &Document::wordsChanged, this, [this] {
        if (!text.file().isEmpty())
            binderModel.setItemWords(current, text.words());
        emit wordsChanged();
    });
    connect(this, &ProjectSession::currentIdChanged, this, &ProjectSession::wordsChanged);
    connect(this, &ProjectSession::edited, this, &ProjectSession::wordsChanged);
    connect(this, &ProjectSession::loaded, this, &ProjectSession::wordsChanged);
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

int ProjectSession::chapterWords() const
{
    // A selected chapter counts itself.
    const int index = indexOf(current);
    if (index >= 0 && project.items.at(index).kind == QLatin1String("chapter"))
        return binderModel.wordsOf(current);
    const QString chapter = binderModel.chapterOf(current);
    return chapter.isEmpty() ? -1 : binderModel.wordsOf(chapter);
}

int ProjectSession::goalWords() const
{
    return static_cast<int>(project.data["goals"]["words"].value_or(std::int64_t { 0 }));
}

namespace {

QString readTextFile(const QString &path, bool *ok)
{
    QFile file(path);
    *ok = file.open(QIODevice::ReadOnly);
    QString text = *ok ? QString::fromUtf8(file.readAll()) : QString();
    if (text.endsWith(u'\n'))
        text.chop(1);
    return text;
}

bool writeTextFile(const QString &path, QString text)
{
    if (!text.isEmpty())
        text += u'\n';
    QSaveFile file(path);
    const QByteArray bytes = text.toUtf8();
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size() && file.commit();
}

} // namespace

QString ProjectSession::split(const QString &id, int position)
{
    editError.clear();
    const int index = indexOf(id);
    if (index < 0 || project.readOnly || project.items.at(index).kind != QLatin1String("scene")
        || project.items.at(index).section == QLatin1String("trash")) {
        editError = QStringLiteral("Only a scene can split.");
        emit edited();
        return { };
    }
    // The file must hold the latest text before the split reads it.
    if (current == id && !text.save()) {
        editError = QStringLiteral("Save the scene first.");
        emit edited();
        return { };
    }
    const QDir dir(project.folder);
    const ProjectItem source = project.items.at(index);
    bool ok = false;
    const QString all = readTextFile(dir.filePath(source.file), &ok);
    // Only the whitespace at the cut moves; no other character changes.
    const QString first = all.left(position).trimmed().isEmpty() ? QString() : [&] {
        QString left = all.left(position);
        while (!left.isEmpty() && left.back().isSpace())
            left.chop(1);
        return left;
    }();
    QString second = all.mid(position);
    while (!second.isEmpty() && second.front().isSpace())
        second.remove(0, 1);
    if (!ok || position < 0 || position > all.size() || first.isEmpty() || second.isEmpty()) {
        editError = QStringLiteral("There is no text on one side of the cursor.");
        emit edited();
        return { };
    }

    QSet<QString> ids;
    for (const ProjectItem &item : std::as_const(project.items))
        ids.insert(item.id);
    ProjectItem item;
    item.id = Naming::newItemId(ids);
    item.kind = source.kind;
    item.title = source.title + QStringLiteral(" (2)");
    for (const char *key : { "pov", "place", "time" }) {
        if (const toml::node *value = source.data.get(key))
            item.data.insert(key, *value);
    }
    const QStringList names
        = QDir(dir.filePath(QStringLiteral("manuscript"))).entryList(QDir::Files);
    item.file = QStringLiteral("manuscript/")
        + Naming::fileNameFor(item.title, QSet<QString>(names.begin(), names.end()));
    // The new file first: a crash after this keeps the text twice, never
    // loses it.
    if (!writeTextFile(dir.filePath(item.file), second)
        || !writeTextFile(dir.filePath(source.file), first)) {
        editError = QStringLiteral("Cannot write the split scenes.");
        emit edited();
        return { };
    }
    project.items.append(item);
    const int added = static_cast<int>(project.items.size() - 1);
    const int parent = project.items.at(index).parent;
    const QList<int> &siblings = parent >= 0 ? project.items.at(parent).children : project.roots;
    place(added, parent, static_cast<int>(siblings.indexOf(index)) + 1, source.section);
    commit({ item.file, source.file });
    if (current == id)
        text.open(project.folder, source.file);
    setCurrentId(item.id);
    return item.id;
}

QString ProjectSession::nextScene(const QString &id) const
{
    const int index = indexOf(id);
    if (index < 0 || project.items.at(index).kind != QLatin1String("scene")
        || project.items.at(index).section == QLatin1String("trash"))
        return { };
    const int parent = project.items.at(index).parent;
    const QList<int> &siblings = parent >= 0 ? project.items.at(parent).children : project.roots;
    const qsizetype at = siblings.indexOf(index) + 1;
    if (at >= siblings.size() || project.items.at(siblings.at(at)).kind != QLatin1String("scene"))
        return { };
    return project.items.at(siblings.at(at)).id;
}

bool ProjectSession::mergeWithNext(const QString &id)
{
    editError.clear();
    const QString next = nextScene(id);
    if (next.isEmpty() || project.readOnly) {
        editError = QStringLiteral("There is no next scene to merge.");
        emit edited();
        return false;
    }
    text.save();
    const QDir dir(project.folder);
    const int first = indexOf(id);
    const int second = indexOf(next);
    bool okFirst = false;
    bool okSecond = false;
    const QString a = readTextFile(dir.filePath(project.items.at(first).file), &okFirst);
    const QString b = readTextFile(dir.filePath(project.items.at(second).file), &okSecond);
    if (!okFirst || !okSecond) {
        editError = QStringLiteral("Cannot read the scenes to merge.");
        emit edited();
        return false;
    }
    const QString joined = a.isEmpty() ? b : b.isEmpty() ? a : a + QStringLiteral("\n\n") + b;
    if (!writeTextFile(dir.filePath(project.items.at(first).file), joined)) {
        editError = QStringLiteral("Cannot write the merged scene.");
        emit edited();
        return false;
    }
    // The notes of the second scene join the notes of the first.
    const QString notesA
        = QString::fromStdString(project.items.at(first).data["notes"].value_or(std::string()));
    const QString notesB
        = QString::fromStdString(project.items.at(second).data["notes"].value_or(std::string()));
    if (!notesB.isEmpty()) {
        const QString notes = notesA.isEmpty() ? notesB : notesA + QStringLiteral("\n\n") + notesB;
        project.items[first].data.insert_or_assign("notes", notes.toStdString());
    }
    if (watcher)
        watcher->noteSaved(project.items.at(first).file);
    // The second scene keeps its file in the trash, so its old text stays.
    moveToTrash(next);
    if (current == id || current.isEmpty())
        text.open(project.folder, project.items.at(first).file);
    setCurrentId(id);
    return true;
}

QVariantMap ProjectSession::fields(const QString &id) const
{
    QVariantMap map;
    const int index = indexOf(id);
    if (index < 0)
        return map;
    const ProjectItem &item = project.items.at(index);
    const auto string = [&item](const char *key) {
        return QString::fromStdString(item.data[key].value_or(std::string()));
    };
    map.insert(QStringLiteral("title"), item.title);
    map.insert(QStringLiteral("kind"), item.kind);
    const QString status = string("status");
    map.insert(QStringLiteral("status"),
               status.isEmpty() && item.kind == QLatin1String("scene") ? QStringLiteral("draft")
                                                                       : status);
    for (const char *key : { "pov", "place", "time", "synopsis", "notes" })
        map.insert(QString::fromLatin1(key), string(key));
    QStringList tags;
    if (const toml::array *list = item.data.get_as<toml::array>("tags")) {
        for (const toml::node &tag : *list)
            tags.append(QString::fromStdString(tag.value_or(std::string())));
    }
    map.insert(QStringLiteral("tags"), tags);
    map.insert(QStringLiteral("target"),
               static_cast<int>(item.data["target"].value_or(std::int64_t { 0 })));
    map.insert(QStringLiteral("words"), binderModel.wordsOf(id));
    return map;
}

bool ProjectSession::setField(const QString &id, const QString &key, const QVariant &value)
{
    const int index = indexOf(id);
    if (index < 0 || project.readOnly)
        return false;
    toml::table &data = project.items[index].data;
    const std::string name = key.toStdString();
    if (key == QLatin1String("tags")) {
        // Tags: trimmed, no empty tags, no copies, in the order given.
        QStringList tags;
        for (const QString &tag : value.toStringList()) {
            const QString clean = tag.trimmed();
            if (!clean.isEmpty() && !tags.contains(clean))
                tags.append(clean);
        }
        if (tags.isEmpty()) {
            data.erase(name);
        } else {
            toml::array list;
            for (const QString &tag : std::as_const(tags))
                list.push_back(tag.toStdString());
            data.insert_or_assign(name, std::move(list));
        }
    } else if (key == QLatin1String("target")) {
        bool ok = false;
        const int target = value.toInt(&ok);
        if (!ok || target < 0)
            return false;
        if (target == 0)
            data.erase(name);
        else
            data.insert_or_assign(name, target);
    } else if (key == QLatin1String("status")) {
        const QString status = value.toString();
        if (status != QLatin1String("draft") && status != QLatin1String("revised")
            && status != QLatin1String("final"))
            return false;
        data.insert_or_assign(name, status.toStdString());
    } else if (key == QLatin1String("pov") || key == QLatin1String("place")
               || key == QLatin1String("time") || key == QLatin1String("synopsis")
               || key == QLatin1String("notes")) {
        const QString text = value.toString().trimmed();
        if (text.isEmpty())
            data.erase(name);
        else
            data.insert_or_assign(name, text.toStdString());
    } else {
        return false;
    }
    return commit({ });
}

QVariantList ProjectSession::textsUnder(const QString &id) const
{
    QVariantList list;
    const int index = indexOf(id);
    if (index < 0)
        return list;
    const std::function<void(int)> walk = [&](int at) {
        for (const int child : project.items.at(at).children) {
            const ProjectItem &item = project.items.at(child);
            if (!item.file.isEmpty() && item.kind != QLatin1String("file") && !item.missingFile)
                list.append(QVariantMap { { QStringLiteral("id"), item.id },
                                          { QStringLiteral("title"), item.title },
                                          { QStringLiteral("file"), item.file } });
            walk(child);
        }
    };
    walk(index);
    return list;
}
