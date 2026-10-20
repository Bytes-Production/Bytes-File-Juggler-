#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include "core/SafeMover.h"
#include "core/UndoManager.h"
#include "database/Database.h"

using namespace bytes::core;
using namespace bytes::database;

/// Covers: undoing a successful move restores the file to its original
/// location, and a collision at the original location produces a safely
/// suffixed "(restored)" name instead of overwriting anything.
class TestUndoManager : public QObject {
    Q_OBJECT
private slots:
    void init();
    void cleanup();

    void undoRestoresOriginalLocation();
    void undoWithCollisionUsesRestoredSuffix();
    void undoTwiceFailsSecondTime();

private:
    QTemporaryDir *m_dir = nullptr;
    Database *m_db = nullptr;
};

void TestUndoManager::init() {
    m_dir = new QTemporaryDir();
    m_db = new Database();
    QVERIFY(m_db->openAt(m_dir->filePath("test.db")));
}

void TestUndoManager::cleanup() {
    m_db->close();
    delete m_db;
    delete m_dir;
}

void TestUndoManager::undoRestoresOriginalLocation() {
    const QString source = m_dir->filePath("report.pdf");
    QFile f(source);
    f.open(QIODevice::WriteOnly);
    f.write("contents");
    f.close();

    SafeMover mover;
    MoveRequest req;
    req.sourcePath = source;
    req.destinationDir = m_dir->filePath("Documents/PDF");
    const auto moveResult = mover.moveFile(req);
    QCOMPARE(static_cast<int>(moveResult.outcome), static_cast<int>(MoveOutcome::Moved));

    FileOperationRecord record;
    record.sourcePath = source;
    record.destPath = moveResult.finalDestinationPath;
    record.status = "moved";
    const int id = m_db->recordFileOperation(record);

    UndoManager undo(*m_db);
    const auto outcome = undo.undoOperation(id);
    QVERIFY(outcome.success);
    QVERIFY(QFile::exists(source));
    QVERIFY(!QFile::exists(moveResult.finalDestinationPath));
}

void TestUndoManager::undoWithCollisionUsesRestoredSuffix() {
    const QString source = m_dir->filePath("notes.txt");
    QFile f(source);
    f.open(QIODevice::WriteOnly);
    f.write("v1");
    f.close();

    SafeMover mover;
    MoveRequest req;
    req.sourcePath = source;
    req.destinationDir = m_dir->filePath("Documents/Word");
    const auto moveResult = mover.moveFile(req);
    QVERIFY(static_cast<int>(moveResult.outcome) == static_cast<int>(MoveOutcome::Moved));

    // Something else now occupies the original path.
    QFile another(source);
    another.open(QIODevice::WriteOnly);
    another.write("a different file entirely");
    another.close();

    FileOperationRecord record;
    record.sourcePath = source;
    record.destPath = moveResult.finalDestinationPath;
    record.status = "moved";
    const int id = m_db->recordFileOperation(record);

    UndoManager undo(*m_db);
    const auto outcome = undo.undoOperation(id);
    QVERIFY(outcome.success);
    QVERIFY(outcome.restoredPath.contains("(restored)"));
    QVERIFY(QFile::exists(source)); // the "different file" is still there, untouched
}

void TestUndoManager::undoTwiceFailsSecondTime() {
    const QString source = m_dir->filePath("data.csv");
    QFile f(source);
    f.open(QIODevice::WriteOnly);
    f.write("a,b,c");
    f.close();

    SafeMover mover;
    MoveRequest req;
    req.sourcePath = source;
    req.destinationDir = m_dir->filePath("Documents/Spreadsheets");
    const auto moveResult = mover.moveFile(req);

    FileOperationRecord record;
    record.sourcePath = source;
    record.destPath = moveResult.finalDestinationPath;
    record.status = "moved";
    const int id = m_db->recordFileOperation(record);

    UndoManager undo(*m_db);
    QVERIFY(undo.undoOperation(id).success);
    QVERIFY(!undo.undoOperation(id).success); // already undone
}

QTEST_MAIN(TestUndoManager)
#include "test_undomanager.moc"
