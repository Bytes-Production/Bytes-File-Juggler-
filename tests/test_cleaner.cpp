#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include "cleaner/TempCleaner.h"
#include "cleaner/RecycleBinManager.h"

#include <Windows.h>

using namespace bytes::cleaner;

/// Covers: temp cleaning deletes ordinary files, leaves locked files
/// alone (never force-closes them), and the Recycle Bin query never
/// crashes.
class TestCleaner : public QObject {
    Q_OBJECT
private slots:
    void calculatesSize();
    void cleansOrdinaryFiles();
    void skipsLockedFiles();
    void recycleBinQueryDoesNotCrash();
};

void TestCleaner::calculatesSize() {
    QTemporaryDir dir;
    QFile f(dir.filePath("a.bin"));
    f.open(QIODevice::WriteOnly);
    f.write(QByteArray(2048, 'x'));
    f.close();

    QCOMPARE(TempCleaner::calculateSize(dir.path()), qint64(2048));
}

void TestCleaner::cleansOrdinaryFiles() {
    QTemporaryDir dir;
    QFile f(dir.filePath("junk.tmp"));
    f.open(QIODevice::WriteOnly);
    f.write("junk");
    f.close();

    TempCleaner cleaner;
    const auto result = cleaner.clean(dir.path(), false);
    QCOMPARE(result.filesDeleted, qint64(1));
    QVERIFY(!QFile::exists(dir.filePath("junk.tmp")));
}

void TestCleaner::skipsLockedFiles() {
    QTemporaryDir dir;
    const QString path = dir.filePath("locked.tmp");
    QFile(path).open(QIODevice::WriteOnly);

    const std::wstring wpath = QDir::toNativeSeparators(path).toStdWString();
    HANDLE h = CreateFileW(wpath.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    QVERIFY(h != INVALID_HANDLE_VALUE);

    TempCleaner cleaner;
    const auto result = cleaner.clean(dir.path(), false);

    CloseHandle(h);

    QCOMPARE(result.filesDeleted, qint64(0));
    QCOMPARE(result.filesSkipped, qint64(1));
    QVERIFY(QFile::exists(path)); // never force-deleted
}

void TestCleaner::recycleBinQueryDoesNotCrash() {
    const auto info = RecycleBinManager::query();
    QVERIFY(info.itemCount >= 0);
    QVERIFY(info.totalBytes >= 0);
}

QTEST_MAIN(TestCleaner)
#include "test_cleaner.moc"
