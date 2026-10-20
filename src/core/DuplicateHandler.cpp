#include "DuplicateHandler.h"

#include <QDir>
#include <QFileInfo>
#include <QDateTime>

namespace bytes::core {

QString DuplicateHandler::policyToString(DuplicatePolicy policy) {
    switch (policy) {
    case DuplicatePolicy::Rename: return "rename";
    case DuplicatePolicy::Skip: return "skip";
    case DuplicatePolicy::Ask: return "ask";
    case DuplicatePolicy::Replace: return "replace";
    }
    return "rename";
}

DuplicatePolicy DuplicateHandler::policyFromString(const QString &value) {
    const QString v = value.trimmed().toLower();
    if (v == "skip") return DuplicatePolicy::Skip;
    if (v == "ask") return DuplicatePolicy::Ask;
    if (v == "replace") return DuplicatePolicy::Replace;
    return DuplicatePolicy::Rename;
}

QString DuplicateHandler::nextAvailableName(const QString &destDir, const QString &fileName) {
    QDir dir(destDir);
    if (!dir.exists(fileName))
        return fileName;

    const QFileInfo info(fileName);
    const QString base = info.completeBaseName();
    const QString suffix = info.suffix();

    for (int n = 1; n < 100000; ++n) {
        const QString candidate = suffix.isEmpty()
            ? QString("%1 (%2)").arg(base).arg(n)
            : QString("%1 (%2).%3").arg(base).arg(n).arg(suffix);
        if (!dir.exists(candidate))
            return candidate;
    }
    return QString("%1 (%2)").arg(fileName).arg(QDateTime::currentMSecsSinceEpoch());
}

DuplicateResolution DuplicateHandler::resolve(const QString &destDir, const QString &fileName, DuplicatePolicy policy) {
    DuplicateResolution res;
    QDir dir(destDir);

    if (!dir.exists(fileName)) {
        res.finalFileName = fileName;
        return res;
    }

    switch (policy) {
    case DuplicatePolicy::Skip:
        res.proceed = false;
        res.reason = "A file with this name already exists at the destination";
        return res;
    case DuplicatePolicy::Ask:
        res.needsUserPrompt = true;
        res.proceed = false; // caller must re-resolve once the user answers
        res.reason = "Duplicate found; user decision required";
        return res;
    case DuplicatePolicy::Replace:
        // Never a destructive overwrite: rename the new file aside so both
        // copies survive, exactly like Rename. This guarantees the "never
        // overwrite by default / never lose data" rule holds even if a
        // user or future rule sets Replace.
    case DuplicatePolicy::Rename:
    default:
        res.finalFileName = nextAvailableName(destDir, fileName);
        res.reason = "Renamed to avoid overwriting an existing file";
        return res;
    }
}

} // namespace bytes::core
