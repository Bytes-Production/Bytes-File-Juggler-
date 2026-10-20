#pragma once

#include <QString>

namespace bytes::core {

enum class DuplicatePolicy { Rename, Skip, Ask, Replace };

struct DuplicateResolution {
    bool proceed = true;        // false => caller should skip the move entirely
    bool needsUserPrompt = false; // true only for Ask, before UI has answered
    QString finalFileName;      // name to actually write to disk
    QString reason;             // human-readable, for logging
};

/// Centralizes Bytes's duplicate-file policy. Bytes never silently
/// overwrites a file: "Replace" is implemented as a safe rename-aside
/// rather than a destructive overwrite, so no data is ever lost even when
/// the user has selected it.
class DuplicateHandler {
public:
    static QString policyToString(DuplicatePolicy policy);
    static DuplicatePolicy policyFromString(const QString &value);

    /// Finds the next available "name (1).ext", "name (2).ext" ... in destDir.
    static QString nextAvailableName(const QString &destDir, const QString &fileName);

    /// Decides what to do about `fileName` already existing in `destDir`.
    static DuplicateResolution resolve(const QString &destDir, const QString &fileName, DuplicatePolicy policy);
};

} // namespace bytes::core
