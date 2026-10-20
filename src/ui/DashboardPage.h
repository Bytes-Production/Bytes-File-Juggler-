#pragma once

#include "../database/Database.h"
#include "../logging/ActivityLogger.h"
#include "../core/FileOrganizer.h"

#include <QWidget>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QTimer>

namespace bytes::ui {

/// The landing page: quick stat cards, recent activity, monitoring status,
/// and one-click quick actions.
class DashboardPage : public QWidget {
    Q_OBJECT
public:
    DashboardPage(database::Database &db, logging::ActivityLogger &activityLogger,
                  core::FileOrganizer &organizer, QWidget *parent = nullptr);

    void refresh();

signals:
    void runCleanupRequested();
    void navigateToActivityRequested();

private:
    database::Database &m_db;
    logging::ActivityLogger &m_activityLogger;
    core::FileOrganizer &m_organizer;

    QLabel *m_greetingLabel = nullptr;
    QLabel *m_todayCard = nullptr;
    QLabel *m_weekCard = nullptr;
    QLabel *m_totalCard = nullptr;
    QLabel *m_foldersCard = nullptr;
    QLabel *m_skippedCard = nullptr;
    QLabel *m_errorsCard = nullptr;
    QLabel *m_cleanedCard = nullptr;
    QLabel *m_lastActivityCard = nullptr;
    QLabel *m_statusLabel = nullptr;
    QListWidget *m_recentList = nullptr;
    QPushButton *m_pauseResumeButton = nullptr;
    QTimer m_refreshCoalesceTimer; // avoids rebuilding the whole page per-file during a big sort

    QWidget *buildStatCard(const QString &title, QLabel *&valueLabelOut);
    void updatePauseButton();
};

} // namespace bytes::ui
