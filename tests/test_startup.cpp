#include <QtTest>
#include "windows/StartupManager.h"

using namespace bytes::windows;

/// Covers: enabling/disabling "Start Bytes with Windows" via the per-user
/// registry Run key.
///
/// This test deliberately does NOT try to "restore" a pre-existing real
/// startup entry: this test binary's own path is not a faithful stand-in
/// for whatever the real Bytes.exe's entry pointed at, and writing it back
/// would silently corrupt a real user's startup entry to point at a
/// throwaway test executable. If the key was already enabled before this
/// test ran (e.g. a real Bytes install on the same machine), we leave it
/// disabled afterwards and say so, rather than guess.
class TestStartupManager : public QObject {
    Q_OBJECT
private slots:
    void enableThenDisableRoundTrip();
};

void TestStartupManager::enableThenDisableRoundTrip() {
    const bool wasEnabledBefore = StartupManager::isEnabled();
    if (wasEnabledBefore) {
        qWarning() << "Bytes startup was already enabled before this test ran; "
                       "it will be left DISABLED afterwards rather than guessed at.";
    }

    QVERIFY(StartupManager::setEnabled(true, "C:/Fake/Bytes.exe"));
    QVERIFY(StartupManager::isEnabled());

    QVERIFY(StartupManager::setEnabled(false, "C:/Fake/Bytes.exe"));
    QVERIFY(!StartupManager::isEnabled());
}

QTEST_MAIN(TestStartupManager)
#include "test_startup.moc"
