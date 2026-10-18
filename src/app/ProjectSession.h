#pragma once

#include "BinderModel.h"
#include "Project.h"

#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

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
    // The id of the selected binder item, or "".
    Q_PROPERTY(QString currentId READ currentId WRITE setCurrentId NOTIFY currentIdChanged)

public:
    explicit ProjectSession(QObject *parent = nullptr);

    QString folder() const { return project.folder; }
    void setFolder(const QString &folder);
    QString title() const { return project.title; }
    QString kind() const { return project.kind; }
    bool readOnly() const { return project.readOnly; }
    QString error() const { return loadError; }
    BinderModel *binder() { return &binderModel; }
    QString currentId() const { return current; }
    void setCurrentId(const QString &id);

    // Loads the project from disk again.
    Q_INVOKABLE void reload();

signals:
    void loaded();
    void currentIdChanged();

private:
    Project project;
    QString loadError;
    BinderModel binderModel;
    QString current;
};
