#include "Document.h"
#include "ProjectSession.h"

#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTextBlock>
#include <QTextBlockFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextLayout>
#include <QThread>

namespace {

const char *scene = "manuscript/a-date-she-did-not-know.md";
const char *otherScene = "manuscript/the-river-office.md";

void copyFixture(const QString &dir)
{
    const QString source = QStringLiteral(FIXTURE_DIR "/novel");
    QDirIterator it(source, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString path = it.next();
        const QString target = dir + path.mid(source.size());
        QDir().mkpath(QFileInfo(target).path());
        QVERIFY(QFile::copy(path, target));
        QFile::setPermissions(target, QFile::ReadOwner | QFile::WriteOwner);
    }
}

QByteArray readFile(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

void writeFile(const QString &path, const QByteArray &bytes)
{
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(bytes);
}

void type(QTextDocument &text, const QString &words)
{
    QTextCursor cursor(&text);
    cursor.movePosition(QTextCursor::End);
    cursor.insertText(words);
}

} // namespace

class TestDocument : public QObject
{
    Q_OBJECT

    QTemporaryDir dir;

    QString path(const char *file) const { return dir.filePath(QString::fromUtf8(file)); }

private slots:
    void init()
    {
        QDir(dir.path()).removeRecursively();
        QDir().mkpath(dir.path());
        copyFixture(dir.path());
    }

    void opensWithoutTheLastLineFeed()
    {
        Document document;
        QTextDocument text;
        document.attach(&text);
        document.open(dir.path(), scene);
        QVERIFY(text.toPlainText().startsWith("The letter said"));
        QVERIFY(!text.toPlainText().endsWith('\n'));
        QVERIFY(!document.dirty());
        QCOMPARE(document.error(), QString());
    }

    void hiddenMarksReachTheHighlighter()
    {
        Document document;
        document.setHideMarks(true);
        QTextDocument text;
        // The highlighter runs only on a document with a layout.
        text.documentLayout();
        // The switch before the attach reaches the new highlighter.
        document.attach(&text);
        text.setPlainText(QStringLiteral("a **b**"));
        const QTextBlock block = text.firstBlock();
        const auto pixelsAt = [&block](int at) {
            for (const QTextLayout::FormatRange &range : block.layout()->formats()) {
                if (at >= range.start && at < range.start + range.length)
                    return range.format.property(QTextFormat::FontPixelSize).toInt();
            }
            return 0;
        };
        QCOMPARE(pixelsAt(2), 1);
        // The paragraph with the cursor shows its marks.
        document.cursorAt(3);
        QVERIFY(pixelsAt(2) != 1);
        document.cursorAt(-1);
        QCOMPARE(pixelsAt(2), 1);
        document.setHideMarks(false);
        QVERIFY(pixelsAt(2) != 1);
    }

    void emptyParagraphsTakeTheGap()
    {
        Document document;
        QTextDocument text;
        text.documentLayout();
        document.attach(&text);
        document.setLineDistance(4);
        document.setParagraphGap(12);
        text.setPlainText(QStringLiteral("One.\n\nTwo."));
        document.setParagraphGap(12.5);
        const QTextBlock blank = text.findBlockByNumber(1);
        QCOMPARE(blank.blockFormat().lineHeightType(), int(QTextBlockFormat::FixedHeight));
        QCOMPARE(blank.blockFormat().lineHeight(), 12.5);
        QCOMPARE(text.firstBlock().blockFormat().lineHeight(), 4.0);
        // A new blank paragraph gets the gap after the edit, in the same undo step.
        QTextCursor cursor(&text);
        cursor.movePosition(QTextCursor::End);
        cursor.insertText(QStringLiteral("\n"));
        QTRY_COMPARE(text.lastBlock().blockFormat().lineHeight(), 12.5);
        text.undo();
        QCOMPARE(text.toPlainText(), QStringLiteral("One.\n\nTwo."));
    }

