#include "ProjectSession.h"

#include "Backup.h"
#include "Diff.h"
#include "Index.h"
#include "Naming.h"
#include "ProjectStore.h"
#include "Snapshots.h"

#include <QDir>
#include <QFile>
#include <QLocale>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>

#include <functional>
#include <sstream>
#include <utility>

ProjectSession::ProjectSession(QObject *parent) : QObject(parent)
{
    connect(&text, &Document::saved, this, [this](const QString &file) {
        changedSinceBackup = true;
        noteSaved(file);
    });
    connect(this, &ProjectSession::snapshotsChanged, this, [this] { changedSinceBackup = true; });
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
    backup();
}

void ProjectSession::noteSaved(const QString &file)
{
    staleFiles.insert(file);
    emit textsChanged();
    if (watcher)
        watcher->noteSaved(file);
}

QString ProjectSession::backup()
{
    saveAll();
    if (!changedSinceBackup || project.folder.isEmpty() || !loadError.isEmpty()
        || backupFolder.isEmpty())
        return { };
    QString error;
    const QString zip = Backup::write(project.folder, backupFolder, backupsToKeep, &error);
    if (zip.isEmpty())
        qWarning("Codicier: backup failed: %s", qPrintable(error));
    else
        changedSinceBackup = false;
    return zip;
}

