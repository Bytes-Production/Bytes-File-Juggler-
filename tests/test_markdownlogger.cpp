#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QDate>
#include "logging/MarkdownLogger.h"

using namespace bytes::logging;

/// Covers: daily Markdown log file creation and append-safety (existing
/// content is never truncated or corrupted by a later append).
class TestMarkdownLogger : public QObject {
    Q_OBJECT
private slots:
    void createsDailyFile();
    void appendsWithoutCorrupting();
    void retentionPurgesOldFilesOnly();
};

void TestMarkdownLogger::createsDailyFile() {
    QTemporaryDir dir;
    MarkdownLogger logger(dir.path());
    logger.logFileMoved("setup.exe", "Installer", "C:/Downloads/setup.exe", "C:/Downloads/Installers/setup.exe", "Installer");

    const QString expectedPath = dir.filePath(QDate::currentDate().toString("yyyy-MM-dd") + ".md");
    QVERIFY(QFile::exists(expectedPath));

    QFile f(expectedPath);
    f.open(QIODevice::ReadOnly | QIODevice::Text);
    const QString content = QString::fromUtf8(f.readAll());
    QVERIFY(content.contains("setup.exe"));
    QVERIFY(content.contains("Moved successfully"));
}

void TestMarkdownLogger::appendsWithoutCorrupting() {
    QTemporaryDir dir;
    MarkdownLogger logger(dir.path());
    logger.logFileMoved("one.jpg", "Images", "C:/a", "C:/b/one.jpg", "Images");
    logger.logFileSkipped("two.zip", "C:/a", "Duplicate exists");
    logger.logError("three.txt", "File organization", "Access denied");

    const QString path = dir.filePath(QDate::currentDate().toString("yyyy-MM-dd") + ".md");
    QFile f(path);
    f.open(QIODevice::ReadOnly | QIODevice::Text);
    const QString content = QString::fromUtf8(f.readAll());

    QVERIFY(content.indexOf("one.jpg") < content.indexOf("two.zip"));
    QVERIFY(content.indexOf("two.zip") < content.indexOf("three.txt"));
}

void TestMarkdownLogger::retentionPurgesOldFilesOnly() {
    QTemporaryDir dir;
    const QString oldFile = dir.filePath("2000-01-01.md");
    const QString recentFile = dir.filePath(QDate::currentDate().toString("yyyy-MM-dd") + ".md");

    QFile(oldFile).open(QIODevice::WriteOnly);
    QFile(recentFile).open(QIODevice::WriteOnly);

    MarkdownLogger logger(dir.path());
    logger.purgeOldLogs(30);

    QVERIFY(!QFile::exists(oldFile));
    QVERIFY(QFile::exists(recentFile));
}

QTEST_MAIN(TestMarkdownLogger)
#include "test_markdownlogger.moc"
