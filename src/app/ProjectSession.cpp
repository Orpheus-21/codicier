#include "ProjectSession.h"

#include "ProjectStore.h"

ProjectSession::ProjectSession(QObject *parent) : QObject(parent) { }

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
        project = Project();
        loadError.clear();
        binderModel.setProject(project);
        setCurrentId(QString());
        emit loaded();
        return;
    }
    LoadResult result = ProjectStore::load(folder);
    loadError = result.ok() ? QString() : result.errors.first().text();
    project = std::move(result.project);
    project.folder = folder;
    binderModel.setProject(project);
    setCurrentId(QString());
    emit loaded();
}

void ProjectSession::setCurrentId(const QString &id)
{
    if (id == current)
        return;
    current = id;
    emit currentIdChanged();
}
