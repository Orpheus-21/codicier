#pragma once

#include <QObject>
#include <QStringList>
#include <QUrl>
#include <QtQml/qqmlregistration.h>

// The workspace folders of the author. A workspace is a folder that holds
// project folders. The list lives in settings.ini in the app config folder.
class Workspaces : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(QString current READ current NOTIFY changed)
    Q_PROPERTY(QString currentName READ currentName NOTIFY changed)
    Q_PROPERTY(QStringList recent READ recent NOTIFY changed)

public:
    // The number of recent workspaces that the app keeps.
    static constexpr int recentLimit = 10;

    explicit Workspaces(QObject *parent = nullptr);

    static QString settingsPath();

    QString current() const { return currentFolder; }
    QString currentName() const { return nameOf(currentFolder); }
    QStringList recent() const { return recentFolders; }

    // Makes folder the current workspace. Returns an error text, or "".
    Q_INVOKABLE QString open(const QString &folder);
    // The same for a URL from a folder dialog.
    Q_INVOKABLE QString openUrl(const QUrl &url);
    // Removes folder from the recent list. The folder itself stays.
    Q_INVOKABLE void forget(const QString &folder);
    // The name to show for a folder: its last part.
    Q_INVOKABLE static QString nameOf(const QString &folder);

signals:
    void changed();

private:
    QString currentFolder;
    QStringList recentFolders;

    void store() const;
};
