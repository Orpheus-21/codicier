#pragma once

#include "BinderModel.h"
#include "Document.h"
#include "Project.h"
#include "Watcher.h"

#include <QObject>
#include <QString>

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
    // The last error of an edit, or "".
    QString lastError() const { return editError; }

signals:
    void loaded();
    void currentIdChanged();
    // The binder changed through an add, a rename, or a move.
    void edited();

private:
    Project project;
    QString loadError;
    BinderModel binderModel;
    QString current;
    Document text;
    std::unique_ptr<Watcher> watcher;

    void watch();
    int indexOf(const QString &id) const;
    void place(int index, int parent, int position, const QString &section);
    void detach(int index);
    void setSection(int index, const QString &section);
    bool commit(const QStringList &newFiles);
    QString editError;
};
