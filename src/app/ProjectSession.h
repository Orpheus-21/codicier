#pragma once

#include "BinderModel.h"
#include "Document.h"
#include "Project.h"
#include "Watcher.h"

#include <QObject>
#include <QPointer>
#include <QString>
#include <QVariantMap>

#include <QtQml/qqmlregistration.h>
#include <memory>

// The open project. Set folder, and the session loads the project and fills
// the binder.
class ProjectSession : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString folder READ folder WRITE setFolder NOTIFY loaded)
    Q_PROPERTY(QString title READ title NOTIFY loaded)
    Q_PROPERTY(QString kind READ kind NOTIFY loaded)
    Q_PROPERTY(bool readOnly READ readOnly NOTIFY loaded)
    // The first load error, or "" when the project loaded.
    Q_PROPERTY(QString error READ error NOTIFY loaded)
    Q_PROPERTY(BinderModel *binder READ binder CONSTANT)
    // The text of the selected item, if it has one.
    Q_PROPERTY(Document *document READ document CONSTANT)
    // The id of the selected binder item, or "".
    Q_PROPERTY(QString currentId READ currentId WRITE setCurrentId NOTIFY currentIdChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY edited)
    // Live word counts for the status bar. chapterWords is -1 when the
    // selected item is not in a chapter.
    Q_PROPERTY(int sceneWords READ sceneWords NOTIFY wordsChanged)
    Q_PROPERTY(int chapterWords READ chapterWords NOTIFY wordsChanged)
    Q_PROPERTY(int manuscriptWords READ manuscriptWords NOTIFY wordsChanged)
    // The word goal from project.toml, or 0.
    Q_PROPERTY(int goalWords READ goalWords NOTIFY loaded)

public:
    explicit ProjectSession(QObject *parent = nullptr);
    ~ProjectSession() override;
    ProjectSession(const ProjectSession &) = delete;
    ProjectSession &operator=(const ProjectSession &) = delete;

    QString folder() const { return project.folder; }
    void setFolder(const QString &folder);
    QString title() const { return project.title; }
    QString kind() const { return project.kind; }
    bool readOnly() const { return project.readOnly; }
    QString error() const { return loadError; }
    BinderModel *binder() { return &binderModel; }
    Document *document() { return &text; }
    QString currentId() const { return current; }
    void setCurrentId(const QString &id);

    // Loads the project from disk again.
    Q_INVOKABLE void reload();

    // Adds a new item of kind ("scene", "chapter", "part", "folder", "page",
    // or "note") near the selected item, saves the binder, and selects the
    // new item. Returns its id, or "" with an error in lastError.
    Q_INVOKABLE QString add(const QString &kind);
    // Gives the item a new title and saves the binder.
    Q_INVOKABLE bool rename(const QString &id, const QString &title);
    // Moves the item and its branch to the trash. No file leaves the disk.
    Q_INVOKABLE bool moveToTrash(const QString &id);
    // Moves an item from the trash back to its old parent, or to the top of
    // its section if the old parent is gone.
    Q_INVOKABLE bool restore(const QString &id);
    // Moves the item before or after target, or inside target as its last
    // child ("before", "after", or "inside"). Returns false, and changes
    // nothing, for a move that the format does not allow.
    Q_INVOKABLE bool move(const QString &id, const QString &target, const QString &position);
    // True if move() with the same values would succeed.
    Q_INVOKABLE bool canMove(const QString &id, const QString &target,
                             const QString &position) const;
    // Moves the item one place up (-1) or down (+1) among its siblings.
    Q_INVOKABLE bool moveBy(const QString &id, int delta);
    // Splits the scene at a position in its text: the text after it goes to
    // a new scene right after this one. Returns the id of the new scene.
    Q_INVOKABLE QString split(const QString &id, int position);
    // Joins the next scene in the same parent to the end of this scene, and
    // moves the next scene to the trash with its file.
    Q_INVOKABLE bool mergeWithNext(const QString &id);
    // The id of the next scene in the same parent, or "".
    Q_INVOKABLE QString nextScene(const QString &id) const;
    // The inspector fields of an item: title, kind, status, pov, place,
    // time, synopsis, tags, target, notes, and words.
    Q_INVOKABLE QVariantMap fields(const QString &id) const;
    // Sets one field and saves the binder. An empty value removes the key.
    // Returns false for an unknown key or a value that is not valid.
    Q_INVOKABLE bool setField(const QString &id, const QString &key, const QVariant &value);
    // The text items below the item, in binder order, as maps with id,
    // title, and file. For the composite view.
    Q_INVOKABLE QVariantList textsUnder(const QString &id) const;
    // Connects a document of the composite view, for the item id, to the
    // watcher and to the live word counts, as the editor document is.
    Q_INVOKABLE void trackDocument(Document *document, const QString &id);
    // Saves the text of the item and copies it to snapshots/. Returns the
    // path of the snapshot, or "".
    Q_INVOKABLE QString takeSnapshot(const QString &id, const QString &title);
    // The snapshots of the item, newest first, as maps with path, time,
    // title, and label.
    Q_INVOKABLE QVariantList snapshots(const QString &id) const;
    // Writes a snapshot back to the item. The current text first becomes a
    // snapshot "Before restore".
    Q_INVOKABLE bool restoreSnapshot(const QString &id, const QString &path);
    // The last error of an edit, or "".
    QString lastError() const { return editError; }
    int sceneWords() const { return text.file().isEmpty() ? 0 : text.words(); }
    int chapterWords() const;
    int manuscriptWords() const { return binderModel.manuscriptWords(); }
    int goalWords() const;

signals:
    void loaded();
    void currentIdChanged();
    // The binder changed through an add, a rename, or a move.
    void edited();
    void wordsChanged();
    // A snapshot was taken or restored.
    void snapshotsChanged();

private:
    Project project;
    QString loadError;
    BinderModel binderModel;
    QString current;
    Document text;
    std::unique_ptr<Watcher> watcher;
    // The documents of the composite view.
    QList<QPointer<Document>> tracked;

    void watch();
    int indexOf(const QString &id) const;
    void place(int index, int parent, int position, const QString &section);
    void detach(int index);
    // Where a move would put the item: the parent (-1 for the top level),
    // the position, and the section. Returns false if the move is not valid.
    bool resolveMove(int index, int target, const QString &position, int *parent, int *at,
                     QString *section) const;
    void setSection(int index, const QString &section);
    bool commit(const QStringList &newFiles);
    QString editError;
};
