#include "FileWatcher.h"

#include <QDir>
#include <QDebug>
#include <QMutexLocker>
#include <vector>

#include <Windows.h>

namespace bytes::core {

namespace {
constexpr DWORD kBufferSize = 64 * 1024; // 64KB notify buffer, well within kernel limits
constexpr DWORD kWaitTimeoutMs = 500;    // how often we check for a stop request
}

FileWatcherWorker::FileWatcherWorker(int folderId, QString path)
    : m_folderId(folderId), m_path(std::move(path)) {}

FileWatcherWorker::~FileWatcherWorker() {
    if (m_directoryHandle && m_directoryHandle != INVALID_HANDLE_VALUE)
        CloseHandle(static_cast<HANDLE>(m_directoryHandle));
}

void FileWatcherWorker::stop() {
    m_stopRequested.store(true, std::memory_order_relaxed);
}

void FileWatcherWorker::run() {
    const std::wstring wpath = QDir::toNativeSeparators(m_path).toStdWString();

    HANDLE dirHandle = CreateFileW(
        wpath.c_str(),
        FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
        nullptr);

    if (dirHandle == INVALID_HANDLE_VALUE) {
        emit errorOccurred(m_folderId, QString("Could not open folder for monitoring: %1").arg(m_path));
        emit finished();
        return;
    }
    m_directoryHandle = dirHandle;

    HANDLE event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!event) {
        CloseHandle(dirHandle);
        m_directoryHandle = nullptr;
        emit errorOccurred(m_folderId, "Could not create synchronization event for monitoring");
        emit finished();
        return;
    }

    std::vector<BYTE> buffer(kBufferSize);
    OVERLAPPED overlapped = {};
    overlapped.hEvent = event;

    auto issueRead = [&]() -> bool {
        ResetEvent(event);
        return ReadDirectoryChangesW(
            dirHandle, buffer.data(), static_cast<DWORD>(buffer.size()), TRUE,
            FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_SIZE |
            FILE_NOTIFY_CHANGE_CREATION,
            nullptr, &overlapped, nullptr);
    };

    if (!issueRead()) {
        emit errorOccurred(m_folderId, QString("Failed to start monitoring: error %1").arg(GetLastError()));
        CloseHandle(event);
        CloseHandle(dirHandle);
        m_directoryHandle = nullptr;
        emit finished();
        return;
    }

    while (!m_stopRequested.load(std::memory_order_relaxed)) {
        const DWORD waitResult = WaitForSingleObject(event, kWaitTimeoutMs);

        if (waitResult == WAIT_TIMEOUT)
            continue; // nothing happened; re-check stop flag

        if (waitResult != WAIT_OBJECT_0) {
            emit errorOccurred(m_folderId, "Directory monitoring wait failed unexpectedly");
            break;
        }

        DWORD bytesTransferred = 0;
        if (!GetOverlappedResult(dirHandle, &overlapped, &bytesTransferred, FALSE)) {
            const DWORD err = GetLastError();
            if (err == ERROR_OPERATION_ABORTED)
                break; // we cancelled it ourselves during shutdown
            emit errorOccurred(m_folderId, QString("Overlapped read failed: error %1").arg(err));
            break;
        }

        if (bytesTransferred == 0) {
            // Buffer overflowed (too many changes at once) -- safe fallback
            // is simply to re-issue the read; the organizer will pick up
            // any missed files on its next existing-files pass if needed.
            if (!issueRead())
                break;
            continue;
        }

        size_t offset = 0;
        while (true) {
            const auto *info = reinterpret_cast<const FILE_NOTIFY_INFORMATION *>(buffer.data() + offset);
            const QString relativeName = QString::fromWCharArray(info->FileName, info->FileNameLength / sizeof(WCHAR));
            const QString fullPath = QDir(m_path).filePath(relativeName);

            FileChangeType type;
            bool emitIt = true;
            switch (info->Action) {
            case FILE_ACTION_ADDED: type = FileChangeType::Added; break;
            case FILE_ACTION_MODIFIED: type = FileChangeType::Modified; break;
            case FILE_ACTION_RENAMED_NEW_NAME: type = FileChangeType::RenamedNew; break;
            case FILE_ACTION_RENAMED_OLD_NAME: type = FileChangeType::RenamedOld; break;
            case FILE_ACTION_REMOVED: type = FileChangeType::Removed; break;
            default: emitIt = false; type = FileChangeType::Modified; break;
            }
            if (emitIt)
                emit changed(m_folderId, QDir::toNativeSeparators(fullPath), type);

            if (info->NextEntryOffset == 0)
                break;
            offset += info->NextEntryOffset;
        }

        if (!issueRead())
            break;
    }

    CancelIoEx(dirHandle, &overlapped);
    // Wait for the cancellation to complete so the OVERLAPPED struct isn't
    // touched by the kernel after we free it.
    DWORD dummy = 0;
    GetOverlappedResult(dirHandle, &overlapped, &dummy, TRUE);

    CloseHandle(event);
    CloseHandle(dirHandle);
    m_directoryHandle = nullptr;
    emit finished();
}

// ---------------------------------------------------------------- FileWatcher

FileWatcher::FileWatcher(QObject *parent) : QObject(parent) {
    qRegisterMetaType<bytes::core::FileChangeType>("bytes::core::FileChangeType");
}

FileWatcher::~FileWatcher() {
    unwatchAll();
}

void FileWatcher::stopWatch(Watch &watch) {
    if (!watch.worker || !watch.thread)
        return;
    watch.worker->stop();
    watch.thread->quit();
    watch.thread->wait(3000);
    delete watch.worker;
    delete watch.thread;
    watch.worker = nullptr;
    watch.thread = nullptr;
}

bool FileWatcher::watch(int folderId, const QString &path) {
    if (!QDir(path).exists())
        return false;

    QMutexLocker lock(&m_mutex);
    if (m_watches.contains(folderId))
        stopWatch(m_watches[folderId]);

    Watch watch;
    watch.path = path;
    watch.thread = new QThread();
    watch.worker = new FileWatcherWorker(folderId, path);
    watch.worker->moveToThread(watch.thread);

    connect(watch.thread, &QThread::started, watch.worker, &FileWatcherWorker::run);
    connect(watch.worker, &FileWatcherWorker::changed, this, &FileWatcher::fileEvent, Qt::QueuedConnection);
    connect(watch.worker, &FileWatcherWorker::errorOccurred, this, &FileWatcher::watcherError, Qt::QueuedConnection);
    connect(watch.worker, &FileWatcherWorker::finished, watch.thread, &QThread::quit);

    watch.thread->setObjectName(QString("BytesWatcher-%1").arg(folderId));
    watch.thread->start();

    m_watches.insert(folderId, watch);
    return true;
}

void FileWatcher::unwatch(int folderId) {
    QMutexLocker lock(&m_mutex);
    auto it = m_watches.find(folderId);
    if (it == m_watches.end())
        return;
    stopWatch(it.value());
    m_watches.erase(it);
}

void FileWatcher::unwatchAll() {
    QMutexLocker lock(&m_mutex);
    for (auto it = m_watches.begin(); it != m_watches.end(); ++it)
        stopWatch(it.value());
    m_watches.clear();
}

bool FileWatcher::isWatching(int folderId) const {
    QMutexLocker lock(&m_mutex);
    return m_watches.contains(folderId);
}

QStringList FileWatcher::watchedPaths() const {
    QMutexLocker lock(&m_mutex);
    QStringList paths;
    for (const auto &w : m_watches)
        paths << w.path;
    return paths;
}

} // namespace bytes::core
