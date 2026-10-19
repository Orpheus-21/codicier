#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

// The key map: one key sequence for each command, in keys.ini next to
// settings.ini. The author edits the file. A project reads it when it opens.
class KeyMap : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString file READ file CONSTANT)

public:
    explicit KeyMap(QObject *parent = nullptr);

    QString file() const { return path; }

    // Takes the commands as [{id, name, sequence}], with the default sequences.
    // Writes each command that the file does not have, with its default.
    // Returns {sequences: {id: sequence}, warnings: [text]}. A sequence that
    // is not valid, or that an earlier command has, becomes "".
    Q_INVOKABLE QVariantMap resolve(const QVariantList &commands) const;

private:
    QString path;
};
