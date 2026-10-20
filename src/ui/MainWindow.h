#pragma once

#include "../database/Database.h"
#include "../logging/ActivityLogger.h"
#include "../logging/MarkdownLogger.h"
#include "../core/FileOrganizer.h"
#include "../core/UndoManager.h"
#include "../cleaner/TempCleaner.h"
#include "../windows/TrayManager.h"
#include "../windows/FolderIconManager.h"

#include <QMainWindow>
#include <QListWidget>
#include <QStackedWidget>
#include <QLabel>

namespace bytes::ui {

class DashboardPage;
class JugglerPage;
class RulesPage;
class FoldersPage;
class CleanerPage;
class ActivityPage;
class SettingsPage;
class AboutPage;

/// Top-level application window: sidebar navigation + stacked pages, plus
/// the tray-minimize / single-instance-focus behavior described in the
/// Bytes spec.
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow(database::Database &db,
               logging::ActivityLogger &activityLogger,
               logging::MarkdownLogger &markdownLogger,
               core::FileOrganizer &organizer,
               windows::FolderIconManager &folderIconManager,
               QWidget *parent = nullptr);

    void navigateToActivity();
    void navigateToSettings();
    void navigateToDashboard();

public slots:
    void showAndRaise();

protected:
    void closeEvent(QCloseEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    database::Database &m_db;
    logging::ActivityLogger &m_activityLogger;
    logging::MarkdownLogger &m_markdownLogger;
    core::FileOrganizer &m_organizer;
    windows::FolderIconManager &m_folderIconManager;
    core::UndoManager m_undoManager;

    QListWidget *m_nav = nullptr;
    QStackedWidget *m_stack = nullptr;

    DashboardPage *m_dashboardPage = nullptr;
    JugglerPage *m_jugglerPage = nullptr;
    RulesPage *m_rulesPage = nullptr;
    FoldersPage *m_foldersPage = nullptr;
    CleanerPage *m_cleanerPage = nullptr;
    ActivityPage *m_activityPage = nullptr;
    SettingsPage *m_settingsPage = nullptr;
    AboutPage *m_aboutPage = nullptr;

    void buildUi();
    void applyStyle();
};

} // namespace bytes::ui
