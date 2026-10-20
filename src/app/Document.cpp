#include "Document.h"

#include "Index.h"
#include "ProjectStore.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
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
    setPasses(passes);
    connect(document, &QTextDocument::contentsChanged, this, &Document::edited);
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

void Document::setPassColors(const QVariantList &colors)
{
    if (colors == colorsOfPasses)
        return;
    colorsOfPasses = colors;
    // A theme change paints the tints in the new colors.
    setPasses(passes);
    emit styleChanged();
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
    QTextCursor all(document);
    all.select(QTextCursor::Document);
    QTextBlockFormat format;
    format.setLineHeight(distance, QTextBlockFormat::LineDistanceHeight);
    all.mergeBlockFormat(format);
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
    document->setPlainText(loadedText);
    loading = false;
    applyStyle();
    countWords();
    // The loaded text is the start: no undo goes back past it.
    document->clearUndoRedoStacks();
    document->setModified(false);
}

void Document::edited()
{
    // Only a change to the text counts. The highlighter and the block format
    // change the document too, but they do not set it as modified.
    if (loading || path.isEmpty() || (document != nullptr && !document->isModified()))
        return;
    setDirty(true);
    timer.start(saveDelayMs);
    countTimer.start();
}

void Document::countWords()
{
    const int count = path.isEmpty() ? 0 : Index::countWords(text());
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
