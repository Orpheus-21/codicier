#pragma once

#include <QByteArray>
#include <QString>

// Turns the RTF that Scrivener writes (the Cocoa subset) into Markdown.
// Paragraphs, line breaks, bold, italic, Unicode, and code page 1252 bytes
// come through. Fonts, colours, pictures, and {\*...} groups drop out, a
// link keeps its text, and the style markers of Scrivener (<$Scr_...>) go.
namespace Rtf {

QString toMarkdown(const QByteArray &rtf);

} // namespace Rtf
