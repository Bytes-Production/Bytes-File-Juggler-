#pragma once

#include <QObject>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QIcon>

namespace bytes::windows {

/// Owns the Bytes system tray icon and its context menu, exactly matching
/// the layout specified for Bytes:
///   Bytes
///   ● Monitoring Active
///   Open Bytes / Pause Juggler / Resume Juggler / Run Cleanup /
///   Open Activity / Settings / Exit
class TrayManager : public QObject {
    Q_OBJECT
public:
    explicit TrayManager(const QIcon &icon, QObject *parent = nullptr);

    void show();
    void setMonitoringActive(bool active);
    void setPaused(bool paused);

    /// Shows a native Windows notification. Bytes batches these (see
    /// notifyBatch) rather than firing one per file to avoid spam.
    void showMessage(const QString &title, const QString &body,
                      QSystemTrayIcon::MessageIcon icon = QSystemTrayIcon::Information);

signals:
    void openRequested();
    void pauseRequested();
    void resumeRequested();
    void runCleanupRequested();
    void openActivityRequested();
    void openSettingsRequested();
    void exitRequested();

private:
    QSystemTrayIcon m_trayIcon;
    QMenu m_menu;

    QAction *m_statusAction = nullptr;
    QAction *m_openAction = nullptr;
    QAction *m_pauseAction = nullptr;
    QAction *m_resumeAction = nullptr;
    QAction *m_runCleanupAction = nullptr;
    QAction *m_activityAction = nullptr;
    QAction *m_settingsAction = nullptr;
    QAction *m_exitAction = nullptr;

    void buildMenu();
};

} // namespace bytes::windows