    void savesOneSecondAfterTheLastEdit()
    {
        Document document;
        QTextDocument text;
        document.attach(&text);
        document.open(dir.path(), scene);
        const QByteArray before = readFile(path(scene));

        QElapsedTimer clock;
        clock.start();
        type(text, " More.");
        QVERIFY(document.dirty());
        // A new edit restarts the wait.
        QTest::qWait(600);
        type(text, " Again.");
        QTest::qWait(600);
        QCOMPARE(readFile(path(scene)), before);

        QTRY_VERIFY_WITH_TIMEOUT(!document.dirty(), 2000);
        QVERIFY(clock.elapsed() >= 1500);
        const QByteArray after = readFile(path(scene));
        QVERIFY(after.endsWith("The year is missing. More. Again.\n"));
    }

    void emptyTextSavesAnEmptyFile()
    {
        Document document;
        QTextDocument text;
        document.attach(&text);
        document.open(dir.path(), scene);
        text.clear();
        QVERIFY(document.save());
        QCOMPARE(readFile(path(scene)), QByteArray());
    }

    void openingAnotherFileSavesFirst()
    {
        Document document;
        QTextDocument text;
        document.attach(&text);
        document.open(dir.path(), scene);
        type(text, " Saved on switch.");
        document.open(dir.path(), otherScene);
        QVERIFY(readFile(path(scene)).endsWith("Saved on switch.\n"));
        QVERIFY(text.toPlainText().startsWith("The river office"));
        QVERIFY(!document.dirty());
    }

    void textWaitsForAnEditor()
    {
        Document document;
        document.open(dir.path(), otherScene);
        QVERIFY(document.text().startsWith("The river office"));
        QTextDocument text;
        document.attach(&text);
        QVERIFY(text.toPlainText().startsWith("The river office"));
        QVERIFY(!document.dirty());
    }

    void conflictWaitsForTheAuthor()
    {
        Document document;
        QTextDocument text;
        document.attach(&text);
        document.open(dir.path(), scene);
        type(text, " Mine.");
        document.markConflict();
        document.markConflict();
        QVERIFY(document.conflict());
        writeFile(path(scene), "Theirs.\n");
        // Autosave does not write during a conflict.
        QVERIFY(!document.save());
        QCOMPARE(readFile(path(scene)), QByteArray("Theirs.\n"));

        document.keepMine();
        QVERIFY(!document.conflict());
        QVERIFY(readFile(path(scene)).endsWith("Mine.\n"));
    }

    void useFileKeepsACopyOfMine()
    {
        Document document;
        QTextDocument text;
        document.attach(&text);
        document.open(dir.path(), scene);
        type(text, " Mine.");
        writeFile(path(scene), "Theirs.\n");
        document.markConflict();
        QSignalSpy saved(&document, &Document::saved);
        document.useFile();

        QCOMPARE(text.toPlainText(), QStringLiteral("Theirs."));
        QVERIFY(!document.dirty());
        QCOMPARE(saved.size(), 1);
        const QStringList copies = QDir(dir.filePath(".codicier/recovered")).entryList(QDir::Files);
        QCOMPARE(copies.size(), 1);
        QVERIFY(copies.first().startsWith("a-date-she-did-not-know-"));
        QVERIFY(
            readFile(dir.filePath(".codicier/recovered/" + copies.first())).endsWith("Mine.\n"));

        // With no conflict, the answers do nothing.
        document.useFile();
        document.keepMine();
        QCOMPARE(saved.size(), 1);
    }

    void reloadOnlyWhenClean()
    {
        Document document;
        QTextDocument text;
        document.attach(&text);
        document.open(dir.path(), scene);
        writeFile(path(scene), "Outside.\n");
        document.reloadIfClean();
        QCOMPARE(text.toPlainText(), QStringLiteral("Outside."));
        type(text, " Mine.");
        writeFile(path(scene), "Outside again.\n");
        document.reloadIfClean();
        QCOMPARE(text.toPlainText(), QStringLiteral("Outside. Mine."));
    }

