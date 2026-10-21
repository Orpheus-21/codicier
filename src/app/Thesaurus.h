#pragma once

#include <QHash>
#include <QString>
#include <QStringList>

// Finds synonyms in a MyThes thesaurus from the computer: the th_*.idx and
// th_*.dat files of LibreOffice. It works offline and sends nothing.
class Thesaurus
{
public:
    // Loads the thesaurus for language, such as "en_US" or "en-US". With no
    // file for the language, it takes another one of the same language.
    explicit Thesaurus(const QString &language);

    // The folders with th_*.idx and th_*.dat files, in the order of search.
    // Tests set their own folder.
    static QStringList &searchPaths();

    // True when a thesaurus file for the language exists.
    bool isLoaded() const { return !data.isEmpty(); }
    // The synonyms of word, in the order of the file, with no repeats and
    // without the word itself.
    QStringList synonyms(const QString &word) const;

private:
    // Reads the index: the place of each word in the data file, from the
    // .idx file, or from the .dat file when no .idx file exists.
    void load() const;
    QString decode(const QByteArray &line) const;

    QString data;
    QString index;
    mutable bool loaded = false;
    mutable bool utf8 = true;
    mutable QHash<QString, qint64> offsets;
};