void ProjectSession::setFolder(const QString &folder)
{
    if (folder == project.folder && loadError.isEmpty())
        return;
    // The project that closes gets its backup.
    if (folder != project.folder)
        backup();
    project = Project();
    project.folder = folder;
    changedSinceBackup = false;
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
        index.reset();
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
    index.reset();
    indexStale = true;
    staleFiles.clear();
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
        staleFiles.insert(file);
        emit textsChanged();
        if (file == text.file())
            text.reloadIfClean();
        for (const QPointer<Document> &document : std::as_const(tracked)) {
            if (document && document->file() == file)
                document->reloadIfClean();
        }
    });
    connect(watcher.get(), &Watcher::conflict, this, [this](const QString &file) {
        if (file == text.file())
            text.markConflict();
        for (const QPointer<Document> &document : std::as_const(tracked)) {
            if (document && document->file() == file)
                document->markConflict();
        }
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
    if (kind == QLatin1String("search"))
        return QStringLiteral("searches");
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
    // The open text saves first, so the counts that the binder reads from the
    // files include the latest typing.
    text.save();
    changedSinceBackup = true;
    indexStale = true;
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

QString ProjectSession::saveSearch(const QString &query, bool regex)
{
    if (project.readOnly || !loadError.isEmpty() || query.trimmed().isEmpty())
        return { };
    QSet<QString> ids;
    for (const ProjectItem &each : std::as_const(project.items))
        ids.insert(each.id);
    ProjectItem item;
    item.id = Naming::newItemId(ids);
    item.kind = QStringLiteral("search");
    item.title = query.trimmed();
    item.data.insert_or_assign("query", query.toStdString());
    // The key is written only when it is true, as include only when false.
    if (regex)
        item.data.insert_or_assign("regex", true);
    project.items.append(item);
    place(static_cast<int>(project.items.size() - 1), -1, static_cast<int>(project.roots.size()),
          QStringLiteral("searches"));
    if (!commit({ }))
        return { };
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
    noteSaved(project.items.at(first).file);
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
    map.insert(QStringLiteral("include"), item.data["include"].value_or(true));
    map.insert(QStringLiteral("query"), string("query"));
    map.insert(QStringLiteral("regex"), item.data["regex"].value_or(false));
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
    } else if (key == QLatin1String("include")) {
        // The default is true, so only false is written.
        if (value.typeId() != QMetaType::Bool)
            return false;
        if (value.toBool())
            data.erase(name);
        else
            data.insert_or_assign(name, false);
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

QVariantMap ProjectSession::viewState() const
{
    QVariantMap map;
    if (project.folder.isEmpty())
        return map;
    QFile file(QDir(project.folder).filePath(QStringLiteral(".codicier/state.toml")));
    if (!file.open(QIODevice::ReadOnly))
        return map;
    const QByteArray bytes = file.readAll();
    // The app can lose this file with no harm, so a bad file is no state.
    toml::table table;
    try {
        table = toml::parse(std::string_view(bytes.constData(), bytes.size()));
    } catch (const toml::parse_error &) {
        return map;
    }
    for (const auto &[key, node] : table) {
        const QString name
            = QString::fromUtf8(key.str().data(), static_cast<qsizetype>(key.str().size()));
        if (const auto *text = node.as_string())
            map.insert(name, QString::fromStdString(text->get()));
        else if (const auto *flag = node.as_boolean())
            map.insert(name, flag->get());
        else if (const auto *number = node.as_integer())
            map.insert(name, static_cast<int>(number->get()));
    }
    return map;
}

bool ProjectSession::saveViewState(const QVariantMap &state)
{
    if (project.folder.isEmpty() || !loadError.isEmpty())
        return false;
    if (!ProjectStore::makeAppFolder(project.folder).isEmpty())
        return false;
    const QDir dir(project.folder);
    toml::table table;
    for (auto it = state.cbegin(); it != state.cend(); ++it) {
        const std::string key = it.key().toStdString();
        switch (it.value().typeId()) {
        case QMetaType::Bool:
            table.insert(key, it.value().toBool());
            break;
        case QMetaType::Int:
        case QMetaType::LongLong:
        case QMetaType::Double:
            table.insert(key, static_cast<std::int64_t>(it.value().toDouble()));
            break;
        default:
            table.insert(key, it.value().toString().toStdString());
        }
    }
    std::ostringstream out;
    out << table << '\n';
    const QByteArray bytes = QByteArray::fromStdString(out.str());
    QSaveFile file(dir.filePath(QStringLiteral(".codicier/state.toml")));
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size() && file.commit();
}

QVariantList ProjectSession::items() const
{
    QVariantList list;
    const std::function<void(int, const QString &)> walk = [&](int at, const QString &path) {
        const ProjectItem &item = project.items.at(at);
        list.append(QVariantMap { { QStringLiteral("id"), item.id },
                                  { QStringLiteral("title"), item.title },
                                  { QStringLiteral("kind"), item.kind },
                                  { QStringLiteral("path"), path } });
        const QString below
            = path.isEmpty() ? item.title : path + QStringLiteral(" / ") + item.title;
        for (const int child : item.children)
            walk(child, below);
    };
    for (const int root : project.roots) {
        if (project.items.at(root).section != QLatin1String("trash"))
            walk(root, QString());
    }
    return list;
}

void ProjectSession::trackDocument(Document *document, const QString &id)
{
    if (document == nullptr)
        return;
    tracked.removeAll(nullptr);
    tracked.append(document);
    connect(document, &Document::saved, this, [this](const QString &file) {
        changedSinceBackup = true;
        noteSaved(file);
    });
    connect(document, &Document::dirtyChanged, this, [this, document] {
        if (watcher && !document->file().isEmpty())
            watcher->setUnsaved(document->file(), document->dirty());
    });
    connect(document, &Document::wordsChanged, this, [this, document, id] {
        binderModel.setItemWords(id, document->words());
        emit wordsChanged();
    });
}

QString ProjectSession::takeSnapshot(const QString &id, const QString &title)
{
    const int index = indexOf(id);
    if (index < 0 || project.items.at(index).file.isEmpty()
        || project.items.at(index).kind == QLatin1String("file"))
        return { };
    // The snapshot holds the latest typing.
    if (current == id)
        text.save();
    for (const QPointer<Document> &document : std::as_const(tracked)) {
        if (document && document->file() == project.items.at(index).file)
            document->save();
    }
    const QString path = Snapshots::take(project.folder, id, project.items.at(index).file, title);
    if (!path.isEmpty())
        emit snapshotsChanged();
    return path;
}

QVariantList ProjectSession::snapshots(const QString &id) const
{
    QVariantList list;
    const QLocale english(QLocale::English, QLocale::UnitedKingdom);
    for (const Snapshots::Snapshot &snapshot : Snapshots::list(project.folder, id)) {
        list.append(QVariantMap {
            { QStringLiteral("path"), snapshot.path },
            { QStringLiteral("time"), snapshot.time },
            { QStringLiteral("title"), snapshot.title },
            { QStringLiteral("label"),
              english.toString(snapshot.time, QStringLiteral("d MMMM yyyy, HH:mm")) },
        });
    }
    return list;
}

bool ProjectSession::restoreSnapshot(const QString &id, const QString &path)
{
    const int index = indexOf(id);
    if (index < 0 || project.readOnly)
        return false;
    const QString file = project.items.at(index).file;
    // The text to replace must be on disk, so "Before restore" holds it.
    if (current == id && !text.save())
        return false;
    for (const QPointer<Document> &document : std::as_const(tracked)) {
        if (document && document->file() == file)
            document->save();
    }
    if (!Snapshots::restore(project.folder, id, file, path))
        return false;
    noteSaved(file);
    if (current == id)
        text.open(project.folder, file);
    for (const QPointer<Document> &document : std::as_const(tracked)) {
        if (document && document->file() == file)
            document->reloadIfClean();
    }
    emit snapshotsChanged();
    return true;
}

QVariantMap ProjectSession::compare(const QString &id, const QString &snapshotPath) const
{
    const int index = indexOf(id);
    if (index < 0 || !snapshotPath.startsWith(QStringLiteral("snapshots/") + id + u'/')
        || snapshotPath.contains(QStringLiteral("..")))
        return { };
    const QDir dir(project.folder);
    bool ok = false;
    const QString before = readTextFile(dir.filePath(snapshotPath), &ok);
    if (!ok)
        return { };
    // The open text is newer than its file while edits wait for the save.
    const QString after = current == id
        ? text.text()
        : readTextFile(dir.filePath(project.items.at(index).file), &ok);
    QVariantList segments;
    int added = 0;
    int removed = 0;
    for (const Diff::Segment &segment : Diff::words(before, after)) {
        const char *kind = segment.kind == Diff::Segment::Added ? "added"
            : segment.kind == Diff::Segment::Removed            ? "removed"
                                                                : "same";
        if (segment.kind == Diff::Segment::Added)
            added += Index::countWords(segment.text);
        else if (segment.kind == Diff::Segment::Removed)
            removed += Index::countWords(segment.text);
        segments.append(QVariantMap { { QStringLiteral("kind"), QString::fromLatin1(kind) },
                                      { QStringLiteral("text"), segment.text } });
    }
    return { { QStringLiteral("segments"), segments },
             { QStringLiteral("addedWords"), added },
             { QStringLiteral("removedWords"), removed } };
}

int ProjectSession::trashCount() const
{
    int count = 0;
    for (const ProjectItem &item : project.items) {
        if (item.section == QLatin1String("trash"))
            ++count;
    }
    return count;
}

int ProjectSession::emptyTrash()
{
    editError.clear();
    if (project.readOnly || !loadError.isEmpty())
        return -1;
    const int count = trashCount();
    if (count == 0)
        return 0;
    // A backup first, so a mistake can be undone from the zip.
    if (!backupFolder.isEmpty()) {
        changedSinceBackup = true;
        if (backup().isEmpty()) {
            editError = QStringLiteral("The backup failed, so the trash stays.");
            emit edited();
            return -1;
        }
    }
    if (indexOf(current) >= 0
        && project.items.at(indexOf(current)).section == QLatin1String("trash"))
        setCurrentId(QString());

    const QDir dir(project.folder);
    QList<ProjectItem> kept;
    QList<int> newIndex(project.items.size(), -1);
    for (qsizetype i = 0; i < project.items.size(); ++i) {
        const ProjectItem &item = project.items.at(i);
        if (item.section != QLatin1String("trash")) {
            newIndex[i] = static_cast<int>(kept.size());
            kept.append(item);
            continue;
        }
        if (!item.file.isEmpty())
            QFile::remove(dir.filePath(item.file));
        QDir(dir.filePath(QStringLiteral("snapshots/") + item.id)).removeRecursively();
    }
    // The indexes of the parents, the children, and the roots change.
    for (ProjectItem &item : kept) {
        item.parent = item.parent >= 0 ? newIndex.at(item.parent) : -1;
        QList<int> children;
        for (const int child : std::as_const(item.children)) {
            if (newIndex.at(child) >= 0)
                children.append(newIndex.at(child));
        }
        item.children = children;
    }
    QList<int> roots;
    for (const int root : std::as_const(project.roots)) {
        if (newIndex.at(root) >= 0)
            roots.append(newIndex.at(root));
    }
    project.items = std::move(kept);
    project.roots = std::move(roots);
    commit({ });
    return count;
}

namespace {

// Marks that no text holds, so the escape of a snippet cannot touch them.
const QChar openMark(0x2);
const QChar closeMark(0x3);

// A snippet with marks as rich text: the words with the match in bold, with
// no Markdown marks and no footnotes.
QString snippetHtml(QString snippet)
{
    snippet.remove(QRegularExpression(QStringLiteral("\\[\\^[^\\]]*\\]|\\*")));
    snippet = snippet.simplified().toHtmlEscaped();
    return snippet.replace(openMark, QStringLiteral("<b>"))
        .replace(closeMark, QStringLiteral("</b>"));
}

QRegularExpression searchPattern(const QString &pattern)
{
    return QRegularExpression(pattern,
                              QRegularExpression::CaseInsensitiveOption
                                  | QRegularExpression::UseUnicodePropertiesOption);
}

// The first match that is not empty, as a start and a length. The start is
// -1 if there is no such match.
std::pair<qsizetype, qsizetype> regexMatch(const QRegularExpression &pattern, const QString &text)
{
    QRegularExpressionMatchIterator it = pattern.globalMatch(text);
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        if (match.capturedLength() > 0)
            return { match.capturedStart(), match.capturedLength() };
    }
    return { -1, 0 };
}

QString readFile(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readAll()) : QString();
}

} // namespace

QString ProjectSession::patternError(const QString &pattern) const
{
    const QRegularExpression expression = searchPattern(pattern);
    if (expression.isValid())
        return { };
    // PCRE writes "missing closing parenthesis": make it a sentence.
    QString error = expression.errorString();
    error[0] = error[0].toUpper();
    return error + u'.';
}

QVariantList ProjectSession::search(const QString &query, bool regex)
{
    QVariantList list;
    if (project.folder.isEmpty() || !loadError.isEmpty())
        return list;
    // The search reads the files, so the latest typing goes to disk first.
    saveAll();
    const auto append = [&list](const QString &id, const QString &title, const QString &snippet) {
        list.append(QVariantMap {
            { QStringLiteral("id"), id },
            { QStringLiteral("title"), title },
            { QStringLiteral("snippet"), snippetHtml(snippet) },
        });
    };
    if (regex) {
        // A pattern search reads every file in binder order. The index
        // cannot match a pattern.
        const QRegularExpression pattern = searchPattern(query);
        if (query.isEmpty() || !pattern.isValid())
            return list;
        for (const ProjectItem &item : std::as_const(project.items)) {
            if (binderModel.isInTrash(item.id) || item.kind == QLatin1String("search"))
                continue;
            const bool isText = !item.file.isEmpty() && item.kind != QLatin1String("file");
            const QString body
                = isText ? readFile(QDir(project.folder).filePath(item.file)) : QString();
            const auto [start, length] = regexMatch(pattern, body);
            if (start >= 0) {
                // About ten words on each side of the match.
                const qsizetype from = std::max<qsizetype>(0, start - 60);
                const qsizetype to = std::min(body.size(), start + length + 80);
                const QString snippet = (from > 0 ? QStringLiteral("...") : QString())
                    + body.mid(from, start - from) + openMark + body.mid(start, length) + closeMark
                    + body.mid(start + length, to - start - length)
                    + (to < body.size() ? QStringLiteral("...") : QString());
                append(item.id, item.title, snippet);
            } else if (regexMatch(pattern, item.title).first >= 0) {
                append(item.id, item.title, QString());
            }
        }
        return list;
    }
    if (!index) {
        index = std::make_unique<Index>(project.folder);
        if (const QString error = index->open(); !error.isEmpty()) {
            qWarning("Codicier: cannot open the search index: %s", qPrintable(error));
            index.reset();
            return list;
        }
        indexStale = true;
    }
    QString error;
    if (indexStale) {
        error = index->rebuild(project);
    } else {
        for (const ProjectItem &item : std::as_const(project.items)) {
            if (error.isEmpty() && !item.file.isEmpty() && staleFiles.contains(item.file))
                error = index->update(project, item.id);
        }
    }
    if (!error.isEmpty()) {
        qWarning("Codicier: cannot update the search index: %s", qPrintable(error));
        return list;
    }
    indexStale = false;
    staleFiles.clear();
    for (const SearchHit &hit : index->search(query, openMark, closeMark)) {
        // A saved search is not a result: its title is only a query.
        const int at = indexOf(hit.id);
        if (at >= 0 && !binderModel.isInTrash(hit.id)
            && project.items.at(at).kind != QLatin1String("search"))
            append(hit.id, hit.title, hit.snippet);
    }
    return list;
}

namespace {

// The text of one manuscript item and the matches in it.
struct ReplaceTarget
{
    QString id;
    QString title;
    QString file;
    QString text;
    QList<QRegularExpressionMatch> matches;
};

// A plain query matches as it is written, as a whole word. A pattern
// ignores case, as in the search.
QRegularExpression replacePattern(const QString &query, bool regex)
{
    if (regex)
        return searchPattern(query);
    return QRegularExpression(QStringLiteral("(?<!\\w)") + QRegularExpression::escape(query)
                                  + QStringLiteral("(?!\\w)"),
                              QRegularExpression::UseUnicodePropertiesOption);
}

// The replacement for one match. In a pattern, \1 to \9 are the groups and
// \0 is the whole match.
QString expand(const QString &replacement, const QRegularExpressionMatch &match, bool regex)
{
    if (!regex)
        return replacement;
    QString out;
    for (qsizetype i = 0; i < replacement.size(); ++i) {
        const QChar c = replacement.at(i);
        if (c == u'\\' && i + 1 < replacement.size() && replacement.at(i + 1).isDigit()) {
            out += match.captured(replacement.at(i + 1).digitValue());
            ++i;
        } else {
            out += c;
        }
    }
    return out;
}

// The text items of the manuscript outside the trash, with their matches.
QList<ReplaceTarget> replaceTargets(const Project &project, const BinderModel &binder,
                                    const QRegularExpression &pattern)
{
    QList<ReplaceTarget> targets;
    if (!pattern.isValid() || pattern.pattern().isEmpty())
        return targets;
    for (const ProjectItem &item : project.items) {
        if (item.section != QLatin1String("manuscript") || item.file.isEmpty()
            || item.kind == QLatin1String("file") || binder.isInTrash(item.id))
            continue;
        ReplaceTarget target {
            item.id, item.title, item.file, readFile(QDir(project.folder).filePath(item.file)), { }
        };
        QRegularExpressionMatchIterator it = pattern.globalMatch(target.text);
        while (it.hasNext()) {
            const QRegularExpressionMatch match = it.next();
            if (match.capturedLength() > 0)
                target.matches.append(match);
        }
        if (!target.matches.isEmpty())
            targets.append(target);
    }
    return targets;
}

} // namespace

QVariantList ProjectSession::replacePreview(const QString &query, const QString &replacement,
                                            bool regex)
{
    QVariantList list;
    saveAll();
    // Marks that no text holds, for the old and the new words.
    const QChar newOpen(0x4);
    const QChar newClose(0x5);
    for (const ReplaceTarget &target :
         replaceTargets(project, binderModel, replacePattern(query, regex))) {
        QStringList lines;
        for (const QRegularExpressionMatch &match : target.matches) {
            const qsizetype start = match.capturedStart();
            const qsizetype end = match.capturedEnd();
            const qsizetype from = std::max<qsizetype>(0, start - 40);
            const qsizetype to = std::min(target.text.size(), end + 40);
            QString line = (from > 0 ? QStringLiteral("...") : QString())
                + target.text.mid(from, start - from) + openMark + match.captured() + closeMark
                + u' ' + newOpen + expand(replacement, match, regex) + newClose
                + target.text.mid(end, to - end)
                + (to < target.text.size() ? QStringLiteral("...") : QString());
            line = line.simplified().toHtmlEscaped();
            line.replace(openMark, QStringLiteral("<s>"))
                .replace(closeMark, QStringLiteral("</s>"))
                .replace(newOpen, QStringLiteral("<b>"))
                .replace(newClose, QStringLiteral("</b>"));
            lines.append(line);
        }
        list.append(QVariantMap {
            { QStringLiteral("id"), target.id },
            { QStringLiteral("title"), target.title },
            { QStringLiteral("count"), static_cast<int>(target.matches.size()) },
            { QStringLiteral("lines"), lines },
        });
    }
    return list;
}

int ProjectSession::replaceAll(const QString &query, const QString &replacement, bool regex)
{
    if (project.readOnly || !loadError.isEmpty())
        return -1;
    saveAll();
    int count = 0;
    for (const ReplaceTarget &target :
         replaceTargets(project, binderModel, replacePattern(query, regex))) {
        // The snapshot holds the text before the change, so the author can
        // go back. With no snapshot, the scene does not change.
        if (takeSnapshot(target.id, QStringLiteral("Before replace")).isEmpty())
            return -1;
        QString out;
        qsizetype at = 0;
        for (const QRegularExpressionMatch &match : target.matches) {
            out += target.text.mid(at, match.capturedStart() - at);
            out += expand(replacement, match, regex);
            at = match.capturedEnd();
        }
        out += target.text.mid(at);
        QSaveFile file(QDir(project.folder).filePath(target.file));
        const QByteArray bytes = out.toUtf8();
        if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit())
            return -1;
        changedSinceBackup = true;
        noteSaved(target.file);
        binderModel.setItemWords(target.id, Index::countWords(out));
        if (text.file() == target.file)
            text.reloadIfClean();
        for (const QPointer<Document> &document : std::as_const(tracked)) {
            if (document && document->file() == target.file)
                document->reloadIfClean();
        }
        count += static_cast<int>(target.matches.size());
    }
    emit wordsChanged();
    return count;
}

void ProjectSession::saveAll()
{
    text.save();
    for (const QPointer<Document> &document : std::as_const(tracked)) {
        if (document)
            document->save();
    }
}

QVariantMap ProjectSession::matchIn(const QString &id, const QString &query, bool regex) const
{
    qsizetype start = -1;
    qsizetype length = 0;
    const int at = indexOf(id);
    if (at >= 0 && !project.items.at(at).file.isEmpty()) {
        const QString body = readFile(QDir(project.folder).filePath(project.items.at(at).file));
        if (regex) {
            std::tie(start, length) = regexMatch(searchPattern(query), body);
        } else {
            start = Index::firstMatch(body, query);
            length = query.split(u' ', Qt::SkipEmptyParts).value(0).size();
        }
    }
    return { { QStringLiteral("start"), static_cast<int>(start) },
             { QStringLiteral("length"), static_cast<int>(start < 0 ? 0 : length) } };
}

int ProjectSession::firstMatch(const QString &id, const QString &query) const
{
    return matchIn(id, query, false).value(QStringLiteral("start")).toInt();
}
