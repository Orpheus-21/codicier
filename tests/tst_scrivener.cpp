#include "Scrivener.h"

#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

class TestScrivener : public QObject
{
    Q_OBJECT

    const QString fixture = QStringLiteral(FIXTURE_DIR "/scriv/Lighthouse.scriv");

private slots:
    void readsTheBinderTree()
    {
        QString error;
        const std::optional<Scrivener::Binder> binder = Scrivener::readBinder(fixture, &error);
        QVERIFY2(binder, qPrintable(error));
        QCOMPARE(binder->author, QStringLiteral("A. Writer"));
        QStringList types;
        for (const Scrivener::Item &root : binder->roots)
            types.append(root.type);
        QCOMPARE(types, (QStringList { "DraftFolder", "ResearchFolder", "Folder", "TrashFolder" }));

        const Scrivener::Item &part = binder->roots.at(0).children.at(0);
        QCOMPARE(part.title, QStringLiteral("Part One"));
        const Scrivener::Item &dusk = part.children.at(0);
        QCOMPARE(dusk.label, 1);
        QCOMPARE(dusk.status, 2);
        QCOMPARE(dusk.children.size(), 2);
        QCOMPARE(dusk.children.at(0).uuid, QStringLiteral("00000004-0000-4000-8000-000000000004"));
        QVERIFY(dusk.children.at(0).include);
        QVERIFY(!dusk.children.at(1).include);
        // A text with children, as Scrivener allows.
        const Scrivener::Item &morning = part.children.at(1);
        QCOMPARE(morning.type, QStringLiteral("Text"));
        QCOMPARE(morning.children.at(0).title, QStringLiteral("The log"));
        QCOMPARE(binder->roots.at(1).children.at(1).extension, QStringLiteral("png"));
    }

    void readsTheLabelsAndStatuses()
    {
        const std::optional<Scrivener::Binder> binder = Scrivener::readBinder(fixture);
        QVERIFY(binder);
        QCOMPARE(binder->labels.value(1).name, QStringLiteral("Plot"));
        QVERIFY(qAbs(binder->labels.value(2).red - 0.952941) < 1e-6);
        QVERIFY(binder->labels.value(-1).red < 0);
        QCOMPARE(binder->statuses.value(3), QStringLiteral("Revised Draft"));
        QCOMPARE(binder->statuses.size(), 6);
    }

    void unknownTypesStay()
    {
        QTemporaryDir dir;
        QFile file(dir.filePath("Odd.scrivx"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(
            "<ScrivenerProject Version=\"2.0\"><Binder>"
            "<BinderItem UUID=\"A\" Type=\"DraftFolder\"><Title>Draft</Title><Children>"
            "<BinderItem UUID=\"B\" Type=\"WebArchive\"><Title>Saved page</Title></BinderItem>"
            "</Children></BinderItem></Binder><Future><Thing ID=\"1\"/></Future>"
            "</ScrivenerProject>");
        file.close();
        const std::optional<Scrivener::Binder> binder = Scrivener::readBinder(dir.path());
        QVERIFY(binder);
        QCOMPARE(binder->roots.at(0).children.at(0).type, QStringLiteral("WebArchive"));
    }

    void brokenOrMissingBinderIsAnError()
    {
        QTemporaryDir dir;
        QString error;
        QVERIFY(!Scrivener::readBinder(dir.path(), &error));
        QVERIFY(error.contains("not a Scrivener project"));
        QFile file(dir.filePath("Broken.scrivx"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("<ScrivenerProject><Binder><BinderItem UUID=\"A\">");
        file.close();
        QVERIFY(!Scrivener::readBinder(dir.path(), &error));
        QVERIFY2(error.contains("broken"), qPrintable(error));
    }
};

QTEST_GUILESS_MAIN(TestScrivener)
#include "tst_scrivener.moc"
