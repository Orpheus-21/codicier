#pragma once

#include <QObject>
#include <QPointer>
#include <QQuickTextDocument>
#include <QString>
#include <QTextDocument>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

// The open text file: it loads the file into the editor and saves it one
// second after the last edit. Each save replaces the file in one step.
class Document : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("A Document comes from a ProjectSession.")
    // QML sets this to the textDocument of its TextEdit.
    Q_PROPERTY(QQuickTextDocument *textDocument READ textDocument WRITE setTextDocument NOTIFY
                   textDocumentChanged)
    // The path relative to the project folder, or "" when no text is open.
    Q_PROPERTY(QString file READ file NOTIFY fileChanged)
    Q_PROPERTY(bool dirty READ dirty NOTIFY dirtyChanged)
    // Another program changed the file while the app had unsaved text.
    Q_PROPERTY(bool conflict READ conflict NOTIFY conflictChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)

public:
    static constexpr int saveDelayMs = 1000;

    explicit Document(QObject *parent = nullptr);

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

    // Saves the open file if needed, then opens file. An empty file closes.
    void open(const QString &projectFolder, const QString &file);
    // Writes the text now. Returns false on an error or during a conflict.
    Q_INVOKABLE bool save();
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
    // The app wrote file, so its new content is not an outside edit.
    void saved(const QString &file);

private:
    QPointer<QQuickTextDocument> quickDocument;
    QPointer<QTextDocument> document;
    QString folder;
    QString path;
    // The text of the file, kept until an editor is attached.
    QString loadedText;
    bool unsaved = false;
    bool inConflict = false;
    bool loading = false;
    QString lastError;
    QTimer timer;

    void edited();
    void readFile();
    void showText();
    void setDirty(bool value);
    void setError(const QString &text);
};
