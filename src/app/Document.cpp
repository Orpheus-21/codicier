#include "Document.h"

#include "Index.h"
#include "ProjectStore.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QQuickTextDocument>
#include <QSaveFile>
#include <QTextBlockFormat>
#include <QTextCursor>

namespace {

// A text file ends with one line feed. The editor shows the text without it.
QString fromFile(QString text)
{
    if (text.endsWith(u'\n'))
        text.chop(1);
    return text;
}

QByteArray toFile(QString text)
{
    if (!text.isEmpty() && !text.endsWith(u'\n'))
        text += u'\n';
    return text.toUtf8();
}

QString writeAtomically(const QString &path, const QByteArray &bytes)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit())
        return file.errorString();
    return { };
}

} // namespace

Document::Document(QObject *parent) : QObject(parent)
{
    timer.setSingleShot(true);
    connect(&timer, &QTimer::timeout, this, &Document::save);
    // ponytail: counts the whole text 250 ms after the last edit; count only
    // the changed paragraph if a very long scene makes this slow.
    countTimer.setSingleShot(true);
    countTimer.setInterval(countDelayMs);
    connect(&countTimer, &QTimer::timeout, this, &Document::countWords);
    journalTimer.setSingleShot(true);
    journalTimer.setInterval(200);
    connect(&journalTimer, &QTimer::timeout, this, &Document::writeJournal);
    // A normal quit saves the last edits.
    if (QCoreApplication::instance() != nullptr)
        connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, this,
                &Document::save);
}

Document::~Document()
{
    if (unsaved && document != nullptr)
        save();
}

void Document::setTextDocument(QQuickTextDocument *value)
{
    if (value == quickDocument)
        return;
    quickDocument = value;
    attach(value != nullptr ? value->textDocument() : nullptr);
    emit textDocumentChanged();
}

void Document::replace(int start, int end, const QString &text)
{
    if (document == nullptr)
        return;
    QTextCursor cursor(document);
    cursor.setPosition(start);
    cursor.setPosition(end, QTextCursor::KeepAnchor);
    cursor.insertText(text);
}

void Document::attach(QTextDocument *value)
{
    if (document != nullptr)
        disconnect(document, nullptr, this, nullptr);
    document = value;
    if (document == nullptr)
        return;
    attachedOnce = true;
    // The document owns the highlighter.
    highlighter = new MarkdownHighlighter(document);
    highlighter->setSpellChecker(spell);
    highlighter->setStyleChecks(styleChecks);
    highlighter->setHideMarks(hideMarks);
    highlighter->setNames(entityNames);
    highlighter->setShownBlock(cursorBlock, cursorOffset);
    highlighter->setFocusScope(scopeOfFocus);
    setPasses(passes);
    highlighter->setComments(comments);
    connect(document, &QTextDocument::contentsChanged, this, &Document::edited);
    connect(document, &QTextDocument::contentsChange, this, [this](int, int removed, int added) {
        if (loading || locked || path.isEmpty() || removed - added < largeDeletionChars
            || quietText.isEmpty() || quietText == lastKept)
            return;
        lastKept = quietText;
        emit largeDeletion(path, toFile(quietText));
    });
    // After the edit, so the format change does not run inside it.
    connect(
        document, &QTextDocument::contentsChange, this,
        [this, from = QPointer<QTextDocument>(document)](int position, int, int added) {
            if (from == document)
                styleEdit(position, added);
        },
        Qt::QueuedConnection);
    showText();
}

void Document::setSpellChecker(const SpellChecker *checker)
{
    spell = checker;
    if (highlighter != nullptr)
        highlighter->setSpellChecker(checker);
}

void Document::setPasses(const QList<MarkdownHighlighter::PassRange> &ranges)
{
    passes = ranges;
    if (highlighter == nullptr)
        return;
    QList<QColor> colors;
    for (const QVariant &color : std::as_const(colorsOfPasses))
        colors.append(color.value<QColor>());
    highlighter->setPasses(ranges, colors);
}

