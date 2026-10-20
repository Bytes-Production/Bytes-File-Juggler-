#pragma once

#include "FileWatcher.h"
#include "RuleEngine.h"
#include "SafeMover.h"
#include "../database/Database.h"
#include "../logging/ActivityLogger.h"
#include "../windows/FolderIconManager.h"

#include <QObject>
#include <QHash>
#include <QTimer>
#include <QThreadPool>
#include <QElapsedTimer>
#include <QMutex>
#include <atomic>
#include <optional>

namespace bytes::core {

/// Preview of what Bytes would do with a set of existing files, without
/// touching anything -- used by the first-run "N existing files found"
/// prompt and the Rules/Preview UI.
struct ExistingFilesSummary {
    QHash<QString, int> countByCategory;
    int totalFiles = 0;
};

/// The central orchestrator: owns the FileWatcher, listens for filesystem
/// events, applies the grace period + stability checks, runs the
/// RuleEngine, and dispatches the actual move to a worker thread pool so
/// the GUI/event-loop thread is never blocked.
class FileOrganizer : public QObject {
    Q_OBJECT
public:
    FileOrganizer(database::Database &db,
                  logging::ActivityLogger &activityLogger,
                  windows::FolderIconManager &folderIconManager,
                  QObject *parent = nullptr);

    /// Loads monitored folders from the database and starts watching the
    /// enabled ones. Call once at startup after the database is open.
    void start();
    void stop();

    void reloadFolders();   // call after Folders page add/remove/enable changes
    void reloadRules();     // call after Rules/Categories page edits

    void setPaused(bool paused);
    bool isPaused() const { return m_paused.load(); }

    void setPreviewMode(bool enabled) { m_previewMode.store(enabled); }
    bool previewMode() const { return m_previewMode.load(); }

    /// Manual "Run Organizer" trigger: scans a folder's existing files
    /// right now instead of waiting for filesystem events.
    void organizeExistingFiles(int folderId);

    /// Non-destructive scan used by the first-run wizard / Rules preview.
    ExistingFilesSummary summarizeExistingFiles(const QString &folderPath) const;

    /// Exposes the shared RuleEngine for the Rule Tester dialog.
    RuleEngine &ruleEngine() { return m_ruleEngine; }

signals:
    void statusMessage(QString message);
    void batchNotificationReady(int moved, int skipped, int errors);
    void monitoringStateChanged(bool active);

private slots:
    void onFileEvent(int folderId, QString path, bytes::core::FileChangeType type);
    void onWatcherError(int folderId, QString message);
    void onGraceTimerFired(QString key);
    void onBatchNotifyTimer();
    void restartBatchTimer();
    void onCatchUpTimer();

private:
    struct PendingFile {
        int folderId;
        QString path;
        QTimer *timer = nullptr;
    };

    database::Database &m_db;
    logging::ActivityLogger &m_activityLogger;
    windows::FolderIconManager &m_folderIconManager;

    FileWatcher m_watcher;
    RuleEngine m_ruleEngine;
    QThreadPool m_pool;

    QHash<QString, PendingFile> m_pending; // key: native full path, main-thread only

    mutable QMutex m_foldersMutex;
    QHash<int, database::MonitoredFolder> m_folders; // cache, by id

    std::atomic_bool m_paused{false};
    std::atomic_bool m_previewMode{false};

    QTimer m_batchNotifyTimer;
    std::atomic_int m_batchMoved{0};
    std::atomic_int m_batchSkipped{0};
    std::atomic_int m_batchErrors{0};

    // Hourly safety-net: catches files ReadDirectoryChangesW could have
    // missed (e.g. Bytes wasn't running when they were added). Deliberately
    // bounded to recently-modified files only -- see onCatchUpTimer() --
    // so it can never sweep up an old backlog of pre-existing files the
    // way an unconfirmed "Run Organizer" can.
    QTimer m_catchUpTimer;

    void scheduleGracePeriod(int folderId, const QString &path);
    void processFile(int folderId, const QString &path, bool skipStabilityWait = false);
    std::optional<database::MonitoredFolder> folderById(int id) const;
    QString resolveDestinationRoot(const database::MonitoredFolder &folder) const;
    void ensureDestinationFolder(const QString &rootPath, const QString &absoluteDestDir);
    void recordBatchOutcome(MoveOutcome outcome);
};

} // namespace bytes::core
