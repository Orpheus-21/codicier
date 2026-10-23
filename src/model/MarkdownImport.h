#pragma once

#include "ProjectStore.h"

#include <QString>

// Makes a project from a folder of Markdown files. Each .md file becomes a
// scene, in the natural order of the file names ("2" before "10"); each
// subfolder with .md files becomes a chapter. The title of a scene is its
// first "# " heading, which then leaves the text, else its file name. The
// source folder stays as it is.
namespace MarkdownImport {

QList<LoadError> importFolder(const QString &source, const QString &target, const QString &title);

} // namespace MarkdownImport
