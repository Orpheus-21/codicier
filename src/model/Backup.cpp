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
#include <private/qzipwriter_p.h>

#include <algorithm>

namespace {

bool fail(QString *error, const QString &reason)
{
    if (error != nullptr)
        *error = reason;
    return false;
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

    // Keep the newest zips. The names sort by time.
    QStringList zips
        = target.entryList({ name + QStringLiteral("-*.zip") }, QDir::Files, QDir::Name);
    while (zips.size() > keep)
        QFile::remove(target.filePath(zips.takeFirst()));
    return target.filePath(zipName);
}

} // namespace Backup
