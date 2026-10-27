#pragma once

#include <QAbstractListModel>
#include <QDateTime>
#include <QFileSystemWatcher>
#include <QList>
#include <QString>
#include <QUrl>
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
    // Copies the sample project into this workspace. Returns an error text, or "".
    Q_INVOKABLE QString createSample();
    // Makes a new project in the workspace from a Scrivener project folder
    // (.scriv), named after it, and opens it. The .scriv folder stays as it
    // is. Returns an error text, or "".
    Q_INVOKABLE QString importScrivener(const QUrl &scriv);
    // Makes a new project from a folder of Markdown files, named after the
    // folder, and opens it. Returns an error text, or "".
    Q_INVOKABLE QString importMarkdown(const QUrl &folder);
    // Makes a new project from a DOCX file, named after the file, and opens
    // it. Returns an error text, or "".
    Q_INVOKABLE QString importDocx(const QUrl &file);
    // The binder of a new project of the kind: title, kind, section, and
    // depth for each item, in binder order.
    Q_INVOKABLE static QVariantList templateOutline(const QString &kind);
    // When the newest backup of a project in the backup folder was made, as
    // "today at 14:05" or "3 March at 14:05", or "" with no backup.
    Q_INVOKABLE static QString lastBackup(const QString &projectFolder,
                                          const QString &backupFolder);
    // The zip backups of the project in the backup folder, newest first, as
    // maps with a path and a label (the date).
    Q_INVOKABLE static QVariantList backups(const QString &projectFolder,
                                            const QString &backupFolder);
    // Unpacks a backup as a new project in the workspace, with " (restored)"
    // after its title. The old project stays as it is. Returns an error text,
    // or "" and emits created.
    Q_INVOKABLE QString restoreBackup(const QString &zipPath);
    // Gives a project a new title and a folder name that matches the title.
    // The backups of the project move to the new name in backupFolder. A
    // project that another window holds open is refused. Returns an error
    // text, or "".
    Q_INVOKABLE QString rename(const QString &projectFolder, const QString &title,
                               const QString &backupFolder);
    // Copies a project, without its .codicier folder, to a new project with
    // " (copy)" after its title. Returns an error text, or "".
    Q_INVOKABLE QString duplicate(const QString &projectFolder);
    // Moves a project to the folder "archive" of the workspace. The home
    // screen no longer lists it. Returns an error text, or "".
    Q_INVOKABLE QString archive(const QString &projectFolder);
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
    // An error text if the folder is not a project directly in the
    // workspace, else "".
    QString checkProject(const QString &projectFolder) const;
    static Entry read(const QString &projectFolder);

signals:
    void folderChanged();
    void refreshed();
    // create() made a project in this folder.
    void created(const QString &folder);

private:
    // A new folder in the workspace for a project with the title, with a
    // readable name that no entry has.
    QString newFolderFor(const QString &title) const;
    // Runs an import into a new folder for title, then opens the project.
    // A failed import leaves no folder behind.
    template <typename Import> QString importInto(const QString &title, Import import);
    QString workspace;
    QList<Entry> entries;
    QFileSystemWatcher watcher;
};
