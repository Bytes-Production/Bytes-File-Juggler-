#include "FileOrganizer.h"
#include "DuplicateHandler.h"

#include <QDir>
#include <QFileInfo>
#include <QDateTime>
#include <QMetaObject>
#include <QRunnable>
#include <QDebug>
#include <utility>

namespace bytes::core {

using bytes::database::MonitoredFolder;

FileOrganizer::FileOrganizer(database::Database &db,
                              logging::ActivityLogger &activityLogger,
                              windows::FolderIconManager &folderIconManager,
                              QObject *parent)
    : QObject(parent), m_db(db), m_activityLogger(activityLogger),
      m_folderIconManager(folderIconManager), m_ruleEngine(db) {
    connect(&m_watcher, &FileWatcher::fileEvent, this, &FileOrganizer::onFileEvent, Qt::QueuedConnection);
    connect(&m_watcher, &FileWatcher::watcherError, this, &FileOrganizer::onWatcherError, Qt::QueuedConnection);

    m_batchNotifyTimer.setSingleShot(true);
    connect(&m_batchNotifyTimer, &QTimer::timeout, this, &FileOrganizer::onBatchNotifyTimer);

    connect(&m_catchUpTimer, &QTimer::timeout, this, &FileOrganizer::onCatchUpTimer);

    m_pool.setMaxThreadCount(qMax(2, m_db.getSettingInt("worker_count", 2)));
}

void FileOrganizer::start() {
    reloadFolders();
    reloadRules();

    QMutexLocker lock(&m_foldersMutex);
    for (const auto &folder : std::as_const(m_folders)) {
        if (folder.enabled)
            m_watcher.watch(folder.id, folder.path);
    }
    emit monitoringStateChanged(!m_paused.load());

    m_catchUpTimer.start(60 * 60 * 1000); // hourly safety-net scan
}

void FileOrganizer::stop() {
    m_catchUpTimer.stop();
    m_watcher.unwatchAll();
    for (auto &pending : m_pending) {
        pending.timer->stop();
        delete pending.timer;
    }
    m_pending.clear();
    emit monitoringStateChanged(false);
}

void FileOrganizer::reloadFolders() {
    const auto folders = m_db.monitoredFolders();
    QMutexLocker lock(&m_foldersMutex);
    m_folders.clear();
    for (const auto &f : folders)
        m_folders.insert(f.id, f);
}

void FileOrganizer::reloadRules() {
    m_ruleEngine.reload();
}

void FileOrganizer::setPaused(bool paused) {
    m_paused.store(paused);
    emit monitoringStateChanged(!paused);
    m_activityLogger.logWatcherEvent(paused ? "Juggler paused by user" : "Juggler resumed by user");
}

std::optional<MonitoredFolder> FileOrganizer::folderById(int id) const {
    QMutexLocker lock(&m_foldersMutex);
    auto it = m_folders.constFind(id);
    if (it == m_folders.constEnd())
        return std::nullopt;
    return it.value();
}

void FileOrganizer::onWatcherError(int folderId, QString message) {
    Q_UNUSED(folderId);
    m_activityLogger.logWatcherEvent("Watcher error: " + message);
    emit statusMessage(message);
}

void FileOrganizer::onFileEvent(int folderId, QString path, FileChangeType type) {
    if (type == FileChangeType::Removed || type == FileChangeType::RenamedOld)
        return;

    if (m_paused.load())
        return;

    scheduleGracePeriod(folderId, path);
}

void FileOrganizer::scheduleGracePeriod(int folderId, const QString &path) {
    const QFileInfo info(path);
    if (info.isDir())
        return;
    if (FileClassifier::isTemporaryDownloadName(info.fileName()))
        return; // still downloading -- wait for the final rename event

    const auto folder = folderById(folderId);
    if (!folder || !folder->enabled)
        return;

    // Only organize files dropped directly in the monitored root; ignore
    // changes inside folders Bytes itself already organized into (this is
    // what stops Bytes from re-processing its own moves forever).
    const QString folderRoot = QDir::cleanPath(folder->path);
    if (QDir::cleanPath(info.absolutePath()) != folderRoot)
        return;

    const QString key = QDir::toNativeSeparators(path);
    auto it = m_pending.find(key);
    if (it != m_pending.end()) {
        it->timer->start(); // debounce: another event for the same file resets the wait
        return;
    }

    int graceMs = folder->gracePeriodSeconds * 1000;
    if (graceMs < 0)
        graceMs = 0;
    if (graceMs == 0)
        graceMs = 50; // "Immediately" still yields to the event loop briefly

    auto *timer = new QTimer(this);
    timer->setSingleShot(true);
    timer->setInterval(graceMs);
    connect(timer, &QTimer::timeout, this, [this, key]() { onGraceTimerFired(key); });

    PendingFile pending{folderId, path, timer};
    m_pending.insert(key, pending);
    timer->start();
}

void FileOrganizer::onGraceTimerFired(QString key) {
    auto it = m_pending.find(key);
    if (it == m_pending.end())
        return;

    const PendingFile pending = it.value();
    pending.timer->deleteLater();
    m_pending.erase(it);

    const int folderId = pending.folderId;
    const QString path = pending.path;
    QThreadPool *pool = &m_pool;
    pool->start(QRunnable::create([this, folderId, path]() { processFile(folderId, path); }));
}

QString FileOrganizer::resolveDestinationRoot(const MonitoredFolder &folder) const {
    if (folder.destinationMode == "custom" && !folder.customDestination.isEmpty())
        return QDir::toNativeSeparators(folder.customDestination);
    return QDir::toNativeSeparators(folder.path);
}

void FileOrganizer::ensureDestinationFolder(const QString &rootPath, const QString &absoluteDestDir) {
    const QString cleanRoot = QDir::cleanPath(rootPath);
    const QString cleanDest = QDir::cleanPath(absoluteDestDir);

    if (QDir(cleanDest).exists())
        return;

    // A destination like "Documents/PDF" can require creating *two* new
    // folders (Documents, then Documents/PDF), not just the leaf -- walk
    // upward from the destination collecting every level that doesn't
    // exist yet, so each one gets branded, not only the innermost one.
    // Stop at (and never include) the monitored folder's own root: Bytes
    // brands folders it creates, never the folder the user is watching.
    QStringList missingLevels;
    QString probe = cleanDest;
    while (!probe.isEmpty() && probe != cleanRoot && !QDir(probe).exists() && !QDir(probe).isRoot()) {
        missingLevels.prepend(probe);
        const QString parent = QFileInfo(probe).path();
        if (parent == probe)
            break; // reached the filesystem root; safety guard against looping
        probe = QDir::cleanPath(parent);
    }

    if (!QDir().mkpath(cleanDest))
        return;

    const bool useIcons = m_db.getSettingBool("use_bytes_folder_icons", true);
    for (const QString &level : std::as_const(missingLevels)) {
        m_activityLogger.logFolderCreated(level);
        if (useIcons)
            m_folderIconManager.applyBytesIcon(level);
    }
}

void FileOrganizer::recordBatchOutcome(MoveOutcome outcome) {
    switch (outcome) {
    case MoveOutcome::Moved: m_batchMoved.fetch_add(1); break;
    case MoveOutcome::Skipped: m_batchSkipped.fetch_add(1); break;
    case MoveOutcome::Error: m_batchErrors.fetch_add(1); break;
    case MoveOutcome::WouldMove: break;
    }
    QMetaObject::invokeMethod(this, "restartBatchTimer", Qt::QueuedConnection);
}

void FileOrganizer::restartBatchTimer() {
    m_batchNotifyTimer.start(5000);
}

void FileOrganizer::onBatchNotifyTimer() {
    const int moved = m_batchMoved.exchange(0);
    const int skipped = m_batchSkipped.exchange(0);
    const int errors = m_batchErrors.exchange(0);
    if (moved > 0 || errors > 0)
        emit batchNotificationReady(moved, skipped, errors);
}

void FileOrganizer::processFile(int folderId, const QString &path, bool skipStabilityWait) {
    QFileInfo srcInfo(path);
    if (!srcInfo.exists() || !srcInfo.isFile())
        return; // vanished before we got to it -- nothing to do, nothing lost

    const auto folder = folderById(folderId);
    if (!folder)
        return;

    const int stabilityCount = m_db.getSettingInt("stability_check_count", 3);
    const int stabilityIntervalMs = m_db.getSettingInt("stability_check_interval_ms", 2000);

    if (!skipStabilityWait && !SafeMover::isFileStable(path, stabilityCount, stabilityIntervalMs)) {
        // Still changing or locked (actively downloading/writing). Re-check
        // again shortly rather than giving up -- but never spin forever.
        QMetaObject::invokeMethod(this, [this, folderId, path]() { scheduleGracePeriod(folderId, path); },
                                   Qt::QueuedConnection);
        return;
    }

    const RuleDecision decision = m_ruleEngine.evaluate(srcInfo, folder->path);

    if (decision.skip) {
        m_activityLogger.logSkip(path, decision.detectedType, decision.matchedRuleName, "Matched a Skip rule");
        recordBatchOutcome(MoveOutcome::Skipped);
        return;
    }

    QString destDir;
    QString destRoot;
    if (!decision.destinationAbsolute.isEmpty()) {
        destDir = decision.destinationAbsolute;
        // No monitored-folder boundary applies to a custom absolute
        // destination; ensureDestinationFolder() still won't brand
        // anything that already exists, and stops at a drive root.
    } else {
        destRoot = resolveDestinationRoot(*folder);
        destDir = QDir(destRoot).filePath(decision.destinationRelative);
    }
    destDir = QDir::toNativeSeparators(QDir::cleanPath(destDir));

    const bool preview = m_previewMode.load();
    const DuplicatePolicy policy = DuplicateHandler::policyFromString(m_db.getSetting("duplicate_handling", "rename"));

    if (!preview)
        ensureDestinationFolder(destRoot, destDir);

    MoveRequest request;
    request.sourcePath = path;
    request.destinationDir = destDir;
    request.duplicatePolicy = policy;
    request.retryCount = m_db.getSettingInt("retry_count", 10);
    request.retryDelayMs = m_db.getSettingInt("retry_delay_ms", 500);
    request.dryRun = preview;

    SafeMover mover;
    const MoveResult result = mover.moveFile(request);

    switch (result.outcome) {
    case MoveOutcome::Moved:
        m_activityLogger.logMove(path, result.finalDestinationPath, decision.detectedType, decision.matchedRuleName);
        m_db.incrementFilesProcessed(folderId);
        recordBatchOutcome(MoveOutcome::Moved);
        break;
    case MoveOutcome::WouldMove:
        m_activityLogger.logWouldMove(path, result.finalDestinationPath, decision.detectedType, decision.matchedRuleName);
        break;
    case MoveOutcome::Skipped:
        m_activityLogger.logSkip(path, decision.detectedType, decision.matchedRuleName, result.errorMessage);
        recordBatchOutcome(MoveOutcome::Skipped);
        break;
    case MoveOutcome::Error:
        m_activityLogger.logError(path, "File organization", result.errorMessage);
        recordBatchOutcome(MoveOutcome::Error);
        break;
    }
}

void FileOrganizer::onCatchUpTimer() {
    if (m_paused.load())
        return;

    // Only consider files modified within this window "recent enough" to
    // have plausibly appeared since Bytes was last watching -- anything
    // older is treated as a pre-existing file the user never asked Bytes
    // to touch, exactly like an already-there file would be at first run.
    constexpr qint64 kCatchUpWindowHours = 6;
    const QDateTime cutoff = QDateTime::currentDateTime().addSecs(-kCatchUpWindowHours * 3600);

    QList<database::MonitoredFolder> folders;
    {
        QMutexLocker lock(&m_foldersMutex);
        folders = m_folders.values();
    }

    for (const auto &folder : folders) {
        if (!folder.enabled)
            continue;
        QDir dir(folder.path);
        if (!dir.exists())
            continue;

        const auto entries = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
        for (const auto &entry : entries) {
            if (entry.lastModified() < cutoff)
                continue; // too old -- never auto-touch a pre-existing backlog
            if (FileClassifier::isTemporaryDownloadName(entry.fileName()))
                continue;
            const QString key = QDir::toNativeSeparators(entry.absoluteFilePath());
            if (m_pending.contains(key))
                continue; // already queued via the real-time watcher
            scheduleGracePeriod(folder.id, entry.absoluteFilePath());
        }
    }
}

void FileOrganizer::organizeExistingFiles(int folderId) {
    const auto folder = folderById(folderId);
    if (!folder)
        return;

    QDir dir(folder->path);
    const auto entries = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
    for (const auto &entry : entries) {
        const QString path = entry.absoluteFilePath();
        QThreadPool *pool = &m_pool;
        pool->start(QRunnable::create([this, folderId, path]() { processFile(folderId, path, /*skipStabilityWait=*/false); }));
    }
}

ExistingFilesSummary FileOrganizer::summarizeExistingFiles(const QString &folderPath) const {
    ExistingFilesSummary summary;
    QDir dir(folderPath);
    const auto entries = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);

    for (const auto &entry : entries) {
        if (FileClassifier::isTemporaryDownloadName(entry.fileName()))
            continue;
        const RuleDecision decision = m_ruleEngine.evaluate(entry, folderPath);
        const QString label = decision.skip ? "Skip" : (decision.detectedType.isEmpty() ? "Other" : decision.detectedType);
        summary.countByCategory[label] += 1;
        summary.totalFiles += 1;
    }
    return summary;
}

} // namespace bytes::core
