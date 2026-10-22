#pragma once

#include <QDateTime>
#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

// The daily reminder to write: at a chosen time, once a day, while the app
// runs. On Linux it is a desktop notification over D-Bus; elsewhere the
// window asks for attention. It works offline.
class Reminder : public QObject
{
    Q_OBJECT
    QML_ELEMENT

public:
    explicit Reminder(QObject *parent = nullptr);

    // True when the reminder for time ("HH:mm") is due now and has not shown
    // today. lastShown is the date of the last reminder as yyyy-MM-dd.
    Q_INVOKABLE static bool isDue(const QString &time, const QString &lastShown,
                                  const QDateTime &now);
    // Sends a desktop notification. Returns false where the desktop has no
    // notification service; the caller then alerts the window.
    Q_INVOKABLE static bool notify(const QString &title, const QString &body);
};
