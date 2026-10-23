#pragma once

#include "ProjectStore.h"

#include <QList>
#include <QString>

// Makes projects from plain text: a folder of Markdown files, or the
// chapters that another import found, such as the DOCX import.
namespace MarkdownImport {

struct Scene
{
    QString title;
    // The Markdown of the scene.
    QString text;
};

// A chapter and its scenes. A chapter with no title puts its scenes at the
// top of the manuscript.
struct Chapter
{
    QString title;
    QList<Scene> scenes;
};

// Makes a new project in the empty folder target from the chapters.
QList<LoadError> build(const QString &target, const QString &title, const QList<Chapter> &chapters);

// Makes a project from a folder of Markdown files. Each .md file becomes a
// scene, in the natural order of the file names ("2" before "10"); each
// subfolder with .md files becomes a chapter. The title of a scene is its
// first "# " heading, which then leaves the text, else its file name. The
// source folder stays as it is.
QList<LoadError> importFolder(const QString &source, const QString &target, const QString &title);

} // namespace MarkdownImport
