#pragma once

#include "Project.h"

#include <QByteArray>
#include <QDateTime>
#include <QFileSystemWatcher>
#include <QHash>
#include <QObject>
#include <QSet>
#include <QTimer>

// Sees edits that another program makes to the files of a project. The
// watcher never writes a file. On a conflict the app keeps its own text and
// gets the text on disk, so no text is lost.
class Watcher : public QObject
{
    Q_OBJECT

public:
    explicit Watcher(const Project &project, QObject *parent = nullptr);

    // Watches the files of project. Call it again after a reload.
    void track(const Project &project);
    // The app has text for file that it did not save yet.
    void setUnsaved(const QString &file, bool unsaved);
    // The app saved file itself, so its new content is not an outside edit.
    // This also ends a conflict on the file.
    void noteSaved(const QString &file);
    // The author answered the prompt. A new outside edit gives a new conflict.
    void resolve(const QString &file);

signals:
    // project.toml or binder.toml changed. The app must load the project again.
    void projectChanged();
    // A text file with no unsaved changes changed. The app must read it again.
    void textChanged(const QString &file);
    // A text file with unsaved changes changed. This comes once per conflict.
    void conflict(const QString &file, const QByteArray &onDisk);
    // A file of the project is gone.
    void fileRemoved(const QString &file);

private:
    struct State
    {
        bool present = false;
        qint64 size = -1;
        QDateTime modified;
        QByteArray hash;
    };

    QString folder;
    QFileSystemWatcher watcher;
    QTimer delay;
    QHash<QString, State> known;
    QSet<QString> unsaved;
    QSet<QString> conflicted;

    State stateOf(const QString &file, QByteArray *content = nullptr) const;
    void watchPaths();
    void scan();
};
