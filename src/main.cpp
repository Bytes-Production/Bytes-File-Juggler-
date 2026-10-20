#include "database/Database.h"
#include "logging/MarkdownLogger.h"
#include "logging/ActivityLogger.h"
#include "core/FileOrganizer.h"
#include "windows/FolderIconManager.h"
#include "windows/TrayManager.h"
#include "windows/StartupManager.h"
#include "windows/ShellUtils.h"
#include "cleaner/TempCleaner.h"
#include "cleaner/RecycleBinManager.h"
#include "cleaner/CleanerScheduler.h"
#include "ui/MainWindow.h"
#include "ui/FirstRunWizard.h"

#include <QApplication>
#include <QLocalServer>
#include <QLocalSocket>
#include <QMessageBox>
#include <QPushButton>
#include <QIcon>
#include <QDir>
#include <QFileInfo>
#include <QCommandLineParser>
#include <QStandardPaths>

using namespace bytes;

namespace {

constexpr const char *kSingleInstanceKey = "BytesFileJugglerCleaner_SingleInstance";

/// Ensures only one Bytes instance runs at a time. If another instance is
/// already running, this signals it (so it can raise its window) and
/// returns false so the caller exits immediately.
bool acquireSingleInstance(QLocalServer &server, QObject *receiverForShowRequests) {
    QLocalSocket probe;
    probe.connectToServer(kSingleInstanceKey);
    if (probe.waitForConnected(200)) {
        probe.write("show");
        probe.waitForBytesWritten(200);
        probe.disconnectFromServer();
        return false; // another instance handled it
    }

    QLocalServer::removeServer(kSingleInstanceKey); // clean up a stale socket, if any
    if (!server.listen(kSingleInstanceKey))
        return true; // couldn't set up IPC, but proceed as the only instance anyway

    QObject::connect(&server, &QLocalServer::newConnection, receiverForShowRequests, [&server, receiverForShowRequests]() {
        QLocalSocket *client = server.nextPendingConnection();
        if (!client)
            return;
        QObject::connect(client, &QLocalSocket::readyRead, receiverForShowRequests, [client, receiverForShowRequests]() {
            client->readAll();
            QMetaObject::invokeMethod(receiverForShowRequests, "showAndRaise", Qt::QueuedConnection);
            client->deleteLater();
        });
    });
    return true;
}

void registerMetaTypes() {
    qRegisterMetaType<database::FileOperationRecord>("bytes::database::FileOperationRecord");
    qRegisterMetaType<database::CleanupRecord>("bytes::database::CleanupRecord");
    qRegisterMetaType<database::ErrorRecord>("bytes::database::ErrorRecord");
    qRegisterMetaType<core::FileChangeType>("bytes::core::FileChangeType");
}

} // namespace

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("Bytes");
    app.setApplicationDisplayName("Bytes — File Juggler & Cleaner");
    app.setOrganizationName("Bytes Team");
    app.setApplicationVersion("1.0.0");
    app.setWindowIcon(QIcon(":/icons/bytes.ico"));
    app.setQuitOnLastWindowClosed(false);

    registerMetaTypes();

    QCommandLineParser parser;
    parser.addHelpOption();
    QCommandLineOption trayOption("tray", "Start minimized to the system tray.");
    QCommandLineOption cleanupOption("cleanup-scheduled", "Run the scheduled cleanup and exit (used by Task Scheduler).");
    parser.addOption(trayOption);
    parser.addOption(cleanupOption);
    parser.process(app);

    database::Database db;
    if (!db.open()) {
        QMessageBox::critical(nullptr, "Bytes",
            "Bytes could not create or open its database and cannot continue.\n\n"
            "Please check that %LOCALAPPDATA%\\Bytes is writable.");
        return 1;
    }

    const QString localAppData = QFileInfo(db.databaseFilePath()).absolutePath();
    logging::MarkdownLogger markdownLogger(localAppData + "/Logs");
    logging::ActivityLogger activityLogger(db, markdownLogger);
    windows::FolderIconManager folderIconManager(":/icons/folder.ico", localAppData);

    // --cleanup-scheduled: perform the configured cleanup headlessly and exit
    // (this is what the Windows Task Scheduler entry invokes).
    if (parser.isSet(cleanupOption)) {
        cleaner::TempCleaner cleaner;
        if (db.getSettingBool("cleaner_user_temp_enabled", true)) {
            const auto r = cleaner.clean(cleaner::TempCleaner::userTempPath(), false);
            activityLogger.logCleanup("User Temp", r.bytesFreed, r.filesDeleted, r.filesSkipped, true);
        }
        if (db.getSettingBool("cleaner_windows_temp_enabled", true)) {
            const auto r = cleaner.clean(cleaner::TempCleaner::windowsTempPath(), false);
            activityLogger.logCleanup("Windows Temp", r.bytesFreed, r.filesDeleted, r.filesSkipped, true);
        }
        if (db.getSettingBool("cleaner_recycle_bin_enabled", false)) {
            const auto before = cleaner::RecycleBinManager::query();
            const bool ok = cleaner::RecycleBinManager::empty();
            activityLogger.logCleanup("Recycle Bin", ok ? before.totalBytes : 0, ok ? before.itemCount : 0, 0, ok);
        }
        db.close();
        return 0;
    }

    core::FileOrganizer organizer(db, activityLogger, folderIconManager);

    ui::MainWindow mainWindow(db, activityLogger, markdownLogger, organizer, folderIconManager);

    QLocalServer instanceServer;
    if (!acquireSingleInstance(instanceServer, &mainWindow)) {
        return 0; // another Bytes instance is already running and was notified
    }

    // First run: offer the setup wizard.
    const bool firstRun = !db.getSettingBool("first_run_complete", false);
    if (firstRun) {
        ui::FirstRunWizard wizard(&mainWindow);
        if (wizard.exec() == QDialog::Accepted) {
            const auto choices = wizard.choices();

            if (choices.monitorDownloads) {
                database::MonitoredFolder folder;
                folder.path = windows::ShellUtils::knownDownloadsFolder();
                folder.enabled = true;
                folder.gracePeriodSeconds = choices.gracePeriodSeconds;
                db.addMonitoredFolder(folder);
            }

            if (!choices.smartOrganization) {
                // "Custom Rules Only": start with the built-in categories
                // disabled so nothing moves until the user defines rules.
                for (auto category : db.categories()) {
                    category.enabled = false;
                    db.updateCategory(category);
                }
            }

            db.setSetting("grace_period_seconds", QString::number(choices.gracePeriodSeconds));
            db.setSetting("cleaner_user_temp_enabled", choices.cleanUserTemp ? "1" : "0");
            db.setSetting("cleaner_windows_temp_enabled", choices.cleanWindowsTemp ? "1" : "0");
            db.setSetting("cleaner_recycle_bin_enabled", choices.cleanRecycleBin ? "1" : "0");
            db.setSetting("start_with_windows", choices.startWithWindows ? "1" : "0");
            if (choices.startWithWindows)
                windows::StartupManager::setEnabled(true, QCoreApplication::applicationFilePath());

            cleaner::CleanerScheduler::apply(
                choices.cleanUserTemp || choices.cleanWindowsTemp ? cleaner::ScheduleFrequency::Daily : cleaner::ScheduleFrequency::Off,
                "02:00", QCoreApplication::applicationFilePath());
        }
        db.setSetting("first_run_complete", "1");
        organizer.reloadFolders();
    }

    organizer.start();

    // First-run safety net: if the newly monitored Downloads folder already
    // has a pile of files, ask before touching any of them (default: only
    // organize new files from now on).
    if (firstRun) {
        for (const auto &folder : db.monitoredFolders()) {
            const auto summary = organizer.summarizeExistingFiles(folder.path);
            if (summary.totalFiles > 0) {
                QMessageBox box(&mainWindow);
                box.setWindowTitle("Bytes");
                box.setText(QString("Bytes found %1 existing file(s) in %2.\n\n"
                                     "Do you want Bytes to organize them now, or only organize new files from now on?")
                                .arg(summary.totalFiles).arg(folder.path));
                QPushButton *organizeNow = box.addButton("Organize Existing Files", QMessageBox::AcceptRole);
                QPushButton *onlyNew = box.addButton("Only Organize New Files", QMessageBox::RejectRole);
                box.setDefaultButton(onlyNew); // spec default: only organize new files from now on
                box.exec();
                if (box.clickedButton() == organizeNow)
                    organizer.organizeExistingFiles(folder.id);
            }
        }
    }

    windows::TrayManager tray(QIcon(":/icons/bytes.ico"));
    QObject::connect(&tray, &windows::TrayManager::openRequested, &mainWindow, &ui::MainWindow::showAndRaise);
    QObject::connect(&tray, &windows::TrayManager::pauseRequested, &app, [&organizer, &tray] {
        organizer.setPaused(true);
        tray.setPaused(true);
    });
    QObject::connect(&tray, &windows::TrayManager::resumeRequested, &app, [&organizer, &tray] {
        organizer.setPaused(false);
        tray.setPaused(false);
    });
    QObject::connect(&tray, &windows::TrayManager::openActivityRequested, &mainWindow, [&mainWindow] {
        mainWindow.showAndRaise();
        mainWindow.navigateToActivity();
    });
    QObject::connect(&tray, &windows::TrayManager::openSettingsRequested, &mainWindow, [&mainWindow] {
        mainWindow.showAndRaise();
        mainWindow.navigateToSettings();
    });
    QObject::connect(&tray, &windows::TrayManager::exitRequested, &app, &QApplication::quit);
    QObject::connect(&organizer, &core::FileOrganizer::monitoringStateChanged, &tray, &windows::TrayManager::setMonitoringActive);
    QObject::connect(&organizer, &core::FileOrganizer::batchNotificationReady, &tray,
                      [&tray, &db](int moved, int skipped, int errors) {
        Q_UNUSED(skipped);
        if (!db.getSettingBool("notifications_enabled", true))
            return;
        if (moved > 0)
            tray.showMessage("Bytes", QString("%1 file(s) organized successfully.").arg(moved));
        if (errors > 0)
            tray.showMessage("Bytes", QString("%1 file(s) could not be organized. See Activity for details.").arg(errors),
                              QSystemTrayIcon::Warning);
    });
    tray.show();

    // Periodic housekeeping: purge old logs/history per the retention setting.
    markdownLogger.purgeOldLogs(db.getSettingInt("log_retention_days", 180));
    db.purgeOldActivity(db.getSettingInt("log_retention_days", 180));

    if (!parser.isSet(trayOption))
        mainWindow.show();

    const int result = app.exec();
    organizer.stop();
    db.close();
    return result;
}
