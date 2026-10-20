#pragma once

#include "MarkdownLogger.h"
#include "../database/Database.h"

#include <QObject>

namespace bytes::logging {

/// Single call site for "log every action" (requirement across the whole
/// app): every call here writes to SQLite (structured, queryable history),
/// to the daily Markdown log (human-readable), and emits a Qt signal so the
/// GUI can update live without polling the database.
class ActivityLogger : public QObject {
    Q_OBJECT
public:
    ActivityLogger(database::Database &db, MarkdownLogger &markdownLogger, QObject *parent = nullptr);

    int logMove(const QString &sourcePath, const QString &destPath, const QString &fileType, const QString &ruleName);
    int logSkip(const QString &sourcePath, const QString &fileType, const QString &ruleName, const QString &reason);
    int logWouldMove(const QString &sourcePath, const QString &destPath, const QString &fileType, const QString &ruleName);
    int logError(const QString &sourcePath, const QString &context, const QString &reason);

    void logCleanup(const QString &target, qint64 bytesFreed, qint64 filesDeleted, qint64 filesSkipped, bool success);
    void logFolderCreated(const QString &path);
    void logUndo(int operationId, const QString &fileName, const QString &restoredTo, bool success, const QString &detail);
    void logRuleChange(const QString &description);
    void logWatcherEvent(const QString &message);

signals:
    void operationLogged(bytes::database::FileOperationRecord record);
    void cleanupLogged(bytes::database::CleanupRecord record);
    void errorLogged(bytes::database::ErrorRecord record);

private:
    database::Database &m_db;
    MarkdownLogger &m_markdown;

    static QString today();
};

} // namespace bytes::logging
