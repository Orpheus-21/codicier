#include "Backup.h"

#include <QBuffer>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

// ponytail: QZipWriter is internal Qt API from QtCore. It ships with
// qt6-base, so it adds no package, but a Qt release can change it; CI
// builds with the newest Qt, so a change shows at once. A small zip writer
// of our own can replace it if that happens.
#include <private/qzipreader_p.h>
#include <private/qzipwriter_p.h>

#include <toml++/toml.hpp>

#include <algorithm>
#include <sstream>

namespace {

bool fail(QString *error, const QString &reason)
{
    if (error != nullptr)
        *error = reason;
    return false;
}

QByteArray read(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

// The created time in the text of a project.toml, or "" if there is none.
QString createdIn(const QByteArray &projectToml)
{
    try {
        const toml::table table
            = toml::parse(std::string_view(projectToml.constData(), projectToml.size()));
        if (const auto *created = table.get_as<toml::date_time>("created")) {
            std::ostringstream out;
            out << created->get();
            return QString::fromStdString(out.str());
        }
    } catch (const toml::parse_error &) {
        // A project.toml that does not parse has no created time.
        return { };
    }
    return { };
}

} // namespace

namespace Backup {

QString write(const QString &projectFolder, const QString &backupFolder, int keep, QString *error)
{
    const QDir project(projectFolder);
    if (!project.exists() || backupFolder.isEmpty() || keep < 1) {
        fail(error, QStringLiteral("No project or no backup folder."));
        return { };
    }
    const QString name = QFileInfo(project.absolutePath()).fileName();
    const QDir target(QDir(backupFolder).filePath(name));
    if (!target.mkpath(QStringLiteral("."))) {
        fail(error, QStringLiteral("Cannot make the backup folder."));
        return { };
    }

    // The zip builds in memory: QZipWriter closes its device at the end, and
    // QSaveFile must end with commit, not close. Then the bytes go through
    // QSaveFile, so no half zip ever has the final name.
    QDateTime time = QDateTime::currentDateTime();
    const auto nameAt = [&name](const QDateTime &at) {
        return name + u'-' + at.toString(QStringLiteral("yyyy-MM-ddTHH-mm-ss-zzz"))
            + QStringLiteral(".zip");
    };
    // A taken name moves on by one millisecond, so no backup replaces another.
    while (QFileInfo::exists(target.filePath(nameAt(time))))
        time = time.addMSecs(1);
    const QString zipName = nameAt(time);
    QByteArray bytes;
    {
        QBuffer buffer(&bytes);
        buffer.open(QIODevice::WriteOnly);
        QZipWriter zip(&buffer);
        zip.setCompressionPolicy(QZipWriter::AutoCompress);
        QDirIterator it(project.path(), QDir::Files | QDir::Hidden, QDirIterator::Subdirectories);
        QStringList files;
        while (it.hasNext())
            files.append(project.relativeFilePath(it.next()));
        std::ranges::sort(files);
        for (const QString &file : std::as_const(files)) {
            // .codicier/ holds only what the app can build again or lose.
            if (file.startsWith(QStringLiteral(".codicier/")))
                continue;
            QFile in(project.filePath(file));
            if (!in.open(QIODevice::ReadOnly)) {
                fail(error, QStringLiteral("Cannot read %1.").arg(file));
                return { };
            }
            zip.addFile(name + u'/' + file, in.readAll());
        }
        zip.close();
        if (zip.status() != QZipWriter::NoError) {
            fail(error, QStringLiteral("Cannot write the zip."));
            return { };
        }
    }
    QSaveFile out(target.filePath(zipName));
    if (!out.open(QIODevice::WriteOnly) || out.write(bytes) != bytes.size() || !out.commit()) {
        fail(error, out.errorString());
        return { };
    }

    // Keep the newest zips of this project. The names sort by time. A
    // project with the same folder name in another workspace shares the
    // folder, so only the zips with the same created time count.
    const QString created = createdIn(read(project.filePath(QStringLiteral("project.toml"))));
    QStringList zips;
    for (const QString &zip :
         target.entryList({ name + QStringLiteral("-*.zip") }, QDir::Files, QDir::Name)) {
        const QByteArray toml
            = QZipReader(target.filePath(zip)).fileData(name + QStringLiteral("/project.toml"));
        if (createdIn(toml) == created)
            zips.append(zip);
    }
    while (zips.size() > keep)
        QFile::remove(target.filePath(zips.takeFirst()));
    return target.filePath(zipName);
}

QDateTime latest(const QString &projectFolder, const QString &backupFolder)
{
    const QDir project(projectFolder);
    const QString name = QFileInfo(project.absolutePath()).fileName();
    const QDir target(QDir(backupFolder).filePath(name));
    if (backupFolder.isEmpty() || !target.exists())
        return { };
    const QString created = createdIn(read(project.filePath(QStringLiteral("project.toml"))));
    // The names sort by time, so the newest of this project is the first match
    // from the end.
    const QStringList zips = target.entryList({ name + QStringLiteral("-*.zip") }, QDir::Files,
                                              QDir::Name | QDir::Reversed);
    for (const QString &zip : zips) {
        const QByteArray toml
            = QZipReader(target.filePath(zip)).fileData(name + QStringLiteral("/project.toml"));
        if (createdIn(toml) != created)
            continue;
        const QString stamp = zip.mid(name.size() + 1).chopped(4);
        return QDateTime::fromString(stamp, QStringLiteral("yyyy-MM-ddTHH-mm-ss-zzz"));
    }
    return { };
}

int removeAll(const QString &projectFolder, const QString &backupFolder, QString *error)
{
    const QDir project(projectFolder);
    const QString created = createdIn(read(project.filePath(QStringLiteral("project.toml"))));
    if (created.isEmpty()) {
        fail(error, QStringLiteral("The project has no created time, so its backups are unknown."));
        return -1;
    }
    const QString name = QFileInfo(project.absolutePath()).fileName();
    QDir target(QDir(backupFolder).filePath(name));
    if (backupFolder.isEmpty() || !target.exists())
        return 0;
    int removed = 0;
    for (const QString &zip : target.entryList({ name + QStringLiteral("-*.zip") }, QDir::Files)) {
        const QString path = target.filePath(zip);
        const QByteArray toml = QZipReader(path).fileData(name + QStringLiteral("/project.toml"));
        if (createdIn(toml) != created)
            continue;
        if (!QFile::remove(path)) {
            fail(error, QStringLiteral("Cannot delete %1.").arg(path));
            return -1;
        }
        ++removed;
    }
    // rmdir removes only an empty folder.
    QDir(backupFolder).rmdir(name);
    return removed;
}

} // namespace Backup
