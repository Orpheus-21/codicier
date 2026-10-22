#include "Sources.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest>

class TestSources : public QObject
{
    Q_OBJECT

    static QJsonArray fileOf(const QTemporaryDir &dir)
    {
        QFile file(dir.filePath(QStringLiteral("sources.json")));
        if (!file.open(QIODevice::ReadOnly))
            return { };
        return QJsonDocument::fromJson(file.readAll()).array();
    }

private slots:
    void noFileIsNoSources()
    {
        QTemporaryDir dir;
        QString error;
        QVERIFY(Sources::list(dir.path(), &error).isEmpty());
        QVERIFY(error.isEmpty());
    }

    void saveWritesCslJson()
    {
        QTemporaryDir dir;
        QString id;
        const QVariantMap form { { "type", "book" },
                                 { "title", "Salt: A World History" },
                                 { "authors", "Kurlansky, Mark" },
                                 { "year", "2002" },
                                 { "publisher", "Walker" } };
        QCOMPARE(Sources::save(dir.path(), form, &id), QString());
        QCOMPARE(id, QStringLiteral("kurlansky2002"));
        const QJsonObject item = fileOf(dir).at(0).toObject();
        QCOMPARE(item.value("id").toString(), id);
        QCOMPARE(item.value("author").toArray().at(0).toObject().value("family").toString(),
                 QStringLiteral("Kurlansky"));
        QCOMPARE(item.value("issued")
                     .toObject()
                     .value("date-parts")
                     .toArray()
                     .at(0)
                     .toArray()
                     .at(0)
                     .toInt(),
                 2002);
        QVERIFY(!item.contains("page"));

        const QVariantMap back = Sources::list(dir.path()).at(0).toMap();
        QCOMPARE(back.value("authors").toString(), QStringLiteral("Kurlansky, Mark"));
        QCOMPARE(back.value("label").toString(),
                 QStringLiteral("Kurlansky 2002, Salt: A World History"));
    }

    void keysStayApartAndNamesCanBeLiteral()
    {
        QTemporaryDir dir;
        QString first;
        QString second;
        QString third;
        Sources::save(dir.path(), { { "authors", "Müller, Anna" }, { "year", "1999" } }, &first);
        Sources::save(dir.path(), { { "authors", "Müller, Bea" }, { "year", "1999" } }, &second);
        Sources::save(dir.path(), { { "authors", "World Health Organization" } }, &third);
        QCOMPARE(first, QStringLiteral("muller1999"));
        QCOMPARE(second, QStringLiteral("muller1999a"));
        QCOMPARE(third, QStringLiteral("worldhealthorganizat"));
        QCOMPARE(fileOf(dir)
                     .at(2)
                     .toObject()
                     .value("author")
                     .toArray()
                     .at(0)
                     .toObject()
                     .value("literal")
                     .toString(),
                 QStringLiteral("World Health Organization"));
    }

    void editKeepsOtherFields()
    {
        QTemporaryDir dir;
        QFile file(dir.filePath(QStringLiteral("sources.json")));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(R"([{"id": "k1", "type": "article-journal", "title": "Old", "volume": "12"}])");
        file.close();
        QCOMPARE(
            Sources::save(dir.path(),
                          { { "id", "k1" }, { "type", "article-journal" }, { "title", "New" } }),
            QString());
        const QJsonObject item = fileOf(dir).at(0).toObject();
        QCOMPARE(item.value("title").toString(), QStringLiteral("New"));
        QCOMPARE(item.value("volume").toString(), QStringLiteral("12"));
        QCOMPARE(fileOf(dir).size(), 1);
    }

    void brokenFileIsNeverOverwritten()
    {
        QTemporaryDir dir;
        QFile file(dir.filePath(QStringLiteral("sources.json")));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("{ not json");
        file.close();
        QString error;
        QVERIFY(Sources::list(dir.path(), &error).isEmpty());
        QVERIFY(!error.isEmpty());
        QVERIFY(!Sources::save(dir.path(), { { "title", "T" } }).isEmpty());
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), QByteArray("{ not json"));
    }

    void removeTakesOneSource()
    {
        QTemporaryDir dir;
        QString id;
        Sources::save(dir.path(), { { "title", "Gone" } }, &id);
        Sources::save(dir.path(), { { "title", "Kept" } });
        QCOMPARE(Sources::remove(dir.path(), id), QString());
        QCOMPARE(Sources::list(dir.path()).size(), 1);
        QVERIFY(!Sources::remove(dir.path(), QStringLiteral("nothing")).isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestSources)
#include "tst_sources.moc"
