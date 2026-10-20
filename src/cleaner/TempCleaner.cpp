#include "TempCleaner.h"
#include "../windows/ShellUtils.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QStandardPaths>
#include <QProcessEnvironment>
#include <algorithm>

namespace bytes::cleaner {

QString TempCleaner::userTempPath() {
    const QString temp = QProcessEnvironment::systemEnvironment().value("TEMP");
    if (!temp.isEmpty())
        return QDir::cleanPath(temp);
    return QDir::cleanPath(QStandardPaths::writableLocation(QStandardPaths::TempLocation));
}

QString TempCleaner::windowsTempPath() {
    const QString windir = QProcessEnvironment::systemEnvironment().value("WINDIR", "C:/Windows");
    return QDir::cleanPath(windir + "/Temp");
}

qint64 TempCleaner::calculateSize(const QString &path) {
    qint64 total = 0;
    QDirIterator it(path, QDir::Files | QDir::Hidden | QDir::System | QDir::NoSymLinks,
                     QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        total += it.fileInfo().size();
    }
    return total;
}

CleanupResult TempCleaner::clean(const QString &path, bool dryRun,
                                  const std::function<void(const QString &, bool, const QString &)> &onFile) const {
    CleanupResult result;

    // Absolute last line of defense: never operate on a user data folder,
    // regardless of what a caller passes in.
    if (bytes::windows::ShellUtils::isProtectedUserFolder(path))
        return result;

    QDir root(path);
    if (!root.exists())
        return result;

    QStringList directoriesToCheck;

    QDirIterator it(path, QDir::Files | QDir::Hidden | QDir::System | QDir::NoSymLinks,
                     QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString filePath = it.next();
        const QFileInfo info(filePath);
        const qint64 size = info.size();

        if (dryRun) {
            result.bytesFreed += size;
            result.filesDeleted += 1;
            continue;
        }

        QString error;
        if (bytes::windows::ShellUtils::deleteFilePermanently(filePath, error)) {
            result.bytesFreed += size;
            result.filesDeleted += 1;
            if (onFile)
                onFile(filePath, true, QString());
        } else {
            result.filesSkipped += 1;
            result.skippedReasons << QString("%1: %2").arg(info.fileName(), error);
            if (onFile)
                onFile(filePath, false, error);
            const QString dir = info.absolutePath();
            if (!directoriesToCheck.contains(dir))
                directoriesToCheck << dir;
        }
    }

    if (!dryRun) {
        // Remove now-empty subdirectories, deepest first, but never the
        // root temp directory itself.
        QStringList allDirs;
        QDirIterator dirIt(path, QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System,
                            QDirIterator::Subdirectories);
        while (dirIt.hasNext())
            allDirs << dirIt.next();

        std::sort(allDirs.begin(), allDirs.end(), [](const QString &a, const QString &b) {
            return a.length() > b.length(); // deepest paths first
        });

        for (const QString &dirPath : std::as_const(allDirs)) {
            QDir d(dirPath);
            if (QDir::cleanPath(dirPath) == QDir::cleanPath(path))
                continue;
            d.rmdir("."); // no-op if not empty; never recursive/force delete
        }
    }

    return result;
}

} // namespace bytes::cleaner
