// Show a mock screen with the real Codicier module, or save it as a PNG.
// Usage: mockshot MOCK.qml [system|light|dark|sepia] [OUT.png]
// Without OUT.png the mock opens in a window.

#include <QGuiApplication>
#include <QQmlEngine>
#include <QQmlExtensionPlugin>
#include <QQuickView>

#include <cstdio>

Q_IMPORT_QML_PLUGIN(CodicierPlugin)

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    if (argc < 2) {
        std::fprintf(stderr, "usage: mockshot MOCK.qml [system|light|dark|sepia] [OUT.png]\n");
        return 2;
    }
    const QString theme = argc > 2 ? QString::fromLocal8Bit(argv[2]) : QStringLiteral("system");
    const QString out = argc > 3 ? QString::fromLocal8Bit(argv[3]) : QString();

    QQuickView view;
    view.setResizeMode(QQuickView::SizeRootObjectToView);
    auto *themeObject = view.engine()->singletonInstance<QObject *>("Codicier", "Theme");
    if (themeObject == nullptr) {
        std::fprintf(stderr, "mockshot: the Codicier module has no Theme\n");
        return 2;
    }
    themeObject->setProperty("choice", theme);
    view.setSource(QUrl::fromLocalFile(QString::fromLocal8Bit(argv[1])));
    if (view.status() != QQuickView::Ready) {
        for (const QQmlError &error : view.errors())
            std::fprintf(stderr, "mockshot: %s\n", qPrintable(error.toString()));
        return 2;
    }
    view.setTitle(QStringLiteral("codicier-mock"));
    view.show();
    if (out.isEmpty())
        return app.exec();

    // Save the first frame after the fonts and the layout are ready.
    QObject::connect(
        &view, &QQuickWindow::frameSwapped, &app,
        [&] { QCoreApplication::exit(view.grabWindow().save(out) ? 0 : 2); }, Qt::QueuedConnection);
    return app.exec();
}
