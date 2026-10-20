#pragma once

#include <QString>
#include <QStringList>
#include <functional>

namespace bytes::cleaner {

struct CleanupResult {
    qint64 bytesFreed = 0;
    qint64 filesDeleted = 0;
    qint64 filesSkipped = 0;
    QStringList skippedReasons; // "name.tmp: SKIPPED — FILE IN USE"
};

/// Cleans the User and Windows TEMP directories. Never deletes the TEMP
/// directory itself (only its contents), never force-closes a handle,
/// never elevates, and never touches anything outside the given root.
class TempCleaner {
public:
    static QString userTempPath();
    static QString windowsTempPath();

    /// Recursively sums file sizes under `path`. Best-effort: unreadable
    /// entries are simply skipped rather than aborting the whole scan.
    static qint64 calculateSize(const QString &path);

    /// Deletes files under `path` (recursively), removing now-empty
    /// subdirectories afterwards, but never the root `path` itself. Locked
    /// files are left in place and counted as skipped, never force-closed.
    /// If `dryRun` is true, nothing is deleted -- only sizes/counts are
    /// calculated as if it were run for real.
    CleanupResult clean(const QString &path, bool dryRun,
                        const std::function<void(const QString &file, bool deleted, const QString &reason)> &onFile = nullptr) const;
};

} // namespace bytes::cleaner
