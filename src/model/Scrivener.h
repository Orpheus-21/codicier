#pragma once

#include <QHash>
#include <QList>
#include <QString>

#include <optional>

// Reads a Scrivener 3 project: a .scriv folder with <Name>.scrivx (the
// binder as XML) and Files/Data/<UUID>/ (content.rtf, synopsis.txt,
// notes.rtf, or content.<ext> for media). The reader never writes into the
// .scriv folder. No official schema exists, so the reader accepts what it
// does not know: an unknown item type is kept, and the label and status
// lists are any elements with an ID attribute.
namespace Scrivener {

struct Item
{
    QString uuid;
    // DraftFolder, ResearchFolder, TrashFolder, Folder, Text, Image, and others.
    QString type;
    QString title;
    bool include = true;
    int label = -1;
    int status = -1;
    // The extension of content.<ext> for a media item, such as "png".
    QString extension;
    QList<Item> children;
};

// A label of the project: its name and its colour as red, green, and blue
// from 0 to 1, or a negative red for no colour.
struct Label
{
    QString name;
    double red = -1;
    double green = 0;
    double blue = 0;
};

struct Binder
{
    QList<Item> roots;
    QHash<int, Label> labels;
    QHash<int, QString> statuses;
    QString author;
};

// Reads the binder of the .scriv folder. Returns nothing and sets error
// when the folder has no .scrivx or the XML is broken.
std::optional<Binder> readBinder(const QString &folder, QString *error = nullptr);

} // namespace Scrivener
