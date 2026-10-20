#pragma once

#include "../database/Database.h"
#include "../core/FileOrganizer.h"
#include "../logging/ActivityLogger.h"

#include <QWidget>
#include <QTableWidget>
#include <QListWidget>

namespace bytes::ui {

/// Rules & Categories management page.
class RulesPage : public QWidget {
    Q_OBJECT
public:
    RulesPage(database::Database &db, core::FileOrganizer &organizer,
              logging::ActivityLogger &activityLogger, QWidget *parent = nullptr);
    void refresh();

private:
    database::Database &m_db;
    core::FileOrganizer &m_organizer;
    logging::ActivityLogger &m_activityLogger;

    QListWidget *m_categoryList = nullptr;
    QTableWidget *m_ruleTable = nullptr;

    void addRule();
    void editRule();
    void duplicateRule();
    void deleteRule();
    void toggleEnabled();
    void moveUp();
    void moveDown();
    void autoOptimize();
    void toggleCategoryEnabled(QListWidgetItem *item);

    int selectedRuleId() const;
    void refreshCategories();
    void refreshRules();
    QStringList monitoredFolderPaths() const;
};

} // namespace bytes::ui
