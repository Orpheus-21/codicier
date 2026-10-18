#pragma once

#include <QQmlAbstractUrlInterceptor>
#include <QUrl>

// Sends every non-local URL to a path that does not exist.
// The QML engine then never opens a network connection.
class OfflineGuard : public QQmlAbstractUrlInterceptor
{
public:
    static bool isLocal(const QUrl &url);

    QUrl intercept(const QUrl &url, DataType type) override;
};
