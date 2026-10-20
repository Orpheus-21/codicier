#pragma once

#include <QString>
#include <QStringList>

#include <memory>

class Hunspell;

// Checks the spelling of words with a Hunspell dictionary from the
// computer. It works offline and sends nothing anywhere.
class SpellChecker
{
public:
    // Loads the dictionary for language, such as "en_GB" or "en-GB". With no
    // dictionary for the language, it tries en_US and en_GB, then any other.
    explicit SpellChecker(const QString &language);
    ~SpellChecker();
    SpellChecker(const SpellChecker &) = delete;
    SpellChecker &operator=(const SpellChecker &) = delete;

    // The folders with .dic and .aff files, in the order of search. Tests
    // set their own folder.
    static QStringList &searchPaths();

    // The name of the loaded dictionary, such as "en_GB", or "" for none.
    QString dictionary() const { return name; }
    // True for a known word, and for any word when no dictionary loaded.
    bool isCorrect(const QString &word) const;
    QStringList suggestions(const QString &word) const;
    // Adds a word for this run only, as for the project dictionary.
    void addWord(const QString &word);

private:
    std::unique_ptr<Hunspell> hunspell;
    QString name;
};
