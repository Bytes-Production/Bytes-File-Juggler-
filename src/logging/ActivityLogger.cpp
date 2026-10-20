#include "ActivityLogger.h"

#include <QFileInfo>
#include <QDate>
#include <QDateTime>

namespace bytes::logging {

using namespace bytes::database;

ActivityLogger::ActivityLogger(database::Database &db, MarkdownLogger &markdownLogger, QObject *parent)
    : QObject(parent), m_db(db), m_markdown(markdownLogger) {}

QString ActivityLogger::today() { return QDate::currentDate().toString("yyyy-MM-dd"); }

int ActivityLogger::logMove(const QString &sourcePath, const QString &destPath, const QString &fileType, const QString &ruleName) {
    FileOperationRecord record;
    record.timestamp = QDateTime::currentDateTime();
    record.sourcePath = sourcePath;
    record.destPath = destPath;
    record.fileType = fileType;
    record.ruleName = ruleName;
    record.status = "moved";

    const int id = m_db.recordFileOperation(record);
    record.id = id;

    m_db.bumpStatistic(today(), "files_organized", 1);
    m_markdown.logFileMoved(QFileInfo(sourcePath).fileName(), fileType, sourcePath, destPath, ruleName);

    emit operationLogged(record);
    return id;
}

int ActivityLogger::logSkip(const QString &sourcePath, const QString &fileType, const QString &ruleName, const QString &reason) {
    FileOperationRecord record;
    record.timestamp = QDateTime::currentDateTime();
    record.sourcePath = sourcePath;
    record.fileType = fileType;
    record.ruleName = ruleName;
    record.status = "skipped";
    record.errorMessage = reason;

    const int id = m_db.recordFileOperation(record);
    record.id = id;

    m_db.bumpStatistic(today(), "files_skipped", 1);
    m_markdown.logFileSkipped(QFileInfo(sourcePath).fileName(), sourcePath, reason);

    emit operationLogged(record);
    return id;
}

int ActivityLogger::logWouldMove(const QString &sourcePath, const QString &destPath, const QString &fileType, const QString &ruleName) {
    FileOperationRecord record;
    record.timestamp = QDateTime::currentDateTime();
    record.sourcePath = sourcePath;
    record.destPath = destPath;
    record.fileType = fileType;
    record.ruleName = ruleName;
    record.status = "would_move";

    const int id = m_db.recordFileOperation(record);
    record.id = id;

    m_markdown.logFileWouldMove(QFileInfo(sourcePath).fileName(), sourcePath, destPath, ruleName);

    emit operationLogged(record);
    return id;
}

int ActivityLogger::logError(const QString &sourcePath, const QString &context, const QString &reason) {
    FileOperationRecord record;
    record.timestamp = QDateTime::currentDateTime();
    record.sourcePath = sourcePath;
    record.status = "error";
    record.errorMessage = reason;
    const int opId = m_db.recordFileOperation(record);
    record.id = opId;

    ErrorRecord err;
    err.timestamp = record.timestamp;
    err.context = context;
    err.filePath = sourcePath;
    err.message = reason;
    err.id = m_db.recordError(err);

    m_db.bumpStatistic(today(), "errors", 1);
    m_markdown.logError(QFileInfo(sourcePath).fileName(), context, reason);

    emit operationLogged(record);
    emit errorLogged(err);
    return opId;
}

void ActivityLogger::logCleanup(const QString &target, qint64 bytesFreed, qint64 filesDeleted, qint64 filesSkipped, bool success) {
    CleanupRecord record;
    record.timestamp = QDateTime::currentDateTime();
    record.target = target;
    record.bytesFreed = bytesFreed;
    record.filesDeleted = filesDeleted;
    record.filesSkipped = filesSkipped;
    record.status = success ? "success" : "partial";

    record.id = m_db.recordCleanupOperation(record);
    m_db.bumpStatistic(today(), "bytes_cleaned", bytesFreed);
    m_markdown.logCleanup(target, bytesFreed, filesDeleted, filesSkipped);

    emit cleanupLogged(record);
}

void ActivityLogger::logFolderCreated(const QString &path) {
    m_db.bumpStatistic(today(), "folders_created", 1);
    m_markdown.logFolderCreated(path);
}

void ActivityLogger::logUndo(int operationId, const QString &fileName, const QString &restoredTo, bool success, const QString &detail) {
    Q_UNUSED(operationId);
    m_markdown.logUndo(fileName, restoredTo, success, detail);
}

void ActivityLogger::logRuleChange(const QString &description) {
    m_markdown.logRuleChange(description);
}

void ActivityLogger::logWatcherEvent(const QString &message) {
    m_markdown.logWatcherEvent(message);
}

} // namespace bytes::logging
