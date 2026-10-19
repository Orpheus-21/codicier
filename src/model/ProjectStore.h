#pragma once

#include "Project.h"

#include <QList>
#include <QString>

// A problem in a project file. line is 0 if the problem has no one line.
struct LoadError
{
    QString file;
    int line = 0;
    QString message;

    // "binder.toml:12: the message"
    QString text() const;
};

struct LoadResult
{
    Project project;
    QList<LoadError> errors;

    bool ok() const { return errors.isEmpty(); }
};

// Reads a project folder in the format of docs/format.md.
class ProjectStore
{
public:
    // The format version that this app reads and writes.
    static constexpr int formatVersion = 1;

    // True for a kind of project in docs/format.md, such as "novel".
    static bool isProjectKind(const QString &kind);

    // Makes the .codicier folder of a project, with the .gitignore that keeps
    // it out of git. Returns an error text, or an empty string.
    static QString makeAppFolder(const QString &projectFolder);

    static LoadResult load(const QString &folder);

    // The bytes of project.toml and binder.toml in the canonical layout.
    static QByteArray projectFile(const Project &project);
    static QByteArray binderFile(const Project &project);

    // Writes project.toml and binder.toml. Each file goes to a temporary file
    // that then replaces the old file, so a file is always whole. A file with
    // no change is not written. Returns the errors, or an empty list.
    static QList<LoadError> save(const Project &project);
};
