#include "Reminder.h"

#include <QTime>
#include <QVariantMap>

#ifdef QT_DBUS_LIB
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#endif

Reminder::Reminder(QObject *parent) : QObject(parent) { }

bool Reminder::isDue(const QString &time, const QString &lastShown, const QDateTime &now)
{
    const QTime at = QTime::fromString(time, QStringLiteral("HH:mm"));
    return at.isValid() && now.time() >= at && lastShown != now.date().toString(Qt::ISODate);
}

bool Reminder::notify(const QString &title, const QString &body)
{
#ifdef QT_DBUS_LIB
    QDBusInterface service(QStringLiteral("org.freedesktop.Notifications"),
                           QStringLiteral("/org/freedesktop/Notifications"),
                           QStringLiteral("org.freedesktop.Notifications"),
                           QDBusConnection::sessionBus());
    if (!service.isValid())
        return false;
    const QDBusReply<uint> reply
        = service.call(QStringLiteral("Notify"), QStringLiteral("Codicier"), uint(0),
                       QStringLiteral("codicier"), title, body, QStringList(), QVariantMap(), -1);
    return reply.isValid();
#else
    Q_UNUSED(title)
    Q_UNUSED(body)
    return false;
#endif
}