void Document::setChanges(const QList<std::pair<int, int>> &inserts, const QList<int> &deletions)
{
    if (highlighter != nullptr)
        highlighter->setChanges(inserts, deletions);
}

void Document::setComments(const QList<std::pair<int, int>> &ranges)
{
    comments = ranges;
    if (highlighter != nullptr)
        highlighter->setComments(ranges);
}

void Document::setPassColors(const QVariantList &colors)
{
    if (colors == colorsOfPasses)
        return;
    colorsOfPasses = colors;
    // A theme change paints the tints in the new colors.
    setPasses(passes);
    emit styleChanged();
}

void Document::cursorAt(int position)
{
    const QTextBlock block
        = position < 0 || document == nullptr ? QTextBlock() : document->findBlock(position);
    cursorBlock = block.isValid() ? block.blockNumber() : -1;
    cursorOffset = block.isValid() ? position - block.position() : 0;
    if (highlighter != nullptr)
        highlighter->setShownBlock(cursorBlock, cursorOffset);
}

void Document::setFocusScope(int scope)
{
    if (scope == scopeOfFocus)
        return;
    scopeOfFocus = scope;
    if (highlighter != nullptr)
        highlighter->setFocusScope(scope);
    emit styleChanged();
}

void Document::setEntityNames(const QStringList &names)
{
    entityNames = names;
    if (highlighter != nullptr)
        highlighter->setNames(names);
}

void Document::setHideMarks(bool hide)
{
    hideMarks = hide;
    if (highlighter != nullptr)
        highlighter->setHideMarks(hide);
}

void Document::setStyleChecks(int checks)
{
    styleChecks = checks;
    if (highlighter != nullptr)
        highlighter->setStyleChecks(checks);
}

void Document::recheckSpelling()
{
    if (highlighter != nullptr && spell != nullptr)
        highlighter->rehighlight();
}

void Document::setMarkColor(const QColor &color)
{
    if (color == mark)
        return;
    mark = color;
    applyStyle();
    emit styleChanged();
}

void Document::setAccentColor(const QColor &color)
{
    if (color == accent)
        return;
    accent = color;
    applyStyle();
    emit styleChanged();
}

QTextBlockFormat Document::formatOf(const QTextBlock &block) const
{
    QTextBlockFormat format = block.blockFormat();
    if (gap >= 0 && block.text().isEmpty())
        format.setLineHeight(gap, QTextBlockFormat::FixedHeight);
    else
        format.setLineHeight(distance, QTextBlockFormat::LineDistanceHeight);
    // The paragraph before, past the blank lines.
    QTextBlock before = block.previous();
    while (before.isValid() && before.text().isEmpty())
        before = before.previous();
    const bool indented = indent > 0 && !block.text().isEmpty() && before.isValid()
        && !before.text().startsWith(u'#') && !block.text().startsWith(u'#');
    format.setTextIndent(indented ? indent : 0);
    return format;
}

void Document::styleEdit(int position, int added)
{
    if (document == nullptr || loading || gap < 0)
        return;
    QTextCursor cursor(document);
    bool joined = false;
    for (QTextBlock block = document->findBlock(position);
         block.isValid() && block.position() <= position + added + 1; block = block.next()) {
        const QTextBlockFormat format = formatOf(block);
        if (format == block.blockFormat())
            continue;
        // The change joins the edit, so one undo takes back both.
        if (!joined) {
            cursor.joinPreviousEditBlock();
            joined = true;
        }
        cursor.setPosition(block.position());
        cursor.setBlockFormat(format);
    }
    if (joined)
        cursor.endEditBlock();
}

QString Document::undoFile() const
{
    QString name = path;
    name.replace(u'/', u'_');
    return QDir(folder).filePath(QStringLiteral(".codicier/undo/%1.json").arg(name));
}

QStringList Document::keptUndo() const
{
    QFile file(undoFile());
    if (!file.open(QIODevice::ReadOnly))
        return { };
    QStringList saves;
    const QJsonArray kept = QJsonDocument::fromJson(file.readAll()).array();
    for (const auto &each : kept)
        saves.append(each.toString());
    return saves;
}

