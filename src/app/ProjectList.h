#pragma once

#include <QAbstractListModel>
#include <QDateTime>
#include <QFileSystemWatcher>
#include <QList>
#include <QString>
#include <QtQml/qqmlregistration.h>

#include <cstdint>

// The projects in a workspace folder, newest edit first. Each subfolder with
// a project.toml is a project. A project that does not load stays in the list
// with its error.
class ProjectList : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString folder READ folder WRITE setFolder NOTIFY folderChanged)
    Q_PROPERTY(int totalWords READ totalWords NOTIFY refreshed)
    Q_PROPERTY(int count READ count NOTIFY refreshed)
    Q_PROPERTY(int fictionCount READ fictionCount NOTIFY refreshed)
    Q_PROPERTY(int nonFictionCount READ nonFictionCount NOTIFY refreshed)

public:
    enum Role : std::uint16_t {
        TitleRole = Qt::UserRole + 1,
        KindRole,
        KindLabelRole,
        FictionRole,
        WordsRole,
        GoalRole,
        EditedRole,
        EditedLabelRole,
        LastLineRole,
        FolderRole,
        ErrorRole,
        AuthorRole,
    };
    Q_ENUM(Role)

    struct Entry
    {
        QString title;
        QString kind;
        QString author;
        int words = 0;
        int goal = 0;
        QDateTime edited;
        QString lastLine;
        QString folder;
        QString error;
    };

    explicit ProjectList(QObject *parent = nullptr);

    QString folder() const { return workspace; }
    void setFolder(const QString &folder);
    int totalWords() const;
    int count() const { return static_cast<int>(entries.size()); }
    int fictionCount() const;
    int nonFictionCount() const { return count() - fictionCount(); }

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    // Reads the workspace again.
    Q_INVOKABLE void refresh();
    // Makes a new project from a template. Returns an error text, or "".
    Q_INVOKABLE QString create(const QString &title, const QString &kind, const QString &author);
    // The binder of a new project of the kind: title, kind, section, and
    // depth for each item, in binder order.
    Q_INVOKABLE static QVariantList templateOutline(const QString &kind);
    // Deletes a project for good: its folder and its backups in
    // backupFolder, with no copy in a trash. The folder must be a project
    // directly in the workspace. Returns an error text, or "".
    Q_INVOKABLE QString remove(const QString &projectFolder, const QString &backupFolder);
    // The values of row as a map, for the Continue block.
    Q_INVOKABLE QVariantMap get(int row) const;

    static QString kindLabel(const QString &kind);
    static bool isFiction(const QString &kind);
    // "Today", "Yesterday", "3 days ago", "2 weeks ago", or "March 2026".
    static QString editedLabel(const QDateTime &edited, const QDateTime &now);
    static Entry read(const QString &projectFolder);

signals:
    void folderChanged();
    void refreshed();
    // create() made a project in this folder.
    void created(const QString &folder);

private:
    QString workspace;
    QList<Entry> entries;
    QFileSystemWatcher watcher;
};
