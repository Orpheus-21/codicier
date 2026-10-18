// Input method check. It opens an empty spike editor and writes the editor
// text to OUT as it changes, so a person or a script can type into the window
// and then compare the file with the expected text.
// Usage: typecheck OUT

#include "MarkdownHighlighter.h"

#include <QFile>
#include <QGuiApplication>
#include <QQuickItem>
#include <QQuickTextDocument>
#include <QQuickView>
#include <QTimer>

#include <cstdio>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    if (argc < 2) {
        std::fprintf(stderr, "usage: typecheck OUT\n");
        return 2;
    }

    QQuickView view;
    view.setTitle(QStringLiteral("codicier-typecheck"));
    view.setResizeMode(QQuickView::SizeRootObjectToView);
    view.setInitialProperties({ { "fixture", QString() } });
    view.setSource(QUrl::fromLocalFile(QStringLiteral(PROBE_QML)));
    QQuickItem *root = view.rootObject();
    auto *editor = root != nullptr ? root->findChild<QQuickItem *>("editor") : nullptr;
    if (editor == nullptr) {
        std::fprintf(stderr, "typecheck: EditorProbe.qml did not load\n");
        return 2;
    }
    new MarkdownHighlighter(
        editor->property("textDocument").value<QQuickTextDocument *>()->textDocument());

    // ponytail: polls every 200 ms; a text signal is not needed for a manual check.
    QString written;
    QFile out(QString::fromLocal8Bit(argv[1]));
    QTimer poll;
    QObject::connect(&poll, &QTimer::timeout, &app, [&] {
        const QString text = editor->property("text").toString();
        if (text == written)
            return;
        written = text;
        if (out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            out.write(text.toUtf8());
            out.close();
        }
    });
    poll.start(200);

    std::printf("typecheck: input method module '%s'\n", qgetenv("QT_IM_MODULE").constData());
    std::fflush(stdout);
    view.show();
    return app.exec();
}
