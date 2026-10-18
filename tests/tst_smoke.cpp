#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QTest>

class TestSmoke : public QObject
{
    Q_OBJECT

private slots:
    void mainWindowLoads()
    {
        QQmlApplicationEngine engine;
        engine.load(QUrl::fromLocalFile(QStringLiteral(MAIN_QML)));
        QCOMPARE(engine.rootObjects().size(), 1);

        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        QVERIFY(window);
        QCOMPARE(window->title(), QStringLiteral("Codicier"));
    }
};

QTEST_MAIN(TestSmoke)
#include "tst_smoke.moc"
