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

// The zips in the folder that belong to the project with the created time,
// newest first. The folder name at the top of a zip is the name that the
// project folder had when the zip was made, so a renamed project still finds
// its old zips. The time is the stamp at the end of the file name.
QList<std::pair<QString, QDateTime>> zipsOf(const QDir &target, const QString &created)
{
    QList<std::pair<QString, QDateTime>> found;
    for (const QString &zip : target.entryList({ QStringLiteral("*.zip") }, QDir::Files)) {
        const QString path = target.filePath(zip);
        QZipReader reader(path);
        const auto entries = reader.fileInfoList();
        if (entries.isEmpty())
            continue;
        const QString top = entries.first().filePath.section(u'/', 0, 0);
        if (createdIn(reader.fileData(top + QStringLiteral("/project.toml"))) != created)
            continue;
        found.append({ path,
                       QDateTime::fromString(zip.chopped(4).last(23),
                                             QStringLiteral("yyyy-MM-ddTHH-mm-ss-zzz")) });
    }
    std::ranges::sort(found, [](const auto &a, const auto &b) {
        return a.second != b.second ? a.second > b.second : a.first > b.first;
    });
    return found;
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
    const auto zips = zipsOf(target, created);
    for (qsizetype i = keep; i < zips.size(); ++i)
        QFile::remove(zips.at(i).first);
    return target.filePath(zipName);
}

QDateTime latest(const QString &projectFolder, const QString &backupFolder)
{
    const QList<Entry> entries = list(projectFolder, backupFolder);
    return entries.isEmpty() ? QDateTime() : entries.first().time;
}

QList<Entry> list(const QString &projectFolder, const QString &backupFolder)
{
    QList<Entry> entries;
    const QDir project(projectFolder);
    const QDir target(QDir(backupFolder).filePath(QFileInfo(project.absolutePath()).fileName()));
    if (backupFolder.isEmpty() || !target.exists())
        return entries;
    const QString created = createdIn(read(project.filePath(QStringLiteral("project.toml"))));
    for (const auto &[path, time] : zipsOf(target, created))
        entries.append({ path, time });
    return entries;
}

bool restore(const QString &zipPath, const QString &targetFolder, QString *error)
{
    QZipReader zip(zipPath);
    if (!zip.isReadable())
        return fail(error, QStringLiteral("Cannot read the backup."));
    if (QFileInfo::exists(targetFolder))
        return fail(error, QStringLiteral("The folder %1 exists already.").arg(targetFolder));
    const QDir target(targetFolder);
    if (!target.mkpath(QStringLiteral(".")))
        return fail(error, QStringLiteral("Cannot make the folder %1.").arg(targetFolder));
    bool hasProject = false;
    for (const QZipReader::FileInfo &info : zip.fileInfoList()) {
        // Drop the folder name at the top.
        const qsizetype slash = info.filePath.indexOf(u'/');
        if (slash < 0)
            continue;
        const QString relative = info.filePath.mid(slash + 1);
        if (relative.isEmpty())
            continue;
        // A path that leaves the folder is not in a backup of this app.
        if (relative.startsWith(u'/') || relative.split(u'/').contains(QStringLiteral("..")))
            return fail(error, QStringLiteral("The backup has an unsafe path: %1.").arg(relative));
        if (info.isDir) {
            target.mkpath(relative);
            continue;
        }
        if (!info.isFile)
            continue;
        target.mkpath(QFileInfo(relative).path());
        QSaveFile out(target.filePath(relative));
        const QByteArray bytes = zip.fileData(info.filePath);
        if (!out.open(QIODevice::WriteOnly) || out.write(bytes) != bytes.size() || !out.commit())
            return fail(error, QStringLiteral("Cannot write %1.").arg(relative));
        hasProject = hasProject || relative == QLatin1String("project.toml");
    }
    if (!hasProject)
        return fail(error, QStringLiteral("The zip is not a backup of a project."));
    return true;
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
    for (const auto &[path, time] : zipsOf(target, created)) {
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
