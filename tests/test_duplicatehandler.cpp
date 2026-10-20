#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include "core/DuplicateHandler.h"

using namespace bytes::core;

/// Covers: duplicate file naming ("photo (1).jpg", ...) and each
/// duplicate policy's behavior, including that "Replace" never actually
/// overwrites (Bytes's no-data-loss guarantee).
class TestDuplicateHandler : public QObject {
    Q_OBJECT
private slots:
    void noCollisionReturnsOriginalName();
    void collisionRenames();
    void multipleCollisionsIncrementCounter();
    void skipPolicyDoesNotProceed();
    void replacePolicyNeverOverwrites();
    void askPolicyRequestsPrompt();

private:
    static void touch(const QString &path);
};

void TestDuplicateHandler::touch(const QString &path) {
    QFile f(path);
    f.open(QIODevice::WriteOnly);
    f.write("x");
}

void TestDuplicateHandler::noCollisionReturnsOriginalName() {
    QTemporaryDir dir;
    QCOMPARE(DuplicateHandler::nextAvailableName(dir.path(), "photo.jpg"), QString("photo.jpg"));
}

void TestDuplicateHandler::collisionRenames() {
    QTemporaryDir dir;
    touch(dir.filePath("photo.jpg"));
    QCOMPARE(DuplicateHandler::nextAvailableName(dir.path(), "photo.jpg"), QString("photo (1).jpg"));
}

void TestDuplicateHandler::multipleCollisionsIncrementCounter() {
    QTemporaryDir dir;
    touch(dir.filePath("photo.jpg"));
    touch(dir.filePath("photo (1).jpg"));
    touch(dir.filePath("photo (2).jpg"));
    QCOMPARE(DuplicateHandler::nextAvailableName(dir.path(), "photo.jpg"), QString("photo (3).jpg"));
}

void TestDuplicateHandler::skipPolicyDoesNotProceed() {
    QTemporaryDir dir;
    touch(dir.filePath("a.txt"));
    const auto res = DuplicateHandler::resolve(dir.path(), "a.txt", DuplicatePolicy::Skip);
    QVERIFY(!res.proceed);
}

void TestDuplicateHandler::replacePolicyNeverOverwrites() {
    QTemporaryDir dir;
    touch(dir.filePath("a.txt"));
    const auto res = DuplicateHandler::resolve(dir.path(), "a.txt", DuplicatePolicy::Replace);
    QVERIFY(res.proceed);
    QVERIFY(res.finalFileName != "a.txt"); // renamed aside, original never overwritten
}

void TestDuplicateHandler::askPolicyRequestsPrompt() {
    QTemporaryDir dir;
    touch(dir.filePath("a.txt"));
    const auto res = DuplicateHandler::resolve(dir.path(), "a.txt", DuplicatePolicy::Ask);
    QVERIFY(res.needsUserPrompt);
    QVERIFY(!res.proceed);
}

QTEST_MAIN(TestDuplicateHandler)
#include "test_duplicatehandler.moc"
