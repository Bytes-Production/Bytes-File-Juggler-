#include "UndoManager.h"

#include <QFileInfo>
#include <QDir>

#include <Windows.h>

namespace bytes::core {

using bytes::database::FileOperationRecord;

namespace {

QString restoredCollisionName(const QString &dir, const QString &fileName) {
    QDir d(dir);
    if (!d.exists(fileName))
        return fileName;

    const QFileInfo info(fileName);
    const QString base = info.completeBaseName();
    const QString suffix = info.suffix();

    QString candidate = suffix.isEmpty() ? QString("%1 (restored)").arg(base)
                                          : QString("%1 (restored).%2").arg(base, suffix);
    if (!d.exists(candidate))
        return candidate;

    for (int n = 2; n < 10000; ++n) {
        candidate = suffix.isEmpty() ? QString("%1 (restored %2)").arg(base).arg(n)
                                      : QString("%1 (restored %2).%3").arg(base).arg(n).arg(suffix);
        if (!d.exists(candidate))
            return candidate;
    }
    return candidate;
}

bool nativeMoveBack(const QString &from, const QString &to, QString &errorOut) {
    const std::wstring wfrom = QDir::toNativeSeparators(from).toStdWString();
    const std::wstring wto = QDir::toNativeSeparators(to).toStdWString();
    if (MoveFileExW(wfrom.c_str(), wto.c_str(), MOVEFILE_COPY_ALLOWED | MOVEFILE_WRITE_THROUGH))
        return true;
    const DWORD err = GetLastError();
    wchar_t buffer[512] = {};
    FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, err, 0, buffer, 511, nullptr);
    errorOut = QString::fromWCharArray(buffer).trimmed();
    if (errorOut.isEmpty())
        errorOut = QString("MoveFileExW failed with error %1").arg(err);
    return false;
}

} // namespace

UndoManager::UndoManager(database::Database &db) : m_db(db) {}

UndoOutcome UndoManager::restoreOne(const FileOperationRecord &record) {
    UndoOutcome outcome;
    outcome.operationId = record.id;

    if (record.status != "moved") {
        outcome.message = "Only successfully moved files can be undone";
        return outcome;
    }
    if (record.undone) {
        outcome.message = "This operation was already undone";
        return outcome;
    }

    QFileInfo currentLocation(record.destPath);
    if (!currentLocation.exists()) {
        outcome.message = "The moved file is no longer at its Bytes destination (may have been moved or deleted since)";
        return outcome;
    }

    const QFileInfo originalInfo(record.sourcePath);
    const QString originalDir = originalInfo.absolutePath();
    QDir().mkpath(originalDir);

    QString restoreName = originalInfo.fileName();
    QDir dir(originalDir);
    if (dir.exists(restoreName))
        restoreName = restoredCollisionName(originalDir, restoreName);

    const QString restorePath = QDir::toNativeSeparators(dir.filePath(restoreName));
    const qint64 expectedSize = currentLocation.size();

    QString error;
    if (!nativeMoveBack(record.destPath, restorePath, error)) {
        outcome.message = "Restore failed: " + error;
        return outcome;
    }

    QFileInfo verify(restorePath);
    if (!verify.exists() || verify.size() != expectedSize) {
        outcome.message = "Restore could not be verified";
        return outcome;
    }

    m_db.markOperationUndone(record.id);
    outcome.success = true;
    outcome.restoredPath = restorePath;
    outcome.message = "Restored successfully";
    return outcome;
}

UndoOutcome UndoManager::undoOperation(int operationId) {
    const auto record = m_db.fileOperation(operationId);
    if (!record) {
        UndoOutcome outcome;
        outcome.operationId = operationId;
        outcome.message = "Operation not found";
        return outcome;
    }
    return restoreOne(*record);
}

UndoOutcome UndoManager::undoLast() {
    const auto recent = m_db.recentFileOperations(50);
    for (const auto &record : recent) {
        if (record.status == "moved" && !record.undone)
            return restoreOne(record);
    }
    UndoOutcome outcome;
    outcome.message = "No recent move to undo";
    return outcome;
}

QVector<UndoOutcome> UndoManager::undoMultiple(const QVector<int> &operationIds) {
    QVector<UndoOutcome> results;
    results.reserve(operationIds.size());
    for (int id : operationIds)
        results.push_back(undoOperation(id));
    return results;
}

} // namespace bytes::core
