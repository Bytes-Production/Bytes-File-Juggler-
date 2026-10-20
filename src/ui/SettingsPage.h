#pragma once

#include "../database/Database.h"
#include "../core/FileOrganizer.h"
#include "../logging/MarkdownLogger.h"

#include <QWidget>
#include <QCheckBox>
#include <QComboBox>
#include <QSpinBox>

namespace bytes::ui {

/// General / Juggler / Cleaner / Appearance / Logging / Advanced settings.
class SettingsPage : public QWidget {
    Q_OBJECT
public:
    SettingsPage(database::Database &db, core::FileOrganizer &organizer,
                 logging::MarkdownLogger &markdownLogger, QWidget *parent = nullptr);

signals:
    void appearanceChanged(QString theme);

private:
    database::Database &m_db;
    core::FileOrganizer &m_organizer;
    logging::MarkdownLogger &m_markdownLogger;

    // General
    QCheckBox *m_startWithWindows = nullptr;
    QCheckBox *m_minimizeToTray = nullptr;
    QCheckBox *m_notifications = nullptr;

    // Juggler
    QComboBox *m_gracePeriod = nullptr;
    QSpinBox *m_stabilityChecks = nullptr;
    QComboBox *m_duplicateHandling = nullptr;
    QCheckBox *m_dryRun = nullptr;

    // Appearance
    QComboBox *m_theme = nullptr;

    // Logging
    QCheckBox *m_markdownLogging = nullptr;
    QComboBox *m_logRetention = nullptr;

    // Advanced
    QCheckBox *m_debugLogging = nullptr;
    QSpinBox *m_retryCount = nullptr;
    QSpinBox *m_workerCount = nullptr;
    QCheckBox *m_folderIcons = nullptr;

    void buildGeneralTab(QWidget *tab);
    void buildJugglerTab(QWidget *tab);
    void buildCleanerTab(QWidget *tab);
    void buildAppearanceTab(QWidget *tab);
    void buildLoggingTab(QWidget *tab);
    void buildAdvancedTab(QWidget *tab);

    void loadValues();
    void saveValues();
    void exportConfiguration();
    void importConfiguration();
    void resetConfiguration();
};

} // namespace bytes::ui
