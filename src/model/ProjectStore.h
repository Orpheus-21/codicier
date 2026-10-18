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

    static LoadResult load(const QString &folder);
};
