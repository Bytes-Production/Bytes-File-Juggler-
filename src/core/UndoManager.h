#pragma once

#include "../database/Database.h"

#include <QString>
#include <QVector>

namespace bytes::core {

struct UndoOutcome {
    int operationId = -1;
    bool success = false;
    QString restoredPath;
    QString message;
};

/// Restores files that Bytes previously moved, back to (as close as
/// possible to) their original location. Never overwrites another file
/// while doing so -- if the original location is occupied again, the
/// restored file is suffixed, e.g. "photo (restored).jpg".
class UndoManager {
public:
    explicit UndoManager(database::Database &db);

    /// Undoes the single most recent successful, not-yet-undone move.
    UndoOutcome undoLast();

    /// Undoes one specific operation by its file_operations id.
    UndoOutcome undoOperation(int operationId);

    /// Undoes several operations; returns one outcome per id, in order.
    QVector<UndoOutcome> undoMultiple(const QVector<int> &operationIds);

private:
    database::Database &m_db;

    UndoOutcome restoreOne(const database::FileOperationRecord &record);
};

} // namespace bytes::core
