#pragma once

#include <QString>
#include <QDateTime>
#include <QVector>
#include <QMutex>
#include <QSqlDatabase>
#include <QMetaType>
#include <optional>

namespace bytes::database {

struct MonitoredFolder {
    int id = -1;
    QString path;
    bool enabled = true;
    QString destinationMode = "subfolders"; // "subfolders" | "custom"
    QString customDestination;
    int gracePeriodSeconds = 60;
    bool organizeExisting = false;
    qint64 filesProcessed = 0;
    QDateTime createdAt;
};

struct Category {
    int id = -1;
    QString name;
    QStringList extensions;
    QString destination;
    QString icon;
    bool enabled = true;
    int priority = 100;
    bool builtIn = false;
};

struct Rule {
    int id = -1;
    QString name;
    QString conditionType;  // extension|filetype|contains|startswith|endswith|wildcard|size|source_folder|date
    QString conditionValue;
    QString actionType;     // move|skip|custom_destination
    QString actionValue;
    int priority = 100;
    bool enabled = true;
    QDateTime createdAt;
    QString scopeFolder;    // empty = applies to every monitored folder; otherwise a specific folder's path
};

struct FileOperationRecord {
    int id = -1;
    QDateTime timestamp;
    QString sourcePath;
    QString destPath;
    QString fileType;
    QString ruleName;
    QString status; // moved|skipped|error|would_move|undone
    QString errorMessage;
    bool undone = false;
};

struct CleanupRecord {
    int id = -1;
    QDateTime timestamp;
    QString target;
    qint64 bytesFreed = 0;
    qint64 filesDeleted = 0;
    qint64 filesSkipped = 0;
    QString status;
};

struct ErrorRecord {
    int id = -1;
    QDateTime timestamp;
    QString context;
    QString filePath;
    QString message;
};

struct DayStatistics {
    QString date; // yyyy-MM-dd
    qint64 filesOrganized = 0;
    qint64 foldersCreated = 0;
    qint64 filesSkipped = 0;
    qint64 errors = 0;
    qint64 bytesCleaned = 0;
};

/// Thread-safe façade over the application's SQLite database. Every public
/// method acquires an internal mutex, so Database instances may be shared
/// across worker threads safely. All statements are prepared to avoid SQL
/// injection and to benefit from SQLite's statement caching.
class Database {
public:
    /// Opens (creating if necessary) the database at %LOCALAPPDATA%\Bytes\bytes.db
    /// and runs schema migrations. Returns false on unrecoverable failure.
    bool open();

    /// Test-only overload: opens (creating if necessary) the database at an
    /// explicit path instead of the standard %LOCALAPPDATA%\Bytes location,
    /// so unit tests never touch a real user's data.
    bool openAt(const QString &filePath);
    void close();
    bool isOpen() const;

    QString databaseFilePath() const { return m_dbPath; }

    // --- settings ---
    QString getSetting(const QString &key, const QString &defaultValue = QString()) const;
    bool setSetting(const QString &key, const QString &value);
    int getSettingInt(const QString &key, int defaultValue) const;
    bool getSettingBool(const QString &key, bool defaultValue) const;

    // --- monitored folders ---
    QVector<MonitoredFolder> monitoredFolders() const;
    std::optional<MonitoredFolder> monitoredFolder(int id) const;
    int addMonitoredFolder(const MonitoredFolder &folder);
    bool updateMonitoredFolder(const MonitoredFolder &folder);
    bool removeMonitoredFolder(int id);
    bool setMonitoredFolderEnabled(int id, bool enabled);
    bool incrementFilesProcessed(int folderId, qint64 delta = 1);

    // --- categories ---
    QVector<Category> categories() const;
    int addCategory(const Category &category);
    bool updateCategory(const Category &category);
    bool removeCategory(int id);

    // --- rules ---
    QVector<Rule> rules() const; // ordered by priority desc (specific first)
    int addRule(const Rule &rule);
    bool updateRule(const Rule &rule);
    bool removeRule(int id);
    bool setRuleEnabled(int id, bool enabled);
    bool setRulePriority(int id, int priority);

    // --- file operations / activity log ---
    int recordFileOperation(const FileOperationRecord &record);
    bool markOperationUndone(int id);
    QVector<FileOperationRecord> recentFileOperations(int limit = 200) const;
    QVector<FileOperationRecord> fileOperationsFiltered(const QString &statusFilter,
                                                         const QString &searchText,
                                                         const QDateTime &from,
                                                         const QDateTime &to,
                                                         int limit = 500) const;
    std::optional<FileOperationRecord> fileOperation(int id) const;

    // --- cleanup ---
    int recordCleanupOperation(const CleanupRecord &record);
    QVector<CleanupRecord> recentCleanupOperations(int limit = 100) const;

    // --- errors ---
    int recordError(const ErrorRecord &record);
    QVector<ErrorRecord> recentErrors(int limit = 100) const;

    // --- statistics ---
    DayStatistics statisticsForDate(const QString &yyyyMMdd) const;
    void bumpStatistic(const QString &yyyyMMdd, const char *column, qint64 delta);
    qint64 totalFilesOrganized() const;
    qint64 filesOrganizedSince(const QDateTime &since) const;

    // --- maintenance ---
    void purgeOldActivity(int retentionDays); // 0 = forever, never purge
    bool vacuum();

    /// Restores settings, rules, and categories to their defaults. Never
    /// touches monitored folders or activity history -- "reset
    /// configuration" should not make Bytes forget what happened, only
    /// how it currently behaves.
    bool resetToDefaults();

private:
    mutable QMutex m_mutex;
    QSqlDatabase m_db;
    QString m_connectionName;
    QString m_dbPath;
};

} // namespace bytes::database

Q_DECLARE_METATYPE(bytes::database::FileOperationRecord)
Q_DECLARE_METATYPE(bytes::database::CleanupRecord)
Q_DECLARE_METATYPE(bytes::database::ErrorRecord)
