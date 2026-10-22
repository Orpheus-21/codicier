#include "NameGenerator.h"

#include <QFile>
#include <QRandomGenerator>
#include <QSet>

NameGenerator::NameGenerator(QObject *parent) : QObject(parent)
{
    QFile file(QStringLiteral(":/data/names.tsv"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return;
    for (const QString &line : QString::fromUtf8(file.readAll()).split(u'\n')) {
        const QStringList parts = line.split(u'\t');
        if (parts.size() == 3 && !line.startsWith(u'#'))
            lists[parts.at(0) + u'/' + parts.at(1)].append(parts.at(2));
    }
}

QStringList NameGenerator::cultures() const
{
    QSet<QString> found;
    for (auto it = lists.cbegin(); it != lists.cend(); ++it)
        found.insert(it.key().section(u'/', 0, 0));
    QStringList sorted(found.begin(), found.end());
    sorted.sort();
    return sorted;
}

QString NameGenerator::pick(const QStringList &names) const
{
    return names.isEmpty()
        ? QString()
        : names.at(static_cast<qsizetype>(QRandomGenerator::global()->bounded(names.size())));
}

QString NameGenerator::invent(const QStringList &names)
{
    // The letters that follow each pair of letters in the names. "^" marks
    // the start and "$" the end.
    QHash<QString, QString> next;
    for (const QString &name : names) {
        const QString word = QStringLiteral("^^") + name.toLower() + u'$';
        for (qsizetype i = 0; i + 2 < word.size(); ++i)
            next[word.mid(i, 2)].append(word.at(i + 2));
    }
    const QSet<QString> known(names.begin(), names.end());
    for (int attempt = 0; attempt < 50; ++attempt) {
        QString word = QStringLiteral("^^");
        while (word.size() < 14) {
            const QString choices = next.value(word.right(2));
            if (choices.isEmpty())
                break;
            const QChar letter = choices.at(
                static_cast<qsizetype>(QRandomGenerator::global()->bounded(choices.size())));
            if (letter == u'$')
                break;
            word.append(letter);
        }
        QString name = word.mid(2);
        if (name.size() < 3 || name.size() > 11)
            continue;
        name[0] = name.at(0).toUpper();
        if (!known.contains(name))
            return name;
    }
    return { };
}

QStringList NameGenerator::generate(const QString &culture, const QString &gender, int count,
                                    bool invented) const
{
    QStringList given = lists.value(culture + QStringLiteral("/female"));
    if (gender == QLatin1String("male"))
        given = lists.value(culture + QStringLiteral("/male"));
    else if (gender != QLatin1String("female"))
        given += lists.value(culture + QStringLiteral("/male"));
    const QStringList family = lists.value(culture + QStringLiteral("/family"));
    QStringList names;
    for (int attempt = 0; names.size() < count && attempt < count * 4; ++attempt) {
        const QString first = invented ? invent(given) : pick(given);
        const QString last = invented ? invent(family) : pick(family);
        const QString name = (first + u' ' + last).trimmed();
        if (!first.isEmpty() && !names.contains(name))
            names.append(name);
    }
    return names;
}
