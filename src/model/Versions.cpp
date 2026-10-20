#include "Versions.h"

#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>

#include <utility>

Versions::Versions(QString projectFolder) : folder(std::move(projectFolder)) { }

bool Versions::available()
{
    return !QStandardPaths::findExecutable(QStringLiteral("git")).isEmpty();
}

int Versions::git(const QStringList &arguments, QString *out) const
{
    QProcess process;
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    // git must never wait for a password or a terminal.
    environment.insert(QStringLiteral("GIT_TERMINAL_PROMPT"), QStringLiteral("0"));
    process.setProcessEnvironment(environment);
    process.start(QStringLiteral("git"), QStringList { QStringLiteral("-C"), folder } + arguments);
    if (!process.waitForFinished(30000))
        return -1;
    if (out != nullptr)
        *out = QString::fromUtf8(process.readAllStandardOutput()).trimmed()
            + QString::fromUtf8(process.readAllStandardError()).trimmed();
    return process.exitStatus() == QProcess::NormalExit ? process.exitCode() : -1;
}

QList<Versions::Entry> Versions::log(const QString &path, int limit) const
{
    QList<Entry> entries;
    QString out;
    // A unit separator between the fields, which no message holds.
    if (!isOn()
        || git({ QStringLiteral("log"), QStringLiteral("--max-count=%1").arg(limit),
                 QStringLiteral("--format=%H%x1f%cI%x1f%s"), QStringLiteral("--"),
                 path.isEmpty() ? QStringLiteral(".") : path },
               &out)
            != 0)
        return entries;
    for (const QString &line : out.split(u'\n', Qt::SkipEmptyParts)) {
        const QStringList fields = line.split(QChar(0x1f));
        if (fields.size() == 3)
            entries.append({ fields.at(0), fields.at(1), fields.at(2) });
    }
    return entries;
}

QString Versions::fileAt(const QString &hash, const QString &path, bool *ok) const
{
    QString out;
    // A hash has only hex digits, so no option can hide in it.
    static const QRegularExpression hex(QStringLiteral("^[0-9a-f]{7,40}$"));
    const bool found = hex.match(hash).hasMatch() && !path.contains(QStringLiteral(".."))
        && git({ QStringLiteral("show"), hash + QStringLiteral(":./") + path }, &out) == 0;
    if (ok != nullptr)
        *ok = found;
    return found ? out : QString();
}

QString Versions::restoreAll(const QString &hash)
{
    static const QRegularExpression hex(QStringLiteral("^[0-9a-f]{7,40}$"));
    if (!hex.match(hash).hasMatch())
        return QStringLiteral("This is not a version.");
    QString out;
    // restore with no overlay also removes the files that the version did not have.
    if (git({ QStringLiteral("restore"), QStringLiteral("--source=%1").arg(hash),
              QStringLiteral("--staged"), QStringLiteral("--worktree"), QStringLiteral("--"),
              QStringLiteral(".") },
            &out)
        != 0)
        return QStringLiteral("git could not restore the version: %1").arg(out);
    return { };
}

bool Versions::isOn() const
{
    QString out;
    return available()
        && git({ QStringLiteral("rev-parse"), QStringLiteral("--is-inside-work-tree") }, &out) == 0
        && out == QLatin1String("true");
}

QString Versions::turnOn(const QString &author)
{
    if (!available())
        return QStringLiteral("Versions need the git program. Install git, then try again.");
    QString out;
    if (!isOn() && git({ QStringLiteral("init"), QStringLiteral("--quiet") }, &out) != 0)
        return QStringLiteral("git could not make a repository: %1").arg(out);
    return save(QStringLiteral("Versions on"), author);
}

QString Versions::save(const QString &message, const QString &author)
{
    QString out;
    // Only the project folder: a repository can hold more than one project.
    if (git({ QStringLiteral("add"), QStringLiteral("--all"), QStringLiteral("--"),
              QStringLiteral(".") },
            &out)
        != 0)
        return QStringLiteral("git could not add the files: %1").arg(out);
    if (git({ QStringLiteral("diff"), QStringLiteral("--cached"), QStringLiteral("--quiet"),
              QStringLiteral("--"), QStringLiteral(".") })
        == 0)
        return { };
    // The name of the author of the project when git has no name. Never sign.
    QStringList options { QStringLiteral("-c"), QStringLiteral("commit.gpgsign=false") };
    QString name;
    git({ QStringLiteral("config"), QStringLiteral("user.name") }, &name);
    if (name.isEmpty()) {
        options << QStringLiteral("-c")
                << QStringLiteral("user.name=%1")
                       .arg(author.isEmpty() ? QStringLiteral("Author") : author)
                << QStringLiteral("-c") << QStringLiteral("user.email=author@localhost");
    }
    if (git(options
                + QStringList { QStringLiteral("commit"), QStringLiteral("--quiet"),
                                QStringLiteral("--message"), message, QStringLiteral("--"),
                                QStringLiteral(".") },
            &out)
        != 0)
        return QStringLiteral("git could not save the version: %1").arg(out);
    return { };
}
