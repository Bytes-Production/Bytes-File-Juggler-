#include "TrayManager.h"

#include <QFont>

namespace bytes::windows {

TrayManager::TrayManager(const QIcon &icon, QObject *parent) : QObject(parent), m_trayIcon(icon) {
    m_trayIcon.setToolTip("Bytes — File Juggler & Cleaner");
    buildMenu();
    m_trayIcon.setContextMenu(&m_menu);

    connect(&m_trayIcon, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick)
            emit openRequested();
    });
}

void TrayManager::buildMenu() {
    QAction *title = m_menu.addAction("Bytes");
    QFont bold = title->font();
    bold.setBold(true);
    title->setFont(bold);
    title->setEnabled(false);

    m_statusAction = m_menu.addAction("● Monitoring Active");
    m_statusAction->setEnabled(false);

    m_menu.addSeparator();

    m_openAction = m_menu.addAction("Open Bytes");
    connect(m_openAction, &QAction::triggered, this, &TrayManager::openRequested);

    m_pauseAction = m_menu.addAction("Pause Juggler");
    connect(m_pauseAction, &QAction::triggered, this, &TrayManager::pauseRequested);

    m_resumeAction = m_menu.addAction("Resume Juggler");
    m_resumeAction->setVisible(false);
    connect(m_resumeAction, &QAction::triggered, this, &TrayManager::resumeRequested);

    m_runCleanupAction = m_menu.addAction("Run Cleanup");
    connect(m_runCleanupAction, &QAction::triggered, this, &TrayManager::runCleanupRequested);

    m_activityAction = m_menu.addAction("Open Activity");
    connect(m_activityAction, &QAction::triggered, this, &TrayManager::openActivityRequested);

    m_settingsAction = m_menu.addAction("Settings");
    connect(m_settingsAction, &QAction::triggered, this, &TrayManager::openSettingsRequested);

    m_menu.addSeparator();

    m_exitAction = m_menu.addAction("Exit");
    connect(m_exitAction, &QAction::triggered, this, &TrayManager::exitRequested);
}

void TrayManager::show() {
    m_trayIcon.show();
}

void TrayManager::setMonitoringActive(bool active) {
    m_statusAction->setText(active ? "● Monitoring Active" : "○ Monitoring Inactive");
}

void TrayManager::setPaused(bool paused) {
    m_pauseAction->setVisible(!paused);
    m_resumeAction->setVisible(paused);
    setMonitoringActive(!paused);
}

void TrayManager::showMessage(const QString &title, const QString &body, QSystemTrayIcon::MessageIcon icon) {
    if (QSystemTrayIcon::supportsMessages())
        m_trayIcon.showMessage(title, body, icon, 5000);
}

} // namespace bytes::windows
