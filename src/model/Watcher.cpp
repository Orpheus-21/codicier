#include "Watcher.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace {

// Editors often write a file in more than one step. Wait for the last one.
constexpr int settleMs = 100;

bool isProjectFile(const QString &file)
{
    return file == QLatin1String("project.toml") || file == QLatin1String("binder.toml");
}

} // namespace

Watcher::Watcher(const Project &project, QObject *parent) : QObject(parent)
{
    delay.setSingleShot(true);
    delay.setInterval(settleMs);
    connect(&delay, &QTimer::timeout, this, &Watcher::scan);
    connect(&watcher, &QFileSystemWatcher::fileChanged, &delay, qOverload<>(&QTimer::start));
    connect(&watcher, &QFileSystemWatcher::directoryChanged, &delay, qOverload<>(&QTimer::start));
    track(project);
}

void Watcher::track(const Project &project)
{
    folder = project.folder;
    known.clear();
    conflicted.clear();
    QStringList files { QStringLiteral("project.toml"), QStringLiteral("binder.toml") };
    for (const ProjectItem &item : project.items) {
        if (!item.file.isEmpty())
            files.append(item.file);
    }
    for (const QString &file : files)
        known.insert(file, stateOf(file));
    if (!watcher.files().isEmpty())
        watcher.removePaths(watcher.files());
    if (!watcher.directories().isEmpty())
        watcher.removePaths(watcher.directories());
    watchPaths();
}

void Watcher::setUnsaved(const QString &file, bool value)
{
    if (value)
        unsaved.insert(file);
    else
        unsaved.remove(file);
}

void Watcher::noteSaved(const QString &file)
{
    known.insert(file, stateOf(file));
    conflicted.remove(file);
    watchPaths();
}

void Watcher::resolve(const QString &file)
{
    conflicted.remove(file);
    // The text on disk is now known, so only a newer edit is a new conflict.
    known.insert(file, stateOf(file));
}

Watcher::State Watcher::stateOf(const QString &file, QByteArray *content) const
{
    State state;
    const QString path = QDir(folder).filePath(file);
    const QFileInfo info(path);
    if (!info.exists())
        return state;
    state.present = true;
    state.size = info.size();
    state.modified = info.lastModified();
    QFile in(path);
    if (in.open(QIODevice::ReadOnly)) {
        const QByteArray bytes = in.readAll();
        state.hash = QCryptographicHash::hash(bytes, QCryptographicHash::Sha256);
        if (content != nullptr)
            *content = bytes;
    }
    return state;
}

// A save by rename gives the file a new inode, and the watch on the old one
// ends. Watching the folders sees the rename, and this adds the new file.
void Watcher::watchPaths()
{
    const QDir dir(folder);
    QSet<QString> folders { dir.path() };
    QStringList files;
    for (auto it = known.cbegin(); it != known.cend(); ++it) {
        const QString path = dir.filePath(it.key());
        folders.insert(QFileInfo(path).path());
        if (QFileInfo::exists(path) && !watcher.files().contains(path))
            files.append(path);
    }
    for (const QString &path : folders) {
        if (QFileInfo::exists(path) && !watcher.directories().contains(path))
            files.append(path);
    }
    if (!files.isEmpty())
        watcher.addPaths(files);
}

void Watcher::scan()
{
    bool projectEdited = false;
    for (auto it = known.begin(); it != known.end(); ++it) {
        const QString &file = it.key();
        State &old = it.value();
        const QFileInfo info(QDir(folder).filePath(file));
        if (!info.exists()) {
            if (old.present) {
                old = State();
                emit fileRemoved(file);
            }
            continue;
        }
        // ponytail: size and time stamp decide if a file must be read; an edit
        // that keeps both within the time stamp resolution is missed.
        if (old.present && info.size() == old.size && info.lastModified() == old.modified)
            continue;
        QByteArray content;
        const State now = stateOf(file, &content);
        const bool same = old.present && now.hash == old.hash;
        old = now;
        if (same)
            continue;
        if (isProjectFile(file)) {
            projectEdited = true;
        } else if (unsaved.contains(file)) {
            if (!conflicted.contains(file)) {
                conflicted.insert(file);
                emit conflict(file, content);
            }
        } else {
            emit textChanged(file);
        }
    }
    if (projectEdited)
        emit projectChanged();
    watchPaths();
}
