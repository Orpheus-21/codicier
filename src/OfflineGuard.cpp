#include "OfflineGuard.h"

bool OfflineGuard::isLocal(const QUrl &url)
{
    const QString scheme = url.scheme();
    return scheme.isEmpty() || scheme == u"file" || scheme == u"qrc"
        || scheme == u"data" || scheme == u"image";
}

QUrl OfflineGuard::intercept(const QUrl &url, DataType)
{
    if (isLocal(url))
        return url;
    return QUrl(QStringLiteral("qrc:/offline-blocked"));
}
