#pragma once

#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

// The settings of the app that are not about one workspace, in the same
// settings.ini as the workspaces.
class Settings : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    // The folder for the zip backups of the projects.
    Q_PROPERTY(QString backupFolder READ backupFolder WRITE setBackupFolder NOTIFY changed)
    // The number of backups to keep for each project, 1 to 1000.
    Q_PROPERTY(int backupsToKeep READ backupsToKeep WRITE setBackupsToKeep NOTIFY changed)

public:
    static constexpr int defaultBackupsToKeep = 10;

    explicit Settings(QObject *parent = nullptr);

    static QString defaultBackupFolder();

    QString backupFolder() const { return folder; }
    void setBackupFolder(const QString &value);
    int backupsToKeep() const { return keep; }
    void setBackupsToKeep(int value);
    // The same for a URL from a folder dialog.
    Q_INVOKABLE void setBackupFolderUrl(const QUrl &url);

signals:
    void changed();

private:
    QString folder;
    int keep = defaultBackupsToKeep;

    void store() const;
};