void Document::keepUndo(const QString &before, const QString &after) const
{
    QStringList saves = keptUndo();
    // A list that does not end with the text before belongs to an older file.
    if (saves.isEmpty() || saves.last() != before)
        saves = { before };
    if (saves.last() != after)
        saves.append(after);
    while (saves.size() > undoSaves)
        saves.removeFirst();
    if (!QDir(folder).mkpath(QStringLiteral(".codicier/undo")))
        return;
    QSaveFile file(undoFile());
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(QJsonArray::fromStringList(saves)).toJson(QJsonDocument::Compact));
        file.commit();
    }
}

void Document::replaceAll(const QString &target)
{
    const QString current = document->toPlainText();
    qsizetype head = 0;
    while (head < current.size() && head < target.size() && current.at(head) == target.at(head))
        ++head;
    qsizetype tail = 0;
    while (tail < current.size() - head && tail < target.size() - head
           && current.at(current.size() - 1 - tail) == target.at(target.size() - 1 - tail))
        ++tail;
    QTextCursor cursor(document);
    cursor.setPosition(static_cast<int>(head));
    cursor.setPosition(static_cast<int>(current.size() - tail), QTextCursor::KeepAnchor);
    cursor.insertText(target.mid(head, target.size() - head - tail));
}

QString Document::journalPath() const
{
    if (path.isEmpty())
        return { };
    QString name = path;
    name.replace(u'/', u'_');
    return QDir(folder).filePath(QStringLiteral(".codicier/journal/") + name);
}

void Document::writeJournal()
{
    if (!unsaved || path.isEmpty() || document == nullptr
        || !QDir(folder).mkpath(QStringLiteral(".codicier/journal")))
        return;
    QSaveFile file(journalPath());
    if (file.open(QIODevice::WriteOnly)) {
        file.write(toFile(text()));
        file.commit();
    }
}

void Document::setFirstLineIndent(qreal value)
{
    if (qFuzzyCompare(value + 1, indent + 1))
        return;
    indent = value;
    applyStyle();
    emit styleChanged();
}

void Document::setParagraphGap(qreal value)
{
    if (qFuzzyCompare(value + 2, gap + 2))
        return;
    gap = value;
    applyStyle();
    emit styleChanged();
}

void Document::setLineDistance(qreal value)
{
    if (qFuzzyCompare(value, distance))
        return;
    distance = value;
    applyStyle();
    emit styleChanged();
}

// Applies the colours and the line distance. A style change is not an edit,
// so the modified state of the text stays as it was.
void Document::applyStyle()
{
    if (document == nullptr)
        return;
    if (highlighter != nullptr)
        highlighter->setColors(mark, accent);
    const bool modified = document->isModified();
    loading = true;
    QTextCursor cursor(document);
    for (QTextBlock block = document->begin(); block.isValid(); block = block.next()) {
        cursor.setPosition(block.position());
        cursor.setBlockFormat(formatOf(block));
    }
    loading = false;
    document->setModified(modified);
}

QString Document::text() const
{
    return document != nullptr ? document->toPlainText() : loadedText;
}

void Document::open(const QString &projectFolder, const QString &file)
{
    if (unsaved)
        save();
    timer.stop();
    folder = projectFolder;
    path = file;
    inConflict = false;
    setDirty(false);
    readFile();
    showText();
    countWords();
    emit fileChanged();
    // A journal that the file does not match is text from a run that crashed.
    QFile journal(journalPath());
    if (!path.isEmpty() && journal.open(QIODevice::ReadOnly)) {
        const QByteArray bytes = journal.readAll();
        journal.close();
        QFile::remove(journalPath());
        if (fromFile(QString::fromUtf8(bytes)) != loadedText)
            emit recovered(path, bytes);
    }
    emit conflictChanged();
}

