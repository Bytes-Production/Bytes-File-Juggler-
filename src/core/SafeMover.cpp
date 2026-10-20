#include "SafeMover.h"

#include <QFileInfo>
#include <QDir>
#include <QThread>
#include <QDateTime>

#include <Windows.h>

namespace bytes::core {

QString SafeMover::toExtendedLengthPath(const QString &path) {
    QString native = QDir::toNativeSeparators(QFileInfo(path).absoluteFilePath());
    if (native.startsWith(R"(\\?\)"))
        return native;
    if (native.startsWith(R"(\\)")) // UNC path: \\server\share -> \\?\UNC\server\share
        return R"(\\?\UNC\)" + native.mid(2);
    if (native.length() >= MAX_PATH - 12)
        return R"(\\?\)" + native;
    return native;
}

bool SafeMover::isFileLocked(const QString &path) {
    const std::wstring wpath = toExtendedLengthPath(path).toStdWString();
    // Try to open with NO sharing for write/delete from other handles, but
    // allow reading -- if this fails with sharing violation, someone else
    // has the file open for writing (e.g. still downloading).
    HANDLE h = CreateFileW(wpath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE)
        return true;
    CloseHandle(h);
    return false;
}

bool SafeMover::isFileStable(const QString &path, int checkCount, int intervalMs) {
    if (checkCount < 1)
        checkCount = 1;

    qint64 lastSize = -1;
    qint64 lastModified = -1;

    for (int i = 0; i < checkCount; ++i) {
        QFileInfo info(path);
        info.refresh();
        if (!info.exists())
            return false;

        const qint64 size = info.size();
        const qint64 modified = info.lastModified().toMSecsSinceEpoch();

        if (isFileLocked(path))
            return false;

        if (i > 0 && (size != lastSize || modified != lastModified))
            return false; // still changing

        lastSize = size;
        lastModified = modified;

        if (i < checkCount - 1)
            QThread::msleep(static_cast<unsigned long>(qMax(0, intervalMs)));
    }
    return true;
}

bool SafeMover::nativeMove(const QString &fromPath, const QString &toPath, QString &errorOut) {
    const std::wstring wfrom = toExtendedLengthPath(fromPath).toStdWString();
    const std::wstring wto = toExtendedLengthPath(toPath).toStdWString();

    // MOVEFILE_COPY_ALLOWED handles cross-volume moves (e.g. custom
    // destination on a different drive). MOVEFILE_WRITE_THROUGH ensures the
    // move is flushed before we verify it. We deliberately do NOT pass
    // MOVEFILE_REPLACE_EXISTING -- Bytes never overwrites by default.
    const BOOL ok = MoveFileExW(wfrom.c_str(), wto.c_str(),
                                 MOVEFILE_COPY_ALLOWED | MOVEFILE_WRITE_THROUGH);
    if (!ok) {
        const DWORD err = GetLastError();
        wchar_t buffer[512] = {};
        FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, err,
                        0, buffer, 511, nullptr);
        errorOut = QString::fromWCharArray(buffer).trimmed();
        if (errorOut.isEmpty())
            errorOut = QString("MoveFileExW failed with error %1").arg(err);
        return false;
    }
    return true;
}

MoveResult SafeMover::moveFile(const MoveRequest &request) {
    MoveResult result;

    QFileInfo srcInfo(request.sourcePath);
    if (!srcInfo.exists() || !srcInfo.isFile()) {
        result.outcome = MoveOutcome::Error;
        result.errorMessage = "Source file no longer exists";
        return result;
    }

    QDir destDir(request.destinationDir);
    const QString fileName = srcInfo.fileName();
    QString finalName = fileName;

    const DuplicateResolution dup = DuplicateHandler::resolve(request.destinationDir, fileName, request.duplicatePolicy);
    if (dup.needsUserPrompt) {
        result.outcome = MoveOutcome::Error;
        result.needsUserPrompt = true;
        result.errorMessage = dup.reason;
        return result;
    }
    if (!dup.proceed) {
        result.outcome = MoveOutcome::Skipped;
        result.errorMessage = dup.reason;
        return result;
    }
    finalName = dup.finalFileName;

    const QString finalDestPath = QDir::toNativeSeparators(destDir.filePath(finalName));

    if (request.dryRun) {
        result.outcome = MoveOutcome::WouldMove;
        result.finalDestinationPath = finalDestPath;
        return result;
    }

    if (!destDir.exists() && !destDir.mkpath(".")) {
        result.outcome = MoveOutcome::Error;
        result.errorMessage = "Could not create destination folder";
        return result;
    }

    // Stability + lock re-check right before moving (belt and suspenders --
    // the caller already ran the grace-period stability check, but time may
    // have passed while queued behind other work).
    if (isFileLocked(request.sourcePath)) {
        result.outcome = MoveOutcome::Error;
        result.errorMessage = "File is in use by another process";
        return result;
    }

    const qint64 expectedSize = srcInfo.size();
    QString lastError;
    bool moved = false;

    const int attempts = qMax(1, request.retryCount);
    for (int attempt = 0; attempt < attempts; ++attempt) {
        if (nativeMove(request.sourcePath, finalDestPath, lastError)) {
            moved = true;
            break;
        }
        QThread::msleep(static_cast<unsigned long>(qMax(0, request.retryDelayMs)));
    }

    if (!moved) {
        result.outcome = MoveOutcome::Error;
        result.errorMessage = lastError.isEmpty() ? "Move failed after retries" : lastError;
        return result;
    }

    // Verify: destination must exist with the expected size, and the
    // source must be gone, before we call this a success.
    QFileInfo verifyDest(finalDestPath);
    QFileInfo verifySrc(request.sourcePath);
    if (!verifyDest.exists() || verifyDest.size() != expectedSize || verifySrc.exists()) {
        result.outcome = MoveOutcome::Error;
        result.errorMessage = "Move could not be verified; original left untouched if still present";
        return result;
    }

    result.outcome = MoveOutcome::Moved;
    result.finalDestinationPath = finalDestPath;
    return result;
}

} // namespace bytes::core