    void saveErrorKeepsTheText()
    {
        Document document;
        QTextDocument text;
        document.attach(&text);
        document.open(dir.path(), scene);
        type(text, " Kept.");
        QVERIFY(QFile::remove(path(scene)));
        QVERIFY(QDir().mkdir(path(scene)));
        QVERIFY(!document.save());
        QVERIFY(document.error().startsWith("Cannot save"));
        QVERIFY(document.dirty());
        QVERIFY(text.toPlainText().endsWith("Kept."));
    }

    void missingFileShowsAnError()
    {
        Document document;
        document.open(dir.path(), "manuscript/gone.md");
        QVERIFY(document.error().startsWith("Cannot read"));
        document.open(dir.path(), QString());
        QCOMPARE(document.error(), QString());
        QVERIFY(document.save());
    }

    void styleChangeIsNotAnEdit()
    {
        Document document;
        QTextDocument text;
        document.attach(&text);
        document.open(dir.path(), scene);
        document.setMarkColor(QColor("#8f8a83"));
        document.setAccentColor(QColor("#d4935f"));
        document.setLineDistance(3);
        QVERIFY(!document.dirty());
        QTest::qWait(Document::saveDelayMs + 200);
        QVERIFY(!document.dirty());
        // A style change during unsaved edits keeps them unsaved.
        type(text, " Mine.");
        document.setLineDistance(4);
        QVERIFY(document.dirty());
        QVERIFY(text.isModified());
    }

    void lineDistanceReachesEveryParagraph()
    {
        Document document;
        QTextDocument text;
        document.attach(&text);
        document.setLineDistance(3);
        document.open(dir.path(), scene);
        type(text, "\nA new paragraph.");
        for (QTextBlock block = text.firstBlock(); block.isValid(); block = block.next()) {
            QCOMPARE(block.blockFormat().lineHeightType(),
                     int(QTextBlockFormat::LineDistanceHeight));
            QCOMPARE(block.blockFormat().lineHeight(), 3.0);
        }
        QCOMPARE(document.lineDistance(), 3.0);
        QCOMPARE(document.markColor(), QColor(0x70, 0x6b, 0x65));
    }

    void undoStopsAtTheLoadedText()
    {
        Document document;
        QTextDocument text;
        document.attach(&text);
        document.open(dir.path(), scene);
        QVERIFY(!text.isUndoAvailable());
    }

    void wordCountFollowsTheText()
    {
        Document document;
        QTextDocument text;
        document.attach(&text);
        document.open(dir.path(), scene);
        QCOMPARE(document.words(), 18);
        QSignalSpy counted(&document, &Document::wordsChanged);
        type(text, " Two more.");
        QCOMPARE(document.words(), 18);
        QTRY_COMPARE_WITH_TIMEOUT(document.words(), 20, 1000);
        QCOMPARE(counted.size(), 1);
        document.open(dir.path(), QString());
        QCOMPARE(document.words(), 0);
    }

    void sessionCountsTheSceneTheChapterAndTheBook()
    {
        ProjectSession session;
        session.setFolder(dir.path());
        QCOMPARE(session.goalWords(), 90000);
        QCOMPARE(session.manuscriptWords(), 28);
        session.setCurrentId("t6v4h9r2mz");
        QTextDocument text;
        session.document()->attach(&text);
        QCOMPARE(session.sceneWords(), 18);
        QCOMPARE(session.chapterWords(), 28);
        QSignalSpy changed(&session, &ProjectSession::wordsChanged);
        type(text, " Two more.");
        QTRY_COMPARE_WITH_TIMEOUT(session.sceneWords(), 20, 1000);
        QCOMPARE(session.chapterWords(), 30);
        QCOMPARE(session.manuscriptWords(), 30);
        QVERIFY(changed.size() >= 1);
        // A part is not in a chapter.
        session.setCurrentId("a1b2c3d4e5");
        QCOMPARE(session.chapterWords(), -1);
        QCOMPARE(session.sceneWords(), 0);
    }

    void goneEditorNeverWritesOldText()
    {
        Document document;
        {
            QTextDocument text;
            document.attach(&text);
            document.open(dir.path(), scene);
            type(text, " Saved once.");
            QVERIFY(document.save());
            type(text, " Then more.");
        }
        // The editor text is gone with unsaved edits. A save must not write the
        // text of the first load over the newer file.
        QVERIFY(!document.save());
        QVERIFY(readFile(path(scene)).endsWith("Saved once.\n"));
        QVERIFY(document.text().endsWith("Saved once."));
    }