void Document::readFile()
{
    loadedText.clear();
    setError(QString());
    if (path.isEmpty())
        return;
    QFile in(QDir(folder).filePath(path));
    if (!in.open(QIODevice::ReadOnly)) {
        setError(QStringLiteral("Cannot read %1.").arg(path));
        return;
    }
    loadedText = fromFile(QString::fromUtf8(in.readAll()));
}

void Document::showText()
{
    if (document == nullptr)
        return;
    loading = true;
    // The kept saves of the text come back as undo steps, when the last one
    // is still the text of the file.
    const QStringList saves = keptUndo();
    const bool replay = saves.size() > 1 && saves.last() == loadedText;
    document->setPlainText(replay ? saves.first() : loadedText);
    loading = false;
    applyStyle();
    // The style is no undo step. The loaded text, or the oldest kept save,
    // is the start.
    document->clearUndoRedoStacks();
    if (replay) {
        loading = true;
        for (qsizetype i = 1; i < saves.size(); ++i)
            replaceAll(saves.at(i));
        loading = false;
    }
    countWords();
    document->setModified(false);
}

void Document::edited()
{
    // Only a change to the text counts. The highlighter and the block format
    // change the document too, but they do not set it as modified.
    if (loading || locked || path.isEmpty() || (document != nullptr && !document->isModified()))
        return;
    setDirty(true);
    timer.start(saveDelayMs);
    journalTimer.start();
    countTimer.start();
}

void Document::countWords()
{
    quietText = path.isEmpty() ? QString() : text();
    const int count = path.isEmpty() ? 0 : Index::countWords(quietText);
    if (count == wordCount)
        return;
    wordCount = count;
    emit wordsChanged();
}

bool Document::save()
{
    timer.stop();
    if (path.isEmpty() || !unsaved)
        return true;
    // During a conflict the author decides first. The text stays in memory.
    if (inConflict)
        return false;
    // The editor text is gone, so no text is left that is newer than the
    // file. Writing the text of the last load could undo a later save.
    if (attachedOnce && document == nullptr)
        return false;
    const QString current = text();
    const QString error = writeAtomically(QDir(folder).filePath(path), toFile(current));
    if (!error.isEmpty()) {
        setError(QStringLiteral("Cannot save %1: %2").arg(path, error));
        return false;
    }
    setError(QString());
    journalTimer.stop();
    QFile::remove(journalPath());
    keepUndo(loadedText, current);
    // The saved text is now the text of the file.
    loadedText = current;
    setDirty(false);
    if (document != nullptr)
        document->setModified(false);
    emit saved(path);
    return true;
}

void Document::reloadIfClean()
{
    if (unsaved || path.isEmpty())
        return;
    readFile();
    showText();
}

void Document::markConflict()
{
    if (inConflict)
        return;
    inConflict = true;
    timer.stop();
    emit conflictChanged();
}

void Document::keepMine()
{
    if (!inConflict)
        return;
    inConflict = false;
    emit conflictChanged();
    setDirty(true);
    save();
}

void Document::useFile()
{
    if (!inConflict)
        return;
    // Keep the text of the app before the file replaces it.
    const QDir dir(folder);
    const QString name
        = QStringLiteral(".codicier/recovered/%1-%2.md")
              .arg(QFileInfo(path).completeBaseName(),
                   QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss")));
    if (!ProjectStore::makeAppFolder(folder).isEmpty()
        || !dir.mkpath(QStringLiteral(".codicier/recovered"))
        || !writeAtomically(dir.filePath(name), toFile(text())).isEmpty()) {
        setError(QStringLiteral("Cannot keep a copy of the text, so the file did not replace it."));
        return;
    }
    inConflict = false;
    emit conflictChanged();
    setDirty(false);
    readFile();
    showText();
    emit saved(path);
}

void Document::setDirty(bool value)
{
    if (value == unsaved)
        return;
    unsaved = value;
    emit dirtyChanged();
}

void Document::setError(const QString &text)
{
    if (text == lastError)
        return;
    lastError = text;
    emit errorChanged();
}
