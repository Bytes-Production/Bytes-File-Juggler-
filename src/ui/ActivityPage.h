#pragma once

#include "../database/Database.h"
#include "../logging/ActivityLogger.h"
#include "../core/UndoManager.h"

#include <QWidget>
#include <QTableWidget>
#include <QComboBox>
#include <QLineEdit>
#include <QDateEdit>
#include <QTimer>

namespace bytes::ui {

/// Full activity/history browser with filtering, search, and undo actions.
class ActivityPage : public QWidget {
    Q_OBJECT
public:
    ActivityPage(database::Database &db, logging::ActivityLogger &activityLogger,
                 core::UndoManager &undoManager, QWidget *parent = nullptr);
    void refresh();

private:
    database::Database &m_db;
    logging::ActivityLogger &m_activityLogger;
    core::UndoManager &m_undoManager;

    QComboBox *m_filterCombo = nullptr;
    QLineEdit *m_searchEdit = nullptr;
    QDateEdit *m_fromDate = nullptr;
    QDateEdit *m_toDate = nullptr;
    QTableWidget *m_table = nullptr;
    QTimer m_refreshCoalesceTimer;

    void applyFilters();
    void showDetails(int row, int column);
    void undoLast();
    void undoSelected();
};

} // namespace bytes::ui