    void deletedDocumentSavesItsEdits()
    {
        QTextDocument text;
        {
            Document document;
            document.attach(&text);
            document.open(dir.path(), scene);
            type(text, " Kept on delete.");
        }
        QVERIFY(readFile(path(scene)).endsWith("Kept on delete.\n"));
    }

    void sessionOpensTheSelectedText()
    {
        ProjectSession session;
        session.setFolder(dir.path());
        session.setCurrentId("t6v4h9r2mz");
        QCOMPARE(session.document()->file(), QString::fromUtf8(scene));
        // A chapter has no text, and an unknown id changes nothing.
        session.setCurrentId("f6g7h8j9k0");
        QCOMPARE(session.document()->file(), QString());
        session.setCurrentId("zzzzzzzzzz");
        QCOMPARE(session.currentId(), QStringLiteral("f6g7h8j9k0"));
    }

    void sessionReloadsAnOutsideEditAndFlagsAConflict()
    {
        ProjectSession session;
        session.setFolder(dir.path());
        QTextDocument text;
        session.document()->attach(&text);
        session.setCurrentId("t6v4h9r2mz");

        QThread::msleep(20);
        writeFile(path(scene), "Edited elsewhere.\n");
        QTRY_COMPARE_WITH_TIMEOUT(text.toPlainText(), QStringLiteral("Edited elsewhere."), 5000);

        type(text, " Mine.");
        QThread::msleep(20);
        writeFile(path(scene), "Edited again.\n");
        QTRY_VERIFY_WITH_TIMEOUT(session.document()->conflict(), 5000);
        QCOMPARE(text.toPlainText(), QStringLiteral("Edited elsewhere. Mine."));

        // The own save of the app is not an outside edit.
        session.document()->keepMine();
        QTest::qWait(500);
        QVERIFY(!session.document()->conflict());
        QCOMPARE(text.toPlainText(), QStringLiteral("Edited elsewhere. Mine."));
    }

    void trackedDocumentsGetOutsideEditsAndConflicts()
    {
        ProjectSession session;
        session.setFolder(dir.path());
        session.trackDocument(nullptr, "t6v4h9r2mz");
        Document document;
        QTextDocument text;
        document.attach(&text);
        document.open(dir.path(), scene);
        session.trackDocument(&document, "t6v4h9r2mz");

        QThread::msleep(20);
        writeFile(path(scene), "Edited elsewhere.\n");
        QTRY_COMPARE_WITH_TIMEOUT(text.toPlainText(), QStringLiteral("Edited elsewhere."), 5000);
        QTRY_COMPARE_WITH_TIMEOUT(session.manuscriptWords(), 12, 2000);

        type(text, " Mine.");
        QThread::msleep(20);
        writeFile(path(scene), "Edited again.\n");
        QTRY_VERIFY_WITH_TIMEOUT(document.conflict(), 5000);
        document.keepMine();
        QVERIFY(readFile(path(scene)).endsWith("Mine.\n"));
    }

    void sessionReloadsTheBinderAndKeepsTheSelection()
    {
        ProjectSession session;
        session.setFolder(dir.path());
        session.setCurrentId("t6v4h9r2mz");
        QSignalSpy loaded(&session, &ProjectSession::loaded);
        QByteArray binder = readFile(path("binder.toml"));
        binder.replace("title = \"Part Two\"", "title = \"Part Two: Flood\"");
        QThread::msleep(20);
        writeFile(path("binder.toml"), binder);
        QTRY_VERIFY_WITH_TIMEOUT(loaded.size() >= 1, 5000);
        QCOMPARE(session.currentId(), QStringLiteral("t6v4h9r2mz"));
        QCOMPARE(session.document()->file(), QString::fromUtf8(scene));
    }
};

QTEST_MAIN(TestDocument)
#include "tst_document.moc"
