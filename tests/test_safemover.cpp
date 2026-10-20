#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include "core/SafeMover.h"

#include <Windows.h>

using namespace bytes::core;

/// Covers: dry-run preview never moves a file, locked-file detection,
/// automatic destination folder creation, and duplicate-safe moving.
class TestSafeMover : public QObject {
    Q_OBJECT
private slots:
    void dryRunNeverMoves();
    void createsMissingDestination();
    void detectsLockedFile();
    void movesAndVerifies();
    void duplicateGetsRenamedNotOverwritten();
};

void TestSafeMover::dryRunNeverMoves() {
    QTemporaryDir dir;
    const QString source = dir.filePath("photo.jpg");
    QFile(source).open(QIODevice::WriteOnly);
    QFile(source).write("data");

    SafeMover mover;
    MoveRequest req;
    req.sourcePath = source;
    req.destinationDir = dir.filePath("Images");
    req.dryRun = true;

    const auto result = mover.moveFile(req);
    QCOMPARE(static_cast<int>(result.outcome), static_cast<int>(MoveOutcome::WouldMove));
    QVERIFY(QFile::exists(source)); // untouched
    QVERIFY(!QDir(dir.filePath("Images")).exists()); // dry run creates nothing
}

void TestSafeMover::createsMissingDestination() {
    QTemporaryDir dir;
    const QString source = dir.filePath("doc.txt");
    QFile f(source);
    f.open(QIODevice::WriteOnly);
    f.write("hello");
    f.close();

    SafeMover mover;
    MoveRequest req;
    req.sourcePath = source;
    req.destinationDir = dir.filePath("Documents/Word");

    const auto result = mover.moveFile(req);
    QCOMPARE(static_cast<int>(result.outcome), static_cast<int>(MoveOutcome::Moved));
    QVERIFY(QFile::exists(result.finalDestinationPath));
    QVERIFY(!QFile::exists(source));
}

void TestSafeMover::detectsLockedFile() {
    QTemporaryDir dir;
    const QString path = dir.filePath("locked.bin");
    QFile(path).open(QIODevice::WriteOnly);

    const std::wstring wpath = QDir::toNativeSeparators(path).toStdWString();
    HANDLE h = CreateFileW(wpath.c_str(), GENERIC_READ | GENERIC_WRITE, 0 /* no sharing */, nullptr,
                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    QVERIFY(h != INVALID_HANDLE_VALUE);

    QVERIFY(SafeMover::isFileLocked(path));

    CloseHandle(h);
    QVERIFY(!SafeMover::isFileLocked(path));
}

void TestSafeMover::movesAndVerifies() {
    QTemporaryDir dir;
    const QString source = dir.filePath("song.mp3");
    QFile f(source);
    f.open(QIODevice::WriteOnly);
    f.write(QByteArray(1024, 'x'));
    f.close();

    SafeMover mover;
    MoveRequest req;
    req.sourcePath = source;
    req.destinationDir = dir.filePath("Music");

    const auto result = mover.moveFile(req);
    QCOMPARE(static_cast<int>(result.outcome), static_cast<int>(MoveOutcome::Moved));
    QCOMPARE(QFileInfo(result.finalDestinationPath).size(), qint64(1024));
}

void TestSafeMover::duplicateGetsRenamedNotOverwritten() {
    QTemporaryDir dir;
    const QString destDir = dir.filePath("Images");
    QDir().mkpath(destDir);
    QFile existing(QDir(destDir).filePath("photo.jpg"));
    existing.open(QIODevice::WriteOnly);
    existing.write("original");
    existing.close();

    const QString source = dir.filePath("photo.jpg");
    QFile incoming(source);
    incoming.open(QIODevice::WriteOnly);
    incoming.write("new-incoming-data");
    incoming.close();

    SafeMover mover;
    MoveRequest req;
    req.sourcePath = source;
    req.destinationDir = destDir;
    req.duplicatePolicy = DuplicatePolicy::Rename;

    const auto result = mover.moveFile(req);
    QCOMPARE(static_cast<int>(result.outcome), static_cast<int>(MoveOutcome::Moved));
    QVERIFY(result.finalDestinationPath.contains("photo (1)"));

    // The original duplicate must be untouched.
    QFile original(QDir(destDir).filePath("photo.jpg"));
    original.open(QIODevice::ReadOnly);
    QCOMPARE(QString::fromUtf8(original.readAll()), QString("original"));
}

QTEST_MAIN(TestSafeMover)
#include "test_safemover.moc"
