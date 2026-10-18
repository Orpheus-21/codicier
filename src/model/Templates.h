#pragma once

#include "ProjectStore.h"

#include <QList>
#include <QString>

// Makes new projects with a starting binder for each kind of project.
namespace Templates {

// Makes a project of the given kind in folder. The folder must be missing or
// empty. Returns the errors, or an empty list.
QList<LoadError> create(const QString &folder, const QString &kind, const QString &title,
                        const QString &author);

} // namespace Templates
