#include "Database.h"
#include "DatabaseSchema.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>
#include <QUuid>
#include <QDebug>
#include <QVariant>

namespace bytes::database {

namespace {

// SQLite (correctly) rejects binding SQL NULL into a NOT NULL column, but a
// default-constructed QString() is "null" as far as Qt/QVariant is
// concerned, not merely empty -- and plenty of our structs' QString fields
// are left at their default when a caller only fills in the fields they
// care about. Coalesce to an empty-but-non-null string at the bind site so
// "field never set" always means "empty string" in the database, matching
// what every read-path already treats it as.
QString nz(const QString &s) { return s.isNull() ? QString("") : s; }

QString appDataRoot() {
    // %LOCALAPPDATA%\Bytes  -- never inside Downloads/Documents/Desktop.
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    // AppLocalDataLocation is typically %LOCALAPPDATA%/<OrgName>/<AppName>;
    // we want a clean %LOCALAPPDATA%\Bytes regardless of org name, so build
    // it explicitly from the generic location instead.
    const QString generic = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    QString root = generic.isEmpty() ? base : generic + "/Bytes";
    if (root.isEmpty())
        root = base;
    return root;
}

Category categoryFromQuery(const QSqlQuery &q) {
    Category c;
    c.id = q.value("id").toInt();
    c.name = q.value("name").toString();
    c.extensions = q.value("extensions").toString().split(',', Qt::SkipEmptyParts);
    c.destination = q.value("destination").toString();
    c.icon = q.value("icon").toString();
    c.enabled = q.value("enabled").toBool();
    c.priority = q.value("priority").toInt();
    c.builtIn = q.value("built_in").toBool();
    return c;
}

Rule ruleFromQuery(const QSqlQuery &q) {
    Rule r;
    r.id = q.value("id").toInt();
    r.name = q.value("name").toString();
    r.conditionType = q.value("condition_type").toString();
    r.conditionValue = q.value("condition_value").toString();
    r.actionType = q.value("action_type").toString();
    r.actionValue = q.value("action_value").toString();
    r.priority = q.value("priority").toInt();
    r.enabled = q.value("enabled").toBool();
    r.createdAt = q.value("created_at").toDateTime();
    r.scopeFolder = q.value("scope_folder").toString();
    return r;
}

MonitoredFolder folderFromQuery(const QSqlQuery &q) {
    MonitoredFolder f;
    f.id = q.value("id").toInt();
    f.path = q.value("path").toString();
    f.enabled = q.value("enabled").toBool();
    f.destinationMode = q.value("destination_mode").toString();
    f.customDestination = q.value("custom_destination").toString();
    f.gracePeriodSeconds = q.value("grace_period_seconds").toInt();
    f.organizeExisting = q.value("organize_existing").toBool();
    f.filesProcessed = q.value("files_processed").toLongLong();
    f.createdAt = q.value("created_at").toDateTime();
    return f;
}

FileOperationRecord opFromQuery(const QSqlQuery &q) {
    FileOperationRecord r;
    r.id = q.value("id").toInt();
    r.timestamp = q.value("timestamp").toDateTime();
    r.sourcePath = q.value("source_path").toString();
    r.destPath = q.value("dest_path").toString();
    r.fileType = q.value("file_type").toString();
    r.ruleName = q.value("rule_name").toString();
    r.status = q.value("status").toString();
    r.errorMessage = q.value("error_message").toString();
    r.undone = q.value("undone").toBool();
    return r;
}
} // namespace

bool Database::open() {
    const QString root = appDataRoot();
    QDir().mkpath(root);
    QDir().mkpath(root + "/Logs");
    return openAt(root + "/bytes.db");
}

bool Database::openAt(const QString &filePath) {
    QMutexLocker lock(&m_mutex);

    m_dbPath = filePath;
    QDir().mkpath(QFileInfo(filePath).absolutePath());

    m_connectionName = QStringLiteral("bytes_%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    m_db = QSqlDatabase::addDatabase("QSQLITE", m_connectionName);
    m_db.setDatabaseName(m_dbPath);

    if (!m_db.open()) {
        qWarning() << "Bytes: failed to open database at" << m_dbPath << m_db.lastError().text();
        return false;
    }

    if (!DatabaseSchema::initialize(m_db)) {
        qWarning() << "Bytes: schema initialization failed";
        return false;
    }

    return true;
}

void Database::close() {
    QMutexLocker lock(&m_mutex);
    if (m_db.isOpen())
        m_db.close();
    m_db = QSqlDatabase();
    QSqlDatabase::removeDatabase(m_connectionName);
}

bool Database::isOpen() const {
    QMutexLocker lock(&m_mutex);
    return m_db.isOpen();
}

// ---------------------------------------------------------------- settings

QString Database::getSetting(const QString &key, const QString &defaultValue) const {
    QMutexLocker lock(&m_mutex);
    QSqlQuery q(m_db);
    q.prepare("SELECT value FROM settings WHERE key = :k");
    q.bindValue(":k", key);
    if (q.exec() && q.next())
        return q.value(0).toString();
    return defaultValue;
}

bool Database::setSetting(const QString &key, const QString &value) {
    QMutexLocker lock(&m_mutex);
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO settings (key, value) VALUES (:k, :v) "
              "ON CONFLICT(key) DO UPDATE SET value = excluded.value");
    q.bindValue(":k", key);
    q.bindValue(":v", value);
    if (!q.exec()) {
        qWarning() << "Bytes: setSetting failed" << q.lastError().text();
        return false;
    }
    return true;
}

int Database::getSettingInt(const QString &key, int defaultValue) const {
    bool ok = false;
    const int v = getSetting(key).toInt(&ok);
    return ok ? v : defaultValue;
}

bool Database::getSettingBool(const QString &key, bool defaultValue) const {
    const QString v = getSetting(key);
    if (v.isEmpty())
        return defaultValue;
    return v == "1" || v.compare("true", Qt::CaseInsensitive) == 0;
}

// -------------------------------------------------------- monitored folders

QVector<MonitoredFolder> Database::monitoredFolders() const {
    QMutexLocker lock(&m_mutex);
    QVector<MonitoredFolder> out;
    QSqlQuery q(m_db);
    q.exec("SELECT * FROM monitored_folders ORDER BY id ASC");
    while (q.next())
        out.push_back(folderFromQuery(q));
    return out;
}

std::optional<MonitoredFolder> Database::monitoredFolder(int id) const {
    QMutexLocker lock(&m_mutex);
    QSqlQuery q(m_db);
    q.prepare("SELECT * FROM monitored_folders WHERE id = :id");
    q.bindValue(":id", id);
    if (q.exec() && q.next())
        return folderFromQuery(q);
    return std::nullopt;
}

int Database::addMonitoredFolder(const MonitoredFolder &folder) {
    QMutexLocker lock(&m_mutex);
    QSqlQuery q(m_db);
    q.prepare(R"(
        INSERT INTO monitored_folders
            (path, enabled, destination_mode, custom_destination, grace_period_seconds, organize_existing)
        VALUES (:path, :enabled, :mode, :custom, :grace, :existing)
    )");
    q.bindValue(":path", folder.path);
    q.bindValue(":enabled", folder.enabled ? 1 : 0);
    q.bindValue(":mode", folder.destinationMode);
    q.bindValue(":custom", nz(folder.customDestination));
    q.bindValue(":grace", folder.gracePeriodSeconds);
    q.bindValue(":existing", folder.organizeExisting ? 1 : 0);
    if (!q.exec()) {
        qWarning() << "Bytes: addMonitoredFolder failed" << q.lastError().text();
        return -1;
    }
    return q.lastInsertId().toInt();
}

bool Database::updateMonitoredFolder(const MonitoredFolder &folder) {
    QMutexLocker lock(&m_mutex);
    QSqlQuery q(m_db);
    q.prepare(R"(
        UPDATE monitored_folders SET
            path = :path, enabled = :enabled, destination_mode = :mode,
            custom_destination = :custom, grace_period_seconds = :grace,
            organize_existing = :existing
        WHERE id = :id
    )");
    q.bindValue(":path", folder.path);
    q.bindValue(":enabled", folder.enabled ? 1 : 0);
    q.bindValue(":mode", folder.destinationMode);
    q.bindValue(":custom", nz(folder.customDestination));
    q.bindValue(":grace", folder.gracePeriodSeconds);
    q.bindValue(":existing", folder.organizeExisting ? 1 : 0);
    q.bindValue(":id", folder.id);
    return q.exec();
}

bool Database::removeMonitoredFolder(int id) {
    QMutexLocker lock(&m_mutex);
    QSqlQuery q(m_db);
    q.prepare("DELETE FROM monitored_folders WHERE id = :id");
    q.bindValue(":id", id);
    return q.exec();
}

bool Database::setMonitoredFolderEnabled(int id, bool enabled) {
    QMutexLocker lock(&m_mutex);
    QSqlQuery q(m_db);
    q.prepare("UPDATE monitored_folders SET enabled = :e WHERE id = :id");
    q.bindValue(":e", enabled ? 1 : 0);
    q.bindValue(":id", id);
    return q.exec();
}

bool Database::incrementFilesProcessed(int folderId, qint64 delta) {
    QMutexLocker lock(&m_mutex);
    QSqlQuery q(m_db);
    q.prepare("UPDATE monitored_folders SET files_processed = files_processed + :d WHERE id = :id");
    q.bindValue(":d", delta);
    q.bindValue(":id", folderId);
    return q.exec();
}

// -------------------------------------------------------------- categories

QVector<Category> Database::categories() const {
    QMutexLocker lock(&m_mutex);
    QVector<Category> out;
    QSqlQuery q(m_db);
    q.exec("SELECT * FROM categories ORDER BY priority DESC, name ASC");
    while (q.next())
        out.push_back(categoryFromQuery(q));
    return out;
}

int Database::addCategory(const Category &category) {
    QMutexLocker lock(&m_mutex);
    QSqlQuery q(m_db);
    q.prepare(R"(
        INSERT INTO categories (name, extensions, destination, icon, enabled, priority, built_in)
        VALUES (:name, :ext, :dest, :icon, :enabled, :prio, 0)
    )");
    q.bindValue(":name", category.name);
    q.bindValue(":ext", category.extensions.join(','));
    q.bindValue(":dest", nz(category.destination));
    q.bindValue(":icon", nz(category.icon));
    q.bindValue(":enabled", category.enabled ? 1 : 0);
    q.bindValue(":prio", category.priority);
    if (!q.exec()) {
        qWarning() << "Bytes: addCategory failed" << q.lastError().text();
        return -1;
    }
    return q.lastInsertId().toInt();
}

bool Database::updateCategory(const Category &category) {
    QMutexLocker lock(&m_mutex);
    QSqlQuery q(m_db);
    q.prepare(R"(
        UPDATE categories SET name=:name, extensions=:ext, destination=:dest,
            icon=:icon, enabled=:enabled, priority=:prio
        WHERE id=:id
    )");
    q.bindValue(":name", category.name);
    q.bindValue(":ext", category.extensions.join(','));
    q.bindValue(":dest", nz(category.destination));
    q.bindValue(":icon", nz(category.icon));
    q.bindValue(":enabled", category.enabled ? 1 : 0);
    q.bindValue(":prio", category.priority);
    q.bindValue(":id", category.id);
    return q.exec();
}

bool Database::removeCategory(int id) {
    QMutexLocker lock(&m_mutex);
    QSqlQuery q(m_db);
    q.prepare("DELETE FROM categories WHERE id = :id AND built_in = 0");
    q.bindValue(":id", id);
    return q.exec();
}

// ------------------------------------------------------------------- rules

QVector<Rule> Database::rules() const {
    QMutexLocker lock(&m_mutex);
    QVector<Rule> out;
    QSqlQuery q(m_db);
    q.exec("SELECT * FROM rules ORDER BY priority DESC, id ASC");
    while (q.next())
        out.push_back(ruleFromQuery(q));
    return out;
}

int Database::addRule(const Rule &rule) {
    QMutexLocker lock(&m_mutex);
    QSqlQuery q(m_db);
    q.prepare(R"(
        INSERT INTO rules (name, condition_type, condition_value, action_type, action_value, priority, enabled, scope_folder)
        VALUES (:name, :ct, :cv, :at, :av, :prio, :enabled, :scope)
    )");
    q.bindValue(":name", rule.name);
    q.bindValue(":ct", rule.conditionType);
    q.bindValue(":cv", nz(rule.conditionValue));
    q.bindValue(":at", rule.actionType);
    q.bindValue(":av", nz(rule.actionValue));
    q.bindValue(":prio", rule.priority);
    q.bindValue(":enabled", rule.enabled ? 1 : 0);
    q.bindValue(":scope", nz(rule.scopeFolder));
    if (!q.exec()) {
        qWarning() << "Bytes: addRule failed" << q.lastError().text();
        return -1;
    }
    return q.lastInsertId().toInt();
}

bool Database::updateRule(const Rule &rule) {
    QMutexLocker lock(&m_mutex);
    QSqlQuery q(m_db);
    q.prepare(R"(
        UPDATE rules SET name=:name, condition_type=:ct, condition_value=:cv,
            action_type=:at, action_value=:av, priority=:prio, enabled=:enabled,
            scope_folder=:scope
        WHERE id=:id
    )");
    q.bindValue(":name", rule.name);
    q.bindValue(":ct", rule.conditionType);
    q.bindValue(":cv", nz(rule.conditionValue));
    q.bindValue(":at", rule.actionType);
    q.bindValue(":av", nz(rule.actionValue));
    q.bindValue(":prio", rule.priority);
    q.bindValue(":enabled", rule.enabled ? 1 : 0);
    q.bindValue(":scope", nz(rule.scopeFolder));
    q.bindValue(":id", rule.id);
    return q.exec();
}

bool Database::removeRule(int id) {
    QMutexLocker lock(&m_mutex);
    QSqlQuery q(m_db);
    q.prepare("DELETE FROM rules WHERE id = :id");
    q.bindValue(":id", id);
    return q.exec();
}

bool Database::setRuleEnabled(int id, bool enabled) {
    QMutexLocker lock(&m_mutex);
    QSqlQuery q(m_db);
    q.prepare("UPDATE rules SET enabled = :e WHERE id = :id");
    q.bindValue(":e", enabled ? 1 : 0);
    q.bindValue(":id", id);
    return q.exec();
}

bool Database::setRulePriority(int id, int priority) {
    QMutexLocker lock(&m_mutex);
    QSqlQuery q(m_db);
    q.prepare("UPDATE rules SET priority = :p WHERE id = :id");
    q.bindValue(":p", priority);
    q.bindValue(":id", id);
    return q.exec();
}

// --------------------------------------------------------- file operations

int Database::recordFileOperation(const FileOperationRecord &record) {
    QMutexLocker lock(&m_mutex);
    QSqlQuery q(m_db);
    q.prepare(R"(
        INSERT INTO file_operations
            (source_path, dest_path, file_type, rule_name, status, error_message, undone)
        VALUES (:src, :dst, :type, :rule, :status, :err, :undone)
    )");
    q.bindValue(":src", record.sourcePath);
    q.bindValue(":dst", nz(record.destPath));
    q.bindValue(":type", nz(record.fileType));
    q.bindValue(":rule", nz(record.ruleName));
    q.bindValue(":status", record.status);
    q.bindValue(":err", nz(record.errorMessage));
    q.bindValue(":undone", record.undone ? 1 : 0);
    if (!q.exec()) {
        qWarning() << "Bytes: recordFileOperation failed" << q.lastError().text();
        return -1;
    }
    return q.lastInsertId().toInt();
}

bool Database::markOperationUndone(int id) {
    QMutexLocker lock(&m_mutex);
    QSqlQuery q(m_db);
    q.prepare("UPDATE file_operations SET undone = 1 WHERE id = :id");
    q.bindValue(":id", id);
    return q.exec();
}

QVector<FileOperationRecord> Database::recentFileOperations(int limit) const {
    QMutexLocker lock(&m_mutex);
    QVector<FileOperationRecord> out;
    QSqlQuery q(m_db);
    q.prepare("SELECT * FROM file_operations ORDER BY id DESC LIMIT :lim");
    q.bindValue(":lim", limit);
    q.exec();
    while (q.next())
        out.push_back(opFromQuery(q));
    return out;
}

QVector<FileOperationRecord> Database::fileOperationsFiltered(const QString &statusFilter,
                                                               const QString &searchText,
                                                               const QDateTime &from,
                                                               const QDateTime &to,
                                                               int limit) const {
    QMutexLocker lock(&m_mutex);
    QVector<FileOperationRecord> out;
    QString sql = "SELECT * FROM file_operations WHERE 1=1";
    if (!statusFilter.isEmpty() && statusFilter != "All")
        sql += " AND status = :status";
    if (!searchText.isEmpty())
        sql += " AND (source_path LIKE :search OR dest_path LIKE :search)";
    if (from.isValid())
        sql += " AND timestamp >= :from";
    if (to.isValid())
        sql += " AND timestamp <= :to";
    sql += " ORDER BY id DESC LIMIT :lim";

    QSqlQuery q(m_db);
    q.prepare(sql);
    if (!statusFilter.isEmpty() && statusFilter != "All")
        q.bindValue(":status", statusFilter);
    if (!searchText.isEmpty())
        q.bindValue(":search", "%" + searchText + "%");
    if (from.isValid())
        q.bindValue(":from", from.toString(Qt::ISODate));
    if (to.isValid())
        q.bindValue(":to", to.toString(Qt::ISODate));
    q.bindValue(":lim", limit);

    if (!q.exec()) {
        qWarning() << "Bytes: fileOperationsFiltered failed" << q.lastError().text();
        return out;
    }
    while (q.next())
        out.push_back(opFromQuery(q));
    return out;
}

std::optional<FileOperationRecord> Database::fileOperation(int id) const {
    QMutexLocker lock(&m_mutex);
    QSqlQuery q(m_db);
    q.prepare("SELECT * FROM file_operations WHERE id = :id");
    q.bindValue(":id", id);
    if (q.exec() && q.next())
        return opFromQuery(q);
    return std::nullopt;
}

// ---------------------------------------------------------------- cleanup

int Database::recordCleanupOperation(const CleanupRecord &record) {
    QMutexLocker lock(&m_mutex);
    QSqlQuery q(m_db);
    q.prepare(R"(
        INSERT INTO cleanup_operations (target, bytes_freed, files_deleted, files_skipped, status)
        VALUES (:target, :bytes, :deleted, :skipped, :status)
    )");
    q.bindValue(":target", record.target);
    q.bindValue(":bytes", record.bytesFreed);
    q.bindValue(":deleted", record.filesDeleted);
    q.bindValue(":skipped", record.filesSkipped);
    q.bindValue(":status", record.status);
    if (!q.exec()) {
        qWarning() << "Bytes: recordCleanupOperation failed" << q.lastError().text();
        return -1;
    }
    return q.lastInsertId().toInt();
}

QVector<CleanupRecord> Database::recentCleanupOperations(int limit) const {
    QMutexLocker lock(&m_mutex);
    QVector<CleanupRecord> out;
    QSqlQuery q(m_db);
    q.prepare("SELECT * FROM cleanup_operations ORDER BY id DESC LIMIT :lim");
    q.bindValue(":lim", limit);
    q.exec();
    while (q.next()) {
        CleanupRecord r;
        r.id = q.value("id").toInt();
        r.timestamp = q.value("timestamp").toDateTime();
        r.target = q.value("target").toString();
        r.bytesFreed = q.value("bytes_freed").toLongLong();
        r.filesDeleted = q.value("files_deleted").toLongLong();
        r.filesSkipped = q.value("files_skipped").toLongLong();
        r.status = q.value("status").toString();
        out.push_back(r);
    }
    return out;
}

// ------------------------------------------------------------------ errors

int Database::recordError(const ErrorRecord &record) {
    QMutexLocker lock(&m_mutex);
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO errors (context, file_path, message) VALUES (:ctx, :path, :msg)");
    q.bindValue(":ctx", nz(record.context));
    q.bindValue(":path", nz(record.filePath));
    q.bindValue(":msg", nz(record.message));
    if (!q.exec()) {
        qWarning() << "Bytes: recordError failed" << q.lastError().text();
        return -1;
    }
    return q.lastInsertId().toInt();
}

QVector<ErrorRecord> Database::recentErrors(int limit) const {
    QMutexLocker lock(&m_mutex);
    QVector<ErrorRecord> out;
    QSqlQuery q(m_db);
    q.prepare("SELECT * FROM errors ORDER BY id DESC LIMIT :lim");
    q.bindValue(":lim", limit);
    q.exec();
    while (q.next()) {
        ErrorRecord r;
        r.id = q.value("id").toInt();
        r.timestamp = q.value("timestamp").toDateTime();
        r.context = q.value("context").toString();
        r.filePath = q.value("file_path").toString();
        r.message = q.value("message").toString();
        out.push_back(r);
    }
    return out;
}

// -------------------------------------------------------------- statistics

DayStatistics Database::statisticsForDate(const QString &yyyyMMdd) const {
    QMutexLocker lock(&m_mutex);
    DayStatistics s;
    s.date = yyyyMMdd;
    QSqlQuery q(m_db);
    q.prepare("SELECT * FROM statistics WHERE date = :d");
    q.bindValue(":d", yyyyMMdd);
    if (q.exec() && q.next()) {
        s.filesOrganized = q.value("files_organized").toLongLong();
        s.foldersCreated = q.value("folders_created").toLongLong();
        s.filesSkipped = q.value("files_skipped").toLongLong();
        s.errors = q.value("errors").toLongLong();
        s.bytesCleaned = q.value("bytes_cleaned").toLongLong();
    }
    return s;
}

void Database::bumpStatistic(const QString &yyyyMMdd, const char *column, qint64 delta) {
    QMutexLocker lock(&m_mutex);
    QSqlQuery insert(m_db);
    insert.prepare("INSERT OR IGNORE INTO statistics (date) VALUES (:d)");
    insert.bindValue(":d", yyyyMMdd);
    insert.exec();

    QSqlQuery q(m_db);
    // column name is developer-controlled (never user input), so this
    // string composition is safe; the value itself is still bound.
    q.prepare(QString("UPDATE statistics SET %1 = %1 + :delta WHERE date = :d").arg(column));
    q.bindValue(":delta", delta);
    q.bindValue(":d", yyyyMMdd);
    if (!q.exec())
        qWarning() << "Bytes: bumpStatistic failed" << q.lastError().text();
}

qint64 Database::totalFilesOrganized() const {
    QMutexLocker lock(&m_mutex);
    QSqlQuery q(m_db);
    q.exec("SELECT COUNT(*) FROM file_operations WHERE status = 'moved'");
    if (q.next())
        return q.value(0).toLongLong();
    return 0;
}

qint64 Database::filesOrganizedSince(const QDateTime &since) const {
    QMutexLocker lock(&m_mutex);
    QSqlQuery q(m_db);
    q.prepare("SELECT COUNT(*) FROM file_operations WHERE status = 'moved' AND timestamp >= :since");
    q.bindValue(":since", since.toString(Qt::ISODate));
    q.exec();
    if (q.next())
        return q.value(0).toLongLong();
    return 0;
}

// ----------------------------------------------------------------- maint.

void Database::purgeOldActivity(int retentionDays) {
    if (retentionDays <= 0)
        return; // "Forever" — never purge
    QMutexLocker lock(&m_mutex);
    const QString cutoff = QDateTime::currentDateTime().addDays(-retentionDays).toString(Qt::ISODate);
    QSqlQuery q(m_db);
    q.prepare("DELETE FROM file_operations WHERE timestamp < :cutoff");
    q.bindValue(":cutoff", cutoff);
    q.exec();
    QSqlQuery q2(m_db);
    q2.prepare("DELETE FROM cleanup_operations WHERE timestamp < :cutoff");
    q2.bindValue(":cutoff", cutoff);
    q2.exec();
    QSqlQuery q3(m_db);
    q3.prepare("DELETE FROM errors WHERE timestamp < :cutoff");
    q3.bindValue(":cutoff", cutoff);
    q3.exec();
}

bool Database::vacuum() {
    QMutexLocker lock(&m_mutex);
    QSqlQuery q(m_db);
    return q.exec("VACUUM");
}

bool Database::resetToDefaults() {
    QMutexLocker lock(&m_mutex);
    QSqlQuery(m_db).exec("DELETE FROM settings");
    QSqlQuery(m_db).exec("DELETE FROM rules");
    QSqlQuery(m_db).exec("DELETE FROM categories");
    return DatabaseSchema::initialize(m_db);
}

} // namespace bytes::database
