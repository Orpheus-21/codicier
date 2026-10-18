#include "ProjectSession.h"

#include <QDir>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

class TestProjectSession : public QObject
{
    Q_OBJECT

private slots:
    void loadsTheProject()
    {
        ProjectSession session;
        QSignalSpy loaded(&session, &ProjectSession::loaded);
        session.setFolder(QStringLiteral(FIXTURE_DIR "/novel"));
        QCOMPARE(loaded.size(), 1);
        QCOMPARE(session.title(), QStringLiteral("The Salt Ledger"));
        QCOMPARE(session.kind(), QStringLiteral("novel"));
        QCOMPARE(session.error(), QString());
        QVERIFY(!session.readOnly());

        // The same folder again does not load again.
        session.setFolder(QStringLiteral(FIXTURE_DIR "/novel"));
        QCOMPARE(loaded.size(), 1);
    }

    void badFolderGivesAnError()
    {
        QTemporaryDir dir;
        ProjectSession session;
        session.setFolder(dir.path());
        QVERIFY(session.error().startsWith("project.toml:"));
        QCOMPARE(session.folder(), dir.path());
        QCOMPARE(session.title(), QString());
    }

    void emptyFolderClosesTheProject()
    {
        ProjectSession session;
        session.setFolder(QStringLiteral(FIXTURE_DIR "/novel"));
        QSignalSpy loaded(&session, &ProjectSession::loaded);
        session.setFolder(QString());
        QCOMPARE(loaded.size(), 1);
        QCOMPARE(session.title(), QString());
        QCOMPARE(session.error(), QString());
    }
};

QTEST_GUILESS_MAIN(TestProjectSession)
#include "tst_projectsession.moc"
