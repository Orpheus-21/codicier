#include "KeyMap.h"

#include <QDir>
#include <QKeySequence>
#include <QSettings>
#include <QStandardPaths>

KeyMap::KeyMap(QObject *parent)
    : QObject(parent),
      path(QDir(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation))
               .filePath(QStringLiteral("keys.ini")))
{ }

QVariantMap KeyMap::resolve(const QVariantList &commands) const
{
    QSettings settings(path, QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("keys"));
    QVariantMap sequences;
    QStringList warnings;
    // The name of the command that has each sequence, by its portable text.
    QHash<QString, QString> owners;
    for (const QVariant &each : commands) {
        const QVariantMap command = each.toMap();
        const QString id = command.value(QStringLiteral("id")).toString();
        const QString name = command.value(QStringLiteral("name"), id).toString();
        if (!settings.contains(id))
            settings.setValue(id, command.value(QStringLiteral("sequence")).toString());
        const QString text = settings.value(id).toString().trimmed();
        QString sequence;
        if (!text.isEmpty()) {
            const QKeySequence keys = QKeySequence::fromString(text, QKeySequence::PortableText);
            // An unknown key name gives an empty sequence or an unknown key.
            if (keys.isEmpty() || keys[0].key() == Qt::Key_unknown)
                warnings.append(
                    QStringLiteral("\"%1\" is not a valid key for %2.").arg(text, name));
            else
                sequence = keys.toString(QKeySequence::PortableText);
        }
        if (!sequence.isEmpty() && owners.contains(sequence)) {
            warnings.append(QStringLiteral("%1 is the key of %2 and %3. Only %2 uses it.")
                                .arg(sequence, owners.value(sequence), name));
            sequence.clear();
        }
        if (!sequence.isEmpty())
            owners.insert(sequence, name);
        sequences.insert(id, sequence);
    }
    settings.endGroup();
    return { { QStringLiteral("sequences"), sequences }, { QStringLiteral("warnings"), warnings } };
}
