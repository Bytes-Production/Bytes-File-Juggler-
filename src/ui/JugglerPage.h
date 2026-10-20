#pragma once

#include "../database/Database.h"
#include "../core/FileOrganizer.h"

#include <QWidget>
#include <QTableWidget>

namespace bytes::ui {

/// "File Juggler" page: add/remove/pause/resume monitored folders.
class JugglerPage : public QWidget {
    Q_OBJECT
public:
    JugglerPage(database::Database &db, core::FileOrganizer &organizer, QWidget *parent = nullptr);
    void refresh();

private:
    database::Database &m_db;
    core::FileOrganizer &m_organizer;
    QTableWidget *m_table = nullptr;

    void addFolder();
    void editSelected();
    void removeSelected();
    void togglePauseSelected();
    int selectedFolderId() const;
};

} // namespace bytes::ui
