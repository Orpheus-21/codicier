#include "OfflineGuard.h"

#include <QNetworkAccessManager>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlNetworkAccessManagerFactory>
#include <QTest>

#include <atomic>
#include <memory>

class TestOffline : public QObject
{
    Q_OBJECT

private slots:
    void localUrlsPass_data()
    {
        QTest::addColumn<QString>("url");
        QTest::newRow("file") << "file:///home/user/book/chapter.md";
        QTest::newRow("qrc") << "qrc:/qml/Main.qml";
        QTest::newRow("data") << "data:text/plain,hello";
        QTest::newRow("image provider") << "image://cover/1";
        QTest::newRow("relative") << "images/map.png";
    }

    void localUrlsPass()
    {
        QFETCH(QString, url);
        OfflineGuard guard;
        QCOMPARE(guard.intercept(QUrl(url), OfflineGuard::UrlString), QUrl(url));
    }

    void remoteUrlsBlock_data()
    {
        QTest::addColumn<QString>("url");
        QTest::newRow("http") << "http://example.com/a.png";
        QTest::newRow("https") << "https://example.com/a.qml";
        QTest::newRow("ftp") << "ftp://example.com/a.js";
    }

    void remoteUrlsBlock()
    {
        QFETCH(QString, url);
        OfflineGuard guard;
        const QUrl result = guard.intercept(QUrl(url), OfflineGuard::UrlString);
        QVERIFY(OfflineGuard::isLocal(result));
        QVERIFY(result != QUrl(url));
    }

    // With the guard, a remote image fails and the engine makes no network manager.
    void remoteImageMakesNoNetworkManager()
    {
        QQmlEngine engine;
        CountingFactory factory;
        engine.setNetworkAccessManagerFactory(&factory);
        OfflineGuard guard;
        engine.addUrlInterceptor(&guard);

        auto image = loadRemoteImage(engine);
        QTRY_COMPARE(image->property("status").toInt(), 3); // Image.Error
        QCOMPARE(factory.count, 0);
    }

    // Control: without the guard, the same image makes a network manager.
    // This proves that the counter works.
    void controlWithoutGuardMakesNetworkManager()
    {
        QQmlEngine engine;
        CountingFactory factory;
        engine.setNetworkAccessManagerFactory(&factory);

        auto image = loadRemoteImage(engine);
        QTRY_VERIFY(factory.count > 0);
    }

private:
    struct CountingFactory : QQmlNetworkAccessManagerFactory
    {
        std::atomic_int count = 0;
        QNetworkAccessManager *create(QObject *parent) override
        {
            ++count;
            return new QNetworkAccessManager(parent);
        }
    };

    // 192.0.2.1 is a documentation address (RFC 5737). No real server uses it.
    static std::unique_ptr<QObject> loadRemoteImage(QQmlEngine &engine)
    {
        QQmlComponent component(&engine);
        component.setData("import QtQuick\n"
                          "Image { source: \"https://192.0.2.1/a.png\" }",
                          QUrl());
        std::unique_ptr<QObject> image(component.create());
        if (!image)
            qFatal("%s", qPrintable(component.errorString()));
        return image;
    }
};

QTEST_MAIN(TestOffline)
#include "tst_offline.moc"
