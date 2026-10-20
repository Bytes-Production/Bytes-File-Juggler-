#include <QtTest>
#include <QTemporaryDir>
#include "database/Database.h"

using namespace bytes::database;

/// Covers: database open/schema init, settings round-trip, rules and
/// monitored-folder CRUD, and statistics bumping.
class TestDatabase : public QObject {
    Q_OBJECT
private slots:
    void init();
    void cleanup();

    void schemaSeedsBuiltInCategories();
    void settingsRoundTrip();
    void monitoredFolderCrud();
    void ruleCrudAndPriority();
    void statisticsAccumulate();
    void fileOperationRecordAndFilter();

private:
    QTemporaryDir *m_dir = nullptr;
    Database *m_db = nullptr;
};

void TestDatabase::init() {
    m_dir = new QTemporaryDir();
    m_db = new Database();
    QVERIFY(m_db->openAt(m_dir->filePath("test.db")));
}

void TestDatabase::cleanup() {
    m_db->close();
    delete m_db;
    delete m_dir;
}

void TestDatabase::schemaSeedsBuiltInCategories() {
    const auto categories = m_db->categories();
    QVERIFY(categories.size() >= 15);
    bool foundImages = false;
    for (const auto &c : categories)
        if (c.name == "Images")
            foundImages = true;
    QVERIFY(foundImages);
}

void TestDatabase::settingsRoundTrip() {
    QVERIFY(m_db->setSetting("some_key", "some_value"));
    QCOMPARE(m_db->getSetting("some_key"), QString("some_value"));
    QCOMPARE(m_db->getSetting("missing_key", "fallback"), QString("fallback"));

    m_db->setSetting("count", "42");
    QCOMPARE(m_db->getSettingInt("count", 0), 42);
}

void TestDatabase::monitoredFolderCrud() {
    MonitoredFolder folder;
    folder.path = "C:/Fake/Downloads";
    folder.enabled = true;
    folder.gracePeriodSeconds = 30;

    const int id = m_db->addMonitoredFolder(folder);
    QVERIFY(id > 0);

    auto fetched = m_db->monitoredFolder(id);
    QVERIFY(fetched.has_value());
    QCOMPARE(fetched->path, folder.path);

    QVERIFY(m_db->setMonitoredFolderEnabled(id, false));
    fetched = m_db->monitoredFolder(id);
    QVERIFY(!fetched->enabled);

    QVERIFY(m_db->removeMonitoredFolder(id));
    QVERIFY(!m_db->monitoredFolder(id).has_value());
}

void TestDatabase::ruleCrudAndPriority() {
    Rule r;
    r.name = "Test Rule";
    r.conditionType = "extension";
    r.conditionValue = "foo";
    r.actionType = "skip";
    r.priority = 200;
    r.enabled = true;

    const int id = m_db->addRule(r);
    QVERIFY(id > 0);
    QVERIFY(m_db->setRulePriority(id, 999));

    const auto rules = m_db->rules();
    QVERIFY(!rules.isEmpty());
    QCOMPARE(rules.first().priority, 999); // highest priority sorts first

    QVERIFY(m_db->removeRule(id));
}

void TestDatabase::statisticsAccumulate() {
    m_db->bumpStatistic("2026-01-01", "files_organized", 3);
    m_db->bumpStatistic("2026-01-01", "files_organized", 2);
    const auto stats = m_db->statisticsForDate("2026-01-01");
    QCOMPARE(stats.filesOrganized, 5);
}

void TestDatabase::fileOperationRecordAndFilter() {
    FileOperationRecord record;
    record.sourcePath = "C:/Fake/Downloads/photo.jpg";
    record.destPath = "C:/Fake/Downloads/Images/photo.jpg";
    record.fileType = "Images";
    record.status = "moved";
    const int id = m_db->recordFileOperation(record);
    QVERIFY(id > 0);

    const auto results = m_db->fileOperationsFiltered("moved", "photo", QDateTime(), QDateTime(), 10);
    QCOMPARE(results.size(), 1);
    QCOMPARE(results.first().id, id);

    QVERIFY(m_db->markOperationUndone(id));
    const auto fetched = m_db->fileOperation(id);
    QVERIFY(fetched->undone);
}

QTEST_MAIN(TestDatabase)
#include "test_database.moc"
