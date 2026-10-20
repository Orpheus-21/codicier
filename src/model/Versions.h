#pragma once

#include <QString>
#include <QStringList>

// The versions of a project in git, hidden from the author. The app runs
// the git program on the computer. Nothing leaves the computer: the app
// never runs a git command that uses the network.
class Versions
{
public:
    explicit Versions(QString projectFolder);

    // True if the git program is on the computer.
    static bool available();
    // True if a git repository holds the project folder.
    bool isOn() const;
    // Makes a repository in the project folder, unless one already holds
    // it, and saves a first version. Returns an error text, or "".
    QString turnOn(const QString &author);
    // Saves a version of the project folder with a message. A folder with no
    // change saves nothing. Returns an error text, or "".
    QString save(const QString &message, const QString &author);

private:
    QString folder;

    // Runs git in the project folder. Returns the exit code, and the output
    // in out.
    int git(const QStringList &arguments, QString *out = nullptr) const;
};
