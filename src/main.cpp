#include "OfflineGuard.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    QQmlApplicationEngine engine;
    OfflineGuard offlineGuard;
    engine.addUrlInterceptor(&offlineGuard);
    // Exit with an error if the QML fails to load.
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
        [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule("Codicier", "Main");

    return app.exec();
}
