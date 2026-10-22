#pragma once

#include <QHash>
#include <QObject>
#include <QStringList>
#include <QtQml/qqmlregistration.h>

// Makes names for characters from the name lists of data/names.tsv, which
// come from Wikidata. A real name joins a given name and a family name of one
// culture. An invented name follows the letter patterns of those lists. It
// works offline.
class NameGenerator : public QObject
{
    Q_OBJECT
    QML_ELEMENT

public:
    explicit NameGenerator(QObject *parent = nullptr);

    // The cultures of the lists, sorted.
    Q_INVOKABLE QStringList cultures() const;
    // count names of the culture. gender is "female", "male", or "any".
    // invented makes new names in the style of the culture.
    Q_INVOKABLE QStringList generate(const QString &culture, const QString &gender, int count,
                                     bool invented) const;

private:
    // The names of one culture and kind ("male", "female", "family").
    QHash<QString, QStringList> lists;
    QString pick(const QStringList &names) const;
    // A new name with the letter pairs of names, not one of names.
    static QString invent(const QStringList &names);
};
