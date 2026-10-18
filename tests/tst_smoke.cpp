#include "Workspaces.h"

#include <QFile>
#include <QQmlApplicationEngine>
#include <QQmlExtensionPlugin>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QTest>

Q_IMPORT_QML_PLUGIN(CodicierPlugin)

class TestSmoke : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        // The test must not read or write the settings of the author.
        QStandardPaths::setTestModeEnabled(true);
        QFile::remove(Workspaces::settingsPath());
    }

    void mainWindowLoads()
    {
        QQmlApplicationEngine engine;
        engine.loadFromModule("Codicier", "Main");
        QCOMPARE(engine.rootObjects().size(), 1);

        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        QVERIFY(window);
        QCOMPARE(window->title(), QStringLiteral("Codicier"));
    }
};

QTEST_MAIN(TestSmoke)
#include "tst_smoke.moc"
