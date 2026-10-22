#pragma once

#include "Index.h"
#include "MarkdownHighlighter.h"

#include <QColor>
#include <QObject>
#include <QPointer>
#include <QQuickTextDocument>
#include <QString>
#include <QTextBlock>
#include <QTextBlockFormat>
#include <QTextDocument>
#include <QTimer>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

// The open text file: it loads the file into the editor and saves it one
// second after the last edit. Each save replaces the file in one step. The
// session has one for the editor, and the composite view makes one for each
// scene.
class Document : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    // QML sets this to the textDocument of its TextEdit.
    Q_PROPERTY(QQuickTextDocument *textDocument READ textDocument WRITE setTextDocument NOTIFY
                   textDocumentChanged)
    // The path relative to the project folder, or "" when no text is open.
    Q_PROPERTY(QString file READ file NOTIFY fileChanged)
    Q_PROPERTY(bool dirty READ dirty NOTIFY dirtyChanged)
    // Another program changed the file while the app had unsaved text.
    Q_PROPERTY(bool conflict READ conflict NOTIFY conflictChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    // The theme colours for the Markdown marks and the footnote marks.
    Q_PROPERTY(QColor markColor READ markColor WRITE setMarkColor NOTIFY styleChanged)
    // The colors of the draft passes, in pass order.
    Q_PROPERTY(QVariantList passColors READ passColors WRITE setPassColors NOTIFY styleChanged)
    Q_PROPERTY(QColor accentColor READ accentColor WRITE setAccentColor NOTIFY styleChanged)
    // The space in pixels that the editor adds between two lines.
    Q_PROPERTY(qreal lineDistance READ lineDistance WRITE setLineDistance NOTIFY styleChanged)
    // The height in pixels of an empty paragraph, the blank line between two
    // Markdown paragraphs. Below 0, the empty paragraph keeps the normal line.
    Q_PROPERTY(qreal paragraphGap READ paragraphGap WRITE setParagraphGap NOTIFY styleChanged)
    // The indent in pixels of the first line of a paragraph, or 0. As in a
    // book, the first paragraph and a paragraph after a heading have none.
    Q_PROPERTY(
        qreal firstLineIndent READ firstLineIndent WRITE setFirstLineIndent NOTIFY styleChanged)
    // What stays bright in focus mode: 0 all the text, 1 the paragraph with
    // the cursor, 2 its sentence. The rest takes the mark colour.
    Q_PROPERTY(int focusScope READ focusScope WRITE setFocusScope NOTIFY styleChanged)
    // The words of the open text. It updates 250 ms after the last edit.
    Q_PROPERTY(int words READ words NOTIFY wordsChanged)

public:
    // The wait after the last edit before the save. The settings change it
    // for every document; a timer that runs takes the new value at its next
    // start.
    static inline int saveDelayMs = 1000;
    static constexpr int countDelayMs = 250;

    explicit Document(QObject *parent = nullptr);
    // Saves unsaved edits while the text still exists.
    ~Document() override;
    Document(const Document &) = delete;
    Document &operator=(const Document &) = delete;

    QQuickTextDocument *textDocument() const { return quickDocument; }
    void setTextDocument(QQuickTextDocument *document);
    // The same for a plain QTextDocument, as in tests.
    void attach(QTextDocument *document);

    QString file() const { return path; }
    bool dirty() const { return unsaved; }
    bool conflict() const { return inConflict; }
    QString error() const { return lastError; }
    // The text as the app holds it now.
    QString text() const;

    QColor markColor() const { return mark; }
    void setMarkColor(const QColor &color);
    QColor accentColor() const { return accent; }
    void setAccentColor(const QColor &color);
    qreal lineDistance() const { return distance; }
    int focusScope() const { return scopeOfFocus; }
    qreal paragraphGap() const { return gap; }
    qreal firstLineIndent() const { return indent; }
    void setFirstLineIndent(qreal value);
    void setParagraphGap(qreal value);
    void setFocusScope(int scope);
    int words() const { return wordCount; }
    void setLineDistance(qreal value);

    // Saves the open file if needed, then opens file. An empty file closes.
    Q_INVOKABLE void open(const QString &projectFolder, const QString &file);
    // Writes the text now. Returns false on an error or during a conflict.
    Q_INVOKABLE bool save();
    // The words in text, counted as the scene count counts them.
    Q_INVOKABLE static int wordsIn(const QString &text) { return Index::countWords(text); }
    // True while the document loads a text into the page. A cursor move
    // then comes from the load, not from the author.
    Q_INVOKABLE bool isLoading() const { return loading; }
    // Replaces the characters from start to end with text, as one step of
    // undo.
    Q_INVOKABLE void replace(int start, int end, const QString &text);
    // The checker for the underlines of unknown words, or null for none.
    void setSpellChecker(const SpellChecker *checker);
    // Checks the whole text again, after the checker learned new words.
    void recheckSpelling();
    // The style checks, as MarkdownHighlighter::StyleCheck flags.
    void setStyleChecks(int checks);
    // True hides the Markdown marks on the page. See MarkdownHighlighter.
    void setHideMarks(bool hide);
    // The names of the codex, which the page underlines.
    void setEntityNames(const QStringList &names);
    // The cursor of the page, or -1 when the page has no focus. Its
    // paragraph shows its Markdown marks.
    Q_INVOKABLE void cursorAt(int position);
    // The parts of the text that each draft pass wrote, for the tints.
    void setPasses(const QList<MarkdownHighlighter::PassRange> &ranges);
    QVariantList passColors() const { return colorsOfPasses; }
    void setPassColors(const QVariantList &colors);
    // The commented parts of the text, as a start and a length each.
    void setComments(const QList<std::pair<int, int>> &ranges);
    // Reads the file again, if there are no unsaved edits.
    void reloadIfClean();
    // The watcher saw an outside edit while the text was not saved.
    void markConflict();
    // The answers to the conflict. "Use the file" keeps a copy of the text
    // of the app in .codicier/recovered.
    Q_INVOKABLE void keepMine();
    Q_INVOKABLE void useFile();

signals:
    void textDocumentChanged();
    void fileChanged();
    void dirtyChanged();
    void conflictChanged();
    void errorChanged();
    void styleChanged();
    void wordsChanged();
    // The app wrote file, so its new content is not an outside edit.
    void saved(const QString &file);

private:
    QPointer<QQuickTextDocument> quickDocument;
    QPointer<QTextDocument> document;
    QPointer<MarkdownHighlighter> highlighter;
    const SpellChecker *spell = nullptr;
    int styleChecks = 0;
    bool hideMarks = false;
    QStringList entityNames;
    int cursorBlock = -1;
    int cursorOffset = 0;
    int scopeOfFocus = 0;
    QVariantList colorsOfPasses;
    QList<MarkdownHighlighter::PassRange> passes;
    QList<std::pair<int, int>> comments;
    QColor mark { 0x70, 0x6b, 0x65 };
    QColor accent { 0x94, 0x55, 0x2a };
    qreal distance = 0;
    qreal gap = -1;
    qreal indent = 0;
    QString folder;
    QString path;
    // The text of the file, kept until an editor is attached.
    QString loadedText;
    bool unsaved = false;
    // An editor was attached once. If its text is gone, a save writes nothing.
    bool attachedOnce = false;
    bool inConflict = false;
    bool loading = false;
    QString lastError;
    QTimer timer;
    QTimer countTimer;
    int wordCount = 0;

    void edited();
    void readFile();
    void showText();
    void setDirty(bool value);
    void applyStyle();
    // The block format of one paragraph: the line height, and for an empty
    // paragraph the gap.
    QTextBlockFormat formatOf(const QTextBlock &block) const;
    // The last saves of the text, oldest first, which come back as undo
    // steps when the text opens again. They live in .codicier/undo/.
    static constexpr qsizetype undoSaves = 20;
    QString undoFile() const;
    QStringList keptUndo() const;
    void keepUndo(const QString &before, const QString &after) const;
    // Changes the text to target as one undo step, with the smallest change.
    void replaceAll(const QString &target);
    // Styles the paragraphs that an edit touched, as part of that edit.
    void styleEdit(int position, int added);
    void countWords();
    void setError(const QString &text);
};
