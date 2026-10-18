// Editor speed probe. It opens a chapter in EditorProbe.qml with the Markdown
// highlighter, types keys into the editor, and logs the time from each key
// press to the next frame.
// Usage: probe FIXTURE.md [BUDGET_MS]
// Exit 0 if the p95 time is within the budget, 1 if not, 2 if the run failed.

#include "MarkdownHighlighter.h"

#include <QElapsedTimer>
#include <QFile>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QQuickItem>
#include <QQuickTextDocument>
#include <QQuickView>
#include <QSurfaceFormat>
#include <QTimer>

#include <algorithm>
#include <cstdio>
#include <string_view>
#include <vector>

namespace {

constexpr int warmupKeys = 20;
constexpr int measuredKeys = 300;
constexpr int deadlineMs = 60000;
constexpr std::u16string_view typed = u"the quiet river ";

double percentile(std::vector<double> sorted, double p)
{
    std::sort(sorted.begin(), sorted.end());
    const auto i = static_cast<size_t>(p * static_cast<double>(sorted.size() - 1));
    return sorted[i];
}

void sendKey(QWindow *window, QChar c)
{
    const int key = c == u' ' ? Qt::Key_Space : Qt::Key_A + (c.unicode() - u'a');
    QKeyEvent press(QEvent::KeyPress, key, Qt::NoModifier, QString(c));
    QKeyEvent release(QEvent::KeyRelease, key, Qt::NoModifier, QString(c));
    QGuiApplication::sendEvent(window, &press);
    QGuiApplication::sendEvent(window, &release);
}

} // namespace

int main(int argc, char *argv[])
{
    // The basic render loop renders on the GUI thread, so frameSwapped comes
    // to this thread and the time holds the event, the sync, and the render.
    // No vsync, so the time is the work and not the wait for the screen.
    // Without a display, Qt waits 5 ms before each update. Remove that wait.
    qputenv("QSG_RENDER_LOOP", "basic");
    qputenv("QT_QPA_UPDATE_IDLE_TIME", "0");
    QSurfaceFormat format = QSurfaceFormat::defaultFormat();
    format.setSwapInterval(0);
    QSurfaceFormat::setDefaultFormat(format);

    QGuiApplication app(argc, argv);
    if (argc < 2) {
        std::fprintf(stderr, "usage: probe FIXTURE.md [BUDGET_MS]\n");
        return 2;
    }
    const double budgetMs = argc > 2 ? QString::fromLocal8Bit(argv[2]).toDouble() : 16.0;

    QFile file(QString::fromLocal8Bit(argv[1]));
    if (!file.open(QIODevice::ReadOnly)) {
        std::fprintf(stderr, "probe: cannot read %s\n", argv[1]);
        return 2;
    }
    const QString fixture = QString::fromUtf8(file.readAll());

    QQuickView view;
    view.setInitialProperties({ { "fixture", fixture } });
    view.setSource(QUrl::fromLocalFile(QStringLiteral(PROBE_QML)));
    QQuickItem *root = view.rootObject();
    auto *editor = root != nullptr ? root->findChild<QQuickItem *>("editor") : nullptr;
    if (editor == nullptr) {
        std::fprintf(stderr, "probe: EditorProbe.qml did not load\n");
        return 2;
    }
    // The document owns the highlighter.
    new MarkdownHighlighter(
        editor->property("textDocument").value<QQuickTextDocument *>()->textDocument());

    QTimer::singleShot(deadlineMs, &app, [] {
        std::fprintf(stderr, "probe: no frame within %d ms\n", deadlineMs);
        QCoreApplication::exit(2);
    });

    QElapsedTimer sinceKey;
    std::vector<double> samples;
    int keysSent = 0;
    int startLength = 0;
    bool loaded = false;

    auto typeNext = [&] {
        sendKey(&view, QChar(typed[keysSent % typed.size()]));
        ++keysSent;
        sinceKey.start();
    };

    QObject::connect(&view, &QQuickWindow::frameSwapped, &app, [&] {
        if (!loaded) {
            // The first frame lays out the whole chapter. Start after it, when
            // the editor has focus: on Wayland the focus comes a frame later.
            if (!editor->hasActiveFocus())
                return;
            loaded = true;
            startLength = editor->property("length").toInt();
            QMetaObject::invokeMethod(root, "placeCursor", startLength / 2);
            QTimer::singleShot(0, &app, typeNext);
            return;
        }
        if (!sinceKey.isValid())
            return;
        const double ms = static_cast<double>(sinceKey.nsecsElapsed()) / 1e6;
        sinceKey.invalidate();
        if (keysSent > warmupKeys)
            samples.push_back(ms);
        if (keysSent < warmupKeys + measuredKeys)
            QTimer::singleShot(0, &app, typeNext);
        else
            QCoreApplication::exit(0);
    });

    view.show();
    if (app.exec() != 0)
        return 2;

    const int grown = editor->property("length").toInt() - startLength;
    if (grown != keysSent) {
        std::fprintf(stderr, "probe: sent %d keys but the text grew by %d\n", keysSent, grown);
        return 2;
    }

    const double p95 = percentile(samples, 0.95);
    std::printf("probe: %zu keys, %d chars, %s\n", samples.size(), startLength,
                qPrintable(QGuiApplication::platformName()));
    std::printf("probe: p50 %.2f ms, p95 %.2f ms, max %.2f ms, budget %.1f ms\n",
                percentile(samples, 0.5), p95, percentile(samples, 1.0), budgetMs);
    return p95 <= budgetMs ? 0 : 1;
}
