#include "SpellChecker.h"

#include <QDir>
#include <QFileInfo>

#include <hunspell.hxx>

QStringList &SpellChecker::searchPaths()
{
    // /app/share/hunspell holds the dictionary of the Flatpak.
    static QStringList paths { QStringLiteral("/usr/share/hunspell"),
                               QStringLiteral("/app/share/hunspell"),
                               QStringLiteral("/usr/share/myspell/dicts"),
                               QStringLiteral("/usr/share/myspell") };
    return paths;
}

SpellChecker::SpellChecker(const QString &language)
{
    QString wanted = language;
    wanted.replace(u'-', u'_');
    QStringList names { wanted, QStringLiteral("en_US"), QStringLiteral("en_GB") };
    for (const QString &path : std::as_const(searchPaths())) {
        for (const QString &file :
             QDir(path).entryList({ QStringLiteral("*.dic") }, QDir::Files, QDir::Name))
            names.append(QFileInfo(file).completeBaseName());
    }
    for (const QString &candidate : std::as_const(names)) {
        for (const QString &path : std::as_const(searchPaths())) {
            const QDir dir(path);
            const QString dic = dir.filePath(candidate + QStringLiteral(".dic"));
            const QString aff = dir.filePath(candidate + QStringLiteral(".aff"));
            if (!candidate.isEmpty() && QFileInfo::exists(dic) && QFileInfo::exists(aff)) {
                hunspell = std::make_unique<Hunspell>(aff.toLocal8Bit().constData(),
                                                      dic.toLocal8Bit().constData());
                name = candidate;
                return;
            }
        }
    }
}

SpellChecker::~SpellChecker() = default;

bool SpellChecker::isCorrect(const QString &word) const
{
    return !hunspell || hunspell->spell(word.toStdString());
}

QStringList SpellChecker::suggestions(const QString &word) const
{
    QStringList list;
    if (!hunspell)
        return list;
    for (const std::string &each : hunspell->suggest(word.toStdString()))
        list.append(QString::fromStdString(each));
    return list;
}

void SpellChecker::addWord(const QString &word)
{
    if (hunspell && !word.trimmed().isEmpty())
        hunspell->add(word.trimmed().toStdString());
}
