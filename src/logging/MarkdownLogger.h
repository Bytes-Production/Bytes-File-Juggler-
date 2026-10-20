#pragma once

#include <QString>
#include <QDate>
#include <QMutex>

namespace bytes::logging {

/// Appends human-readable Markdown activity logs to
/// %LOCALAPPDATA%\Bytes\Logs\yyyy-MM-dd.md, one file per day, matching the
/// exact format specified for Bytes. Safe to call from any thread.
class MarkdownLogger {
public:
    explicit MarkdownLogger(QString logDirectory);

    void logFileMoved(const QString &fileName, const QString &type, const QString &source,
                       const QString &destination, const QString &ruleName);
    void logFileSkipped(const QString &fileName, const QString &source, const QString &reason);
    void logFileWouldMove(const QString &fileName, const QString &source, const QString &destination,
                           const QString &ruleName);
    void logError(const QString &fileName, const QString &context, const QString &reason);
    void logFolderCreated(const QString &path);
    void logCleanup(const QString &target, qint64 bytesFreed, qint64 filesDeleted, qint64 filesSkipped);
    void logUndo(const QString &fileName, const QString &restoredTo, bool success, const QString &detail);
    void logWatcherEvent(const QString &message);
    void logRuleChange(const QString &description);

    /// Deletes daily log files older than retentionDays (0 = keep forever).
    void purgeOldLogs(int retentionDays) const;

    QString logDirectory() const { return m_logDir; }

private:
    QString m_logDir;
    mutable QMutex m_mutex;

    QString currentLogFilePath() const;
    void appendSection(const QString &markdown);
    static QString timeStamp();
    static QString escapeCode(const QString &text);
};

} // namespace bytes::logging
