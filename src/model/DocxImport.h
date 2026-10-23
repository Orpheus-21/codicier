#pragma once

#include "ProjectStore.h"

#include <QString>

// Makes a project from a DOCX file. The headings of the highest level in
// the file start the chapters; a paragraph of only "***", "* * *", or "#"
// starts a new scene. Bold and italic become Markdown. The Title and
// Subtitle paragraphs stay out. The file stays as it is.
namespace DocxImport {

QList<LoadError> importFile(const QString &docx, const QString &target, const QString &title);

} // namespace DocxImport
