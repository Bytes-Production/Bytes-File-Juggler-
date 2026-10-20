#pragma once

#include "DuplicateHandler.h"

#include <QString>

namespace bytes::core {

struct MoveRequest {
    QString sourcePath;
    QString destinationDir;    // directory the file should end up in (created if missing)
    DuplicatePolicy duplicatePolicy = DuplicatePolicy::Rename;
    int retryCount = 10;
    int retryDelayMs = 500;
    bool dryRun = false;
};

enum class MoveOutcome { Moved, Skipped, WouldMove, Error };

struct MoveResult {
    MoveOutcome outcome = MoveOutcome::Error;
    QString finalDestinationPath;
    QString errorMessage;
    bool needsUserPrompt = false; // true when duplicatePolicy == Ask and no answer was supplied
};

/// Performs the actual, safe filesystem move for one file. Every safety
/// rule from the Bytes spec lives here: never overwrite silently, never
/// delete the source before the destination write is verified, never
/// touch a file that is still being written to.
///
/// Intended to run on a background worker thread -- every method here may
/// block (stability polling, retries) and must never be called from the
/// GUI thread.
class SafeMover {
public:
    /// Polls size + modified-time `checkCount` times, `intervalMs` apart.
    /// Returns true only if the file looked identical on every check and
    /// could be opened for read the whole time (i.e. nobody is writing to
    /// it right now).
    static bool isFileStable(const QString &path, int checkCount, int intervalMs);

    /// True if the file cannot currently be opened with read+write sharing
    /// denied to writers (i.e. some other process holds an exclusive or
    /// write handle on it).
    static bool isFileLocked(const QString &path);

    /// Moves (or, in dry-run mode, only evaluates) one file according to
    /// `request`. Creates the destination directory tree if needed. Only
    /// reports Moved after verifying the destination file exists with the
    /// expected size and the source no longer does.
    MoveResult moveFile(const MoveRequest &request);

private:
    static bool nativeMove(const QString &fromPath, const QString &toPath, QString &errorOut);
    static QString toExtendedLengthPath(const QString &path);
};

} // namespace bytes::core
