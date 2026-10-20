#pragma once

#include "../database/Database.h"
#include "../core/FileOrganizer.h"

#include <QWidget>
#include <QTableWidget>

namespace bytes::ui {

/// Detailed table of monitored folders (status/rules/destination/files
/// processed), with drag-and-drop folder addition.
class FoldersPage : public QWidget {
    Q_OBJECT
public:
    FoldersPage(database::Database &db, core::FileOrganizer &organizer, QWidget *parent = nullptr);
    void refresh();

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    database::Database &m_db;
    core::FileOrganizer &m_organizer;
    QTableWidget *m_table = nullptr;

    void addFolderPath(const QString &path);
};

} // namespace bytes::ui
