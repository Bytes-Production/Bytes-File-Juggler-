#pragma once

#include <QSqlDatabase>

namespace bytes::database {

/// Creates (or upgrades) the SQLite schema used by the whole application.
/// All statements are idempotent (CREATE TABLE IF NOT EXISTS) so calling
/// this on every startup is safe.
class DatabaseSchema {
public:
    /// Returns the schema version this build of Bytes expects.
    static int currentVersion();

    /// Creates all tables/indexes if they do not already exist and seeds
    /// default data (built-in categories) on first run. Returns false and
    /// leaves the database untouched (aside from what SQLite itself did)
    /// if any statement fails.
    static bool initialize(QSqlDatabase &db);

private:
    static bool createTables(QSqlDatabase &db);
    static void migrateColumnsIfMissing(QSqlDatabase &db);
    static bool seedDefaultCategories(QSqlDatabase &db);
    static bool seedDefaultRules(QSqlDatabase &db);
    static bool seedDefaultSettings(QSqlDatabase &db);
    static int readSchemaVersion(QSqlDatabase &db);
    static void writeSchemaVersion(QSqlDatabase &db, int version);
};

} // namespace bytes::database
