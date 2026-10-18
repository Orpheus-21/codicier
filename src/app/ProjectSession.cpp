#include "ProjectSession.h"

#include "ProjectStore.h"

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
