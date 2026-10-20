#include "MarkdownLogger.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QStringConverter>
#include <QDateTime>
#include <QMutexLocker>
#include <QDebug>

namespace bytes::logging {

MarkdownLogger::MarkdownLogger(QString logDirectory) : m_logDir(std::move(logDirectory)) {
    QDir().mkpath(m_logDir);
}

QString MarkdownLogger::timeStamp() {
    return QDateTime::currentDateTime().toString("HH:mm:ss");
}

QString MarkdownLogger::escapeCode(const QString &text) {
    // Backticks would break the inline-code spans we use for paths/names.
    QString out = text;
    out.replace('`', "'");
    return out;
}

QString MarkdownLogger::currentLogFilePath() const {
    return QDir(m_logDir).filePath(QDate::currentDate().toString("yyyy-MM-dd") + ".md");
}

void MarkdownLogger::appendSection(const QString &markdown) {
    QMutexLocker lock(&m_mutex);

    const QString path = currentLogFilePath();
    const bool isNewFile = !QFileInfo::exists(path);

    QFile file(path);
    if (!file.open(QIODevice::Append | QIODevice::Text)) {
        qWarning() << "Bytes: could not open markdown log for append:" << path;
        return;
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);

    if (isNewFile) {
        out << "# Bytes Activity Log — " << QDate::currentDate().toString("yyyy-MM-dd") << "\n\n";
        out << "---\n\n";
    }

    out << markdown;
    if (!markdown.endsWith('\n'))
        out << '\n';
    out << "\n";
}

void MarkdownLogger::logFileMoved(const QString &fileName, const QString &type, const QString &source,
                                   const QString &destination, const QString &ruleName) {
    const QString md = QString(
        "### %1 — SUCCESS\n\n"
        "**File:** `%2`\n\n"
        "**Type:** %3\n\n"
        "**From:**\n\n`%4`\n\n"
        "**To:**\n\n`%5`\n\n"
        "**Rule:** %6\n\n"
        "**Status:** ✓ Moved successfully\n\n---"
    ).arg(timeStamp(), escapeCode(fileName), type, escapeCode(source), escapeCode(destination),
          ruleName.isEmpty() ? QStringLiteral("Built-in classifier") : ruleName);
    appendSection(md);
}

void MarkdownLogger::logFileSkipped(const QString &fileName, const QString &source, const QString &reason) {
    const QString md = QString(
        "### %1 — SKIPPED\n\n"
        "**File:** `%2`\n\n"
        "**Source:**\n\n`%3`\n\n"
        "**Reason:** %4\n\n"
        "**Status:** ⏭ Skipped\n\n---"
    ).arg(timeStamp(), escapeCode(fileName), escapeCode(source), reason);
    appendSection(md);
}

void MarkdownLogger::logFileWouldMove(const QString &fileName, const QString &source, const QString &destination,
                                       const QString &ruleName) {
    const QString md = QString(
        "### %1 — PREVIEW\n\n"
        "**File:** `%2`\n\n"
        "**From:**\n\n`%3`\n\n"
        "**To:**\n\n`%4`\n\n"
        "**Rule:** %5\n\n"
        "**Status:** Would Move (dry run)\n\n---"
    ).arg(timeStamp(), escapeCode(fileName), escapeCode(source), escapeCode(destination),
          ruleName.isEmpty() ? QStringLiteral("Built-in classifier") : ruleName);
    appendSection(md);
}

void MarkdownLogger::logError(const QString &fileName, const QString &context, const QString &reason) {
    const QString md = QString(
        "### %1 — ERROR\n\n"
        "**File:** `%2`\n\n"
        "**Context:** %3\n\n"
        "**Reason:** %4\n\n"
        "**Action:** Skipped, original left untouched\n\n"
        "**Status:** ✗ Error\n\n---"
    ).arg(timeStamp(), escapeCode(fileName), context, reason);
    appendSection(md);
}

void MarkdownLogger::logFolderCreated(const QString &path) {
    const QString md = QString(
        "### %1 — FOLDER CREATED\n\n"
        "**Path:**\n\n`%2`\n\n---"
    ).arg(timeStamp(), escapeCode(path));
    appendSection(md);
}

void MarkdownLogger::logCleanup(const QString &target, qint64 bytesFreed, qint64 filesDeleted, qint64 filesSkipped) {
    const double mb = bytesFreed / (1024.0 * 1024.0);
    const QString md = QString(
        "### %1 — CLEANUP\n\n"
        "**Target:** %2\n\n"
        "**Freed:** %3 MB\n\n"
        "**Files deleted:** %4\n\n"
        "**Files skipped (in use):** %5\n\n---"
    ).arg(timeStamp(), target).arg(mb, 0, 'f', 1).arg(filesDeleted).arg(filesSkipped);
    appendSection(md);
}

void MarkdownLogger::logUndo(const QString &fileName, const QString &restoredTo, bool success, const QString &detail) {
    const QString md = QString(
        "### %1 — UNDO %2\n\n"
        "**File:** `%3`\n\n"
        "**Restored to:**\n\n`%4`\n\n"
        "**Detail:** %5\n\n---"
    ).arg(timeStamp(), success ? "SUCCESS" : "FAILED", escapeCode(fileName), escapeCode(restoredTo), detail);
    appendSection(md);
}

void MarkdownLogger::logWatcherEvent(const QString &message) {
    const QString md = QString("### %1 — WATCHER\n\n%2\n\n---").arg(timeStamp(), message);
    appendSection(md);
}

void MarkdownLogger::logRuleChange(const QString &description) {
    const QString md = QString("### %1 — RULE CHANGE\n\n%2\n\n---").arg(timeStamp(), description);
    appendSection(md);
}

void MarkdownLogger::purgeOldLogs(int retentionDays) const {
    if (retentionDays <= 0)
        return;

    const QDate cutoff = QDate::currentDate().addDays(-retentionDays);
    QDir dir(m_logDir);
    const auto entries = dir.entryInfoList({"*.md"}, QDir::Files);
    for (const auto &entry : entries) {
        const QDate fileDate = QDate::fromString(entry.completeBaseName(), "yyyy-MM-dd");
        if (fileDate.isValid() && fileDate < cutoff)
            QFile::remove(entry.absoluteFilePath());
    }
}

} // namespace bytes::logging
