#pragma once

#include "../database/Database.h"
#include "../logging/ActivityLogger.h"

#include <QWidget>
#include <QLabel>
#include <QCheckBox>
#include <QComboBox>
#include <QTimeEdit>
#include <QSpinBox>
#include <QThreadPool>

namespace bytes::ui {

/// Cleaner page: User Temp / Windows Temp / Recycle Bin sizing and cleanup,
/// plus the automatic cleanup schedule.
class CleanerPage : public QWidget {
    Q_OBJECT
public:
    CleanerPage(database::Database &db, logging::ActivityLogger &activityLogger, QWidget *parent = nullptr);

    void refreshSizes();

public slots:
    void runCleanupNow(); // used by the Dashboard's "Run Cleanup" quick action

private:
    database::Database &m_db;
    logging::ActivityLogger &m_activityLogger;
    QThreadPool m_pool;

    QCheckBox *m_userTempCheck = nullptr;
    QCheckBox *m_windowsTempCheck = nullptr;
    QCheckBox *m_recycleBinCheck = nullptr;
    QLabel *m_userTempSizeLabel = nullptr;
    QLabel *m_windowsTempSizeLabel = nullptr;
    QLabel *m_recycleBinSizeLabel = nullptr;
    QLabel *m_potentialLabel = nullptr;

    QComboBox *m_scheduleCombo = nullptr;
    QTimeEdit *m_scheduleTime = nullptr;
    QSpinBox *m_scheduleHours = nullptr;
    QLabel *m_scheduleTimeLabel = nullptr;
    QLabel *m_scheduleHoursLabel = nullptr;

    qint64 m_userTempBytes = 0, m_windowsTempBytes = 0, m_recycleBinBytes = 0;

    void cleanSelected();
    void cleanEverything();
    void performCleanup(bool userTemp, bool windowsTemp, bool recycleBin);
    void updatePotentialLabel();
    void onScheduleChanged();
};

} // namespace bytes::ui
