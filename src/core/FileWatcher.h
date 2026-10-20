#pragma once

#include <QObject>
#include <QThread>
#include <QHash>
#include <QMutex>
#include <QString>
#include <QStringList>
#include <QMetaType>
#include <atomic>
#include <memory>

namespace bytes::core {

enum class FileChangeType { Added, Modified, RenamedNew, RenamedOld, Removed };

/// Runs ReadDirectoryChangesW on a dedicated background thread for one
/// monitored folder and emits Qt signals for changes. One worker exists
/// per watched folder; all workers report back to the owning FileWatcher.
class FileWatcherWorker : public QObject {
    Q_OBJECT
public:
    FileWatcherWorker(int folderId, QString path);
    ~FileWatcherWorker() override;

public slots:
    void run();       // entry point, invoked on the worker's own thread
    void stop();       // thread-safe request to exit run() promptly

signals:
    void changed(int folderId, QString fullPath, bytes::core::FileChangeType type);
    void errorOccurred(int folderId, QString message);
    void finished();

private:
    int m_folderId;
    QString m_path;
    std::atomic_bool m_stopRequested{false};
    void *m_directoryHandle = nullptr; // HANDLE, opaque to keep <Windows.h> out of this header
};

/// Owns one FileWatcherWorker + QThread pair per monitored folder id.
/// Safe to call from the GUI thread; all Windows API calls happen on the
/// worker threads. Consumers should connect to `fileEvent` with a queued
/// connection (the default for cross-thread signals) to marshal work back
/// onto whichever thread should react to it.
class FileWatcher : public QObject {
    Q_OBJECT
public:
    explicit FileWatcher(QObject *parent = nullptr);
    ~FileWatcher() override;

    /// Starts (or restarts, if already watching) a folder. Returns false if
    /// the path does not exist or a thread could not be started.
    bool watch(int folderId, const QString &path);
    void unwatch(int folderId);
    void unwatchAll();
    bool isWatching(int folderId) const;
    QStringList watchedPaths() const;

signals:
    void fileEvent(int folderId, QString fullPath, bytes::core::FileChangeType type);
    void watcherError(int folderId, QString message);

private:
    struct Watch {
        QThread *thread = nullptr;
        FileWatcherWorker *worker = nullptr;
        QString path;
    };

    mutable QMutex m_mutex;
    QHash<int, Watch> m_watches;

    void stopWatch(Watch &watch);
};

} // namespace bytes::core

Q_DECLARE_METATYPE(bytes::core::FileChangeType)
