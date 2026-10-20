#pragma once

#include <QString>

namespace bytes::cleaner {

struct RecycleBinInfo {
    qint64 itemCount = 0;
    qint64 totalBytes = 0;
};

/// Thin wrapper around the Shell's Recycle Bin APIs. Emptying always
/// requires an explicit, separate confirmation from the caller (the UI
/// layer) -- this class performs no confirmation itself, it only executes
/// once asked.
class RecycleBinManager {
public:
    /// Queries size/count across all drives' recycle bins for the current user.
    static RecycleBinInfo query();

    /// Empties the Recycle Bin. Returns false if the Shell call fails; a
    /// failure here never deletes files by any other means.
    static bool empty();
};

} // namespace bytes::cleaner
