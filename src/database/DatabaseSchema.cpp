#include "DatabaseSchema.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include <QVariant>
#include <QVector>
#include <QPair>

namespace bytes::database {

namespace {

// Built-in categories seeded on first run. Users may disable, re-prioritize
// or add their own on top of these; the categories table is the single
// source of truth the RuleEngine reads at runtime.
struct SeedCategory {
    const char *name;
    const char *extensions; // comma separated, no dots
    const char *destination; // relative to the monitored folder
    int priority;
};

constexpr SeedCategory kSeedCategories[] = {
    {"Images", "jpg,jpeg,png,gif,webp,bmp,tif,tiff,svg,ico,heic,heif,avif,raw,cr2,cr3,nef,arw,dng,orf,rw2", "Images", 100},
    {"Videos", "mp4,mkv,avi,mov,wmv,webm,flv,mpeg,mpg,m4v,3gp,ts,mts,m2ts", "Videos", 100},
    {"Music", "mp3,wav,flac,aac,m4a,ogg,oga,opus,wma,aiff,alac", "Music", 100},
    {"PDF", "pdf", "Documents/PDF", 110},
    {"Word", "doc,docx,odt,rtf,txt,md,tex,pages", "Documents/Word", 100},
    {"Spreadsheets", "xls,xlsx,xlsm,csv,ods,numbers,tsv", "Documents/Spreadsheets", 100},
    {"Presentations", "ppt,pptx,pptm,odp,key", "Documents/Presentations", 100},
    {"Archives", "zip,rar,7z,tar,gz,bz2,xz,tgz,cab", "Archives", 100},
    {"Installers", "exe,msi,msix,appx,appxbundle", "Installers", 100},
    {"Disk Images", "iso,img,bin,vhd,vhdx", "Disk Images", 100},
    {"Development", "py,js,ts,jsx,tsx,java,c,cpp,h,hpp,cs,go,rs,php,rb,swift,kt,kts,dart,lua,sql,html,htm,css,scss,sass,json,xml,yaml,yml,toml,ini,env,sh,bat,cmd,ps1", "Development", 100},
    {"3D & CAD", "blend,fbx,obj,stl,3ds,dae,gltf,glb,step,stp,iges,igs,dwg,dxf,skp", "3D & CAD", 100},
    {"Fonts", "ttf,otf,woff,woff2,eot", "Fonts", 100},
    {"Ebooks", "epub,mobi,azw,azw3,cbz,cbr", "Ebooks", 100},
    {"Database", "db,sqlite,sqlite3,mdb,accdb,dbf", "Database", 100},
    {"Backups", "bak,old,backup", "Backups", 100},
    {"Torrents", "torrent", "Torrents", 100},
    {"Shortcuts", "lnk,url", "Shortcuts", 100},
    {"Data", "vcf,ics", "Data", 100},
};

bool exec(QSqlDatabase &db, const QString &sql, const char *what) {
    QSqlQuery q(db);
    if (!q.exec(sql)) {
        qWarning() << "Bytes: schema statement failed (" << what << "):" << q.lastError().text();
        return false;
    }
    return true;
}

} // namespace

int DatabaseSchema::currentVersion() { return 1; }

bool DatabaseSchema::createTables(QSqlDatabase &db) {
    return exec(db, R"(
        CREATE TABLE IF NOT EXISTS settings (
            key   TEXT PRIMARY KEY,
            value TEXT
        )
    )", "settings") &&
    exec(db, R"(
        CREATE TABLE IF NOT EXISTS monitored_folders (
            id                   INTEGER PRIMARY KEY AUTOINCREMENT,
            path                 TEXT UNIQUE NOT NULL,
            enabled              INTEGER NOT NULL DEFAULT 1,
            destination_mode     TEXT NOT NULL DEFAULT 'subfolders',
            custom_destination   TEXT NOT NULL DEFAULT '',
            grace_period_seconds INTEGER NOT NULL DEFAULT 60,
            organize_existing    INTEGER NOT NULL DEFAULT 0,
            files_processed      INTEGER NOT NULL DEFAULT 0,
            created_at           TEXT NOT NULL DEFAULT (datetime('now'))
        )
    )", "monitored_folders") &&
    exec(db, R"(
        CREATE TABLE IF NOT EXISTS categories (
            id           INTEGER PRIMARY KEY AUTOINCREMENT,
            name         TEXT UNIQUE NOT NULL,
            extensions   TEXT NOT NULL DEFAULT '',
            destination  TEXT NOT NULL DEFAULT '',
            icon         TEXT NOT NULL DEFAULT '',
            enabled      INTEGER NOT NULL DEFAULT 1,
            priority     INTEGER NOT NULL DEFAULT 100,
            built_in     INTEGER NOT NULL DEFAULT 0
        )
    )", "categories") &&
    exec(db, R"(
        CREATE TABLE IF NOT EXISTS rules (
            id             INTEGER PRIMARY KEY AUTOINCREMENT,
            name           TEXT NOT NULL,
            condition_type TEXT NOT NULL,
            condition_value TEXT NOT NULL DEFAULT '',
            action_type    TEXT NOT NULL DEFAULT 'move',
            action_value   TEXT NOT NULL DEFAULT '',
            priority       INTEGER NOT NULL DEFAULT 100,
            enabled        INTEGER NOT NULL DEFAULT 1,
            created_at     TEXT NOT NULL DEFAULT (datetime('now')),
            scope_folder   TEXT NOT NULL DEFAULT ''
        )
    )", "rules") &&
    exec(db, R"(
        CREATE TABLE IF NOT EXISTS file_operations (
            id            INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp     TEXT NOT NULL DEFAULT (datetime('now')),
            source_path   TEXT NOT NULL,
            dest_path     TEXT NOT NULL DEFAULT '',
            file_type     TEXT NOT NULL DEFAULT '',
            rule_name     TEXT NOT NULL DEFAULT '',
            status        TEXT NOT NULL,
            error_message TEXT NOT NULL DEFAULT '',
            undone        INTEGER NOT NULL DEFAULT 0
        )
    )", "file_operations") &&
    exec(db, R"(
        CREATE TABLE IF NOT EXISTS cleanup_operations (
            id            INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp     TEXT NOT NULL DEFAULT (datetime('now')),
            target        TEXT NOT NULL,
            bytes_freed   INTEGER NOT NULL DEFAULT 0,
            files_deleted INTEGER NOT NULL DEFAULT 0,
            files_skipped INTEGER NOT NULL DEFAULT 0,
            status        TEXT NOT NULL DEFAULT 'success'
        )
    )", "cleanup_operations") &&
    exec(db, R"(
        CREATE TABLE IF NOT EXISTS errors (
            id        INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp TEXT NOT NULL DEFAULT (datetime('now')),
            context   TEXT NOT NULL DEFAULT '',
            file_path TEXT NOT NULL DEFAULT '',
            message   TEXT NOT NULL DEFAULT ''
        )
    )", "errors") &&
    exec(db, R"(
        CREATE TABLE IF NOT EXISTS statistics (
            id               INTEGER PRIMARY KEY AUTOINCREMENT,
            date             TEXT UNIQUE NOT NULL,
            files_organized  INTEGER NOT NULL DEFAULT 0,
            folders_created  INTEGER NOT NULL DEFAULT 0,
            files_skipped    INTEGER NOT NULL DEFAULT 0,
            errors           INTEGER NOT NULL DEFAULT 0,
            bytes_cleaned    INTEGER NOT NULL DEFAULT 0
        )
    )", "statistics") &&
    exec(db, "CREATE INDEX IF NOT EXISTS idx_file_ops_timestamp ON file_operations(timestamp)", "idx_file_ops_timestamp") &&
    exec(db, "CREATE INDEX IF NOT EXISTS idx_file_ops_status ON file_operations(status)", "idx_file_ops_status") &&
    exec(db, "CREATE INDEX IF NOT EXISTS idx_rules_priority ON rules(priority)", "idx_rules_priority");
}

void DatabaseSchema::migrateColumnsIfMissing(QSqlDatabase &db) {
    // Databases created before a column existed need it added explicitly --
    // CREATE TABLE IF NOT EXISTS is a no-op once the table already exists.
    // ALTER TABLE ... ADD COLUMN fails harmlessly if the column is already
    // there, which exec() logs but callers here intentionally ignore.
    QSqlQuery q(db);
    q.exec("ALTER TABLE rules ADD COLUMN scope_folder TEXT NOT NULL DEFAULT ''");
}

bool DatabaseSchema::seedDefaultCategories(QSqlDatabase &db) {
    QSqlQuery count(db);
    count.exec("SELECT COUNT(*) FROM categories");
    if (count.next() && count.value(0).toInt() > 0)
        return true; // already seeded

    QSqlQuery insert(db);
    insert.prepare(R"(
        INSERT INTO categories (name, extensions, destination, icon, enabled, priority, built_in)
        VALUES (:name, :extensions, :destination, '', 1, :priority, 1)
    )");

    for (const auto &c : kSeedCategories) {
        insert.bindValue(":name", QString::fromLatin1(c.name));
        insert.bindValue(":extensions", QString::fromLatin1(c.extensions));
        insert.bindValue(":destination", QString::fromLatin1(c.destination));
        insert.bindValue(":priority", c.priority);
        if (!insert.exec()) {
            qWarning() << "Bytes: failed to seed category" << c.name << insert.lastError().text();
            return false;
        }
    }
    return true;
}

bool DatabaseSchema::seedDefaultRules(QSqlDatabase &db) {
    QSqlQuery count(db);
    count.exec("SELECT COUNT(*) FROM rules");
    if (count.next() && count.value(0).toInt() > 0)
        return true; // user already has rules -- never insert over them

    // A single, sensible starter rule: don't reorganize desktop shortcuts
    // or installer icons out from under the user. High priority so it is
    // checked well before the generic "Shortcuts" category. Fully visible
    // and editable/removable from the Rules page like any other rule.
    QSqlQuery insert(db);
    insert.prepare(R"(
        INSERT INTO rules (name, condition_type, condition_value, action_type, action_value, priority, enabled)
        VALUES (:name, 'extension', :ext, 'skip', '', :priority, 1)
    )");
    insert.bindValue(":name", "Ignore Shortcuts & Installed Program Icons");
    insert.bindValue(":ext", "lnk,url");
    insert.bindValue(":priority", 900);
    if (!insert.exec()) {
        qWarning() << "Bytes: failed to seed default rule" << insert.lastError().text();
        return false;
    }
    return true;
}

bool DatabaseSchema::seedDefaultSettings(QSqlDatabase &db) {
    static const QVector<QPair<QString, QString>> defaults = {
        {"first_run_complete", "0"},
        {"start_with_windows", "0"},
        {"minimize_to_tray", "1"},
        {"notifications_enabled", "1"},
        {"notify_on_batch_only", "1"},
        {"grace_period_seconds", "60"},
        {"stability_check_interval_ms", "2000"},
        {"stability_check_count", "3"},
        {"duplicate_handling", "rename"}, // rename | skip | ask | replace
        {"dry_run_enabled", "0"},
        {"retry_count", "10"},
        {"retry_delay_ms", "500"},
        {"cleaner_schedule", "daily"}, // off | daily | weekly | monthly
        {"cleaner_schedule_time", "02:00"},
        {"cleaner_schedule_hours", "6"},
        {"cleaner_user_temp_enabled", "1"},
        {"cleaner_windows_temp_enabled", "1"},
        {"cleaner_recycle_bin_enabled", "0"},
        {"appearance_theme", "system"}, // light | dark | system
        {"markdown_logging_enabled", "1"},
        {"log_retention_days", "180"}, // 30/90/180/365/0(forever)
        {"debug_logging_enabled", "0"},
        {"use_bytes_folder_icons", "1"},
        {"worker_count", "2"},
        {"debounce_interval_ms", "750"},
        {"operation_timeout_ms", "30000"},
        {"file_signature_detection", "1"},
        {"long_path_support", "1"},
        {"close_window_to_tray", "1"},
        {"schema_version", QString::number(DatabaseSchema::currentVersion())},
    };

    QSqlQuery insert(db);
    insert.prepare("INSERT OR IGNORE INTO settings (key, value) VALUES (:k, :v)");
    for (const auto &kv : defaults) {
        insert.bindValue(":k", kv.first);
        insert.bindValue(":v", kv.second);
        if (!insert.exec()) {
            qWarning() << "Bytes: failed to seed setting" << kv.first << insert.lastError().text();
            return false;
        }
    }
    return true;
}

int DatabaseSchema::readSchemaVersion(QSqlDatabase &db) {
    QSqlQuery q(db);
    q.exec("SELECT value FROM settings WHERE key = 'schema_version'");
    if (q.next())
        return q.value(0).toInt();
    return 0;
}

void DatabaseSchema::writeSchemaVersion(QSqlDatabase &db, int version) {
    QSqlQuery q(db);
    q.prepare("INSERT INTO settings (key, value) VALUES ('schema_version', :v) "
              "ON CONFLICT(key) DO UPDATE SET value = :v2");
    q.bindValue(":v", QString::number(version));
    q.bindValue(":v2", QString::number(version));
    q.exec();
}

bool DatabaseSchema::initialize(QSqlDatabase &db) {
    if (!db.isOpen()) {
        qWarning() << "Bytes: cannot initialize schema, database is not open";
        return false;
    }

    QSqlQuery pragma(db);
    pragma.exec("PRAGMA foreign_keys = ON");
    pragma.exec("PRAGMA journal_mode = WAL");

    if (!createTables(db))
        return false;
    migrateColumnsIfMissing(db);
    if (!seedDefaultCategories(db))
        return false;
    if (!seedDefaultRules(db))
        return false;
    if (!seedDefaultSettings(db))
        return false;

    writeSchemaVersion(db, currentVersion());
    return true;
}

} // namespace bytes::database
