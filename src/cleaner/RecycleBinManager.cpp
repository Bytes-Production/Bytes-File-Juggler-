#include "RecycleBinManager.h"

#include <Windows.h>
#include <shellapi.h>

namespace bytes::cleaner {

RecycleBinInfo RecycleBinManager::query() {
    RecycleBinInfo info;
    SHQUERYRBINFO rbInfo = {};
    rbInfo.cbSize = sizeof(SHQUERYRBINFO);

    // nullptr root path queries the recycle bin across all drives.
    if (SUCCEEDED(SHQueryRecycleBinW(nullptr, &rbInfo))) {
        info.itemCount = static_cast<qint64>(rbInfo.i64NumItems);
        info.totalBytes = static_cast<qint64>(rbInfo.i64Size);
    }
    return info;
}

bool RecycleBinManager::empty() {
    const DWORD flags = SHERB_NOCONFIRMATION | SHERB_NOPROGRESSUI | SHERB_NOSOUND;
    return SHEmptyRecycleBinW(nullptr, nullptr, flags) == S_OK;
}

} // namespace bytes::cleaner
