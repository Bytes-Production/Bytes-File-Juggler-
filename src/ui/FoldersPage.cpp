#include "FoldersPage.h"

#include <QVBoxLayout>
#include <QLabel>
#include <QHeaderView>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QDir>
#include <QFileInfo>
#include <QFont>
#include <QUrl>

namespace bytes::ui {

FoldersPage::FoldersPage(database::Database &db, core::FileOrganizer &organizer, QWidget *parent)
    : QWidget(parent), m_db(db), m_organizer(organizer) {

    setAcceptDrops(true);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 14, 16, 14);

    auto *title = new QLabel("Folders", this);
    QFont f = title->font();
    f.setPointSize(14);
    f.setBold(true);
    title->setFont(f);
    layout->addWidget(title);

    auto *hint = new QLabel("Drag and drop a folder here to start monitoring it.", this);
    hint->setStyleSheet("color: gray;");
    layout->addWidget(hint);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(5);
    m_table->setHorizontalHeaderLabels({"Monitored Folder", "Status", "Rules", "Destination", "Files Processed"});
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    layout->addWidget(m_table, 1);

    refresh();
}

void FoldersPage::dragEnterEvent(QDragEnterEvent *event) {
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void FoldersPage::dropEvent(QDropEvent *event) {
    for (const QUrl &url : event->mimeData()->urls()) {
        const QString path = url.toLocalFile();
        if (QFileInfo(path).isDir())
            addFolderPath(path);
    }
    event->acceptProposedAction();
}

void FoldersPage::addFolderPath(const QString &path) {
    database::MonitoredFolder folder;
    folder.path = QDir::toNativeSeparators(path);
    folder.enabled = true;
    folder.gracePeriodSeconds = m_db.getSettingInt("grace_period_seconds", 60);
    if (m_db.addMonitoredFolder(folder) >= 0) {
        m_organizer.reloadFolders();
        m_organizer.start();
        refresh();
    }
}

void FoldersPage::refresh() {
    const auto folders = m_db.monitoredFolders();
    const auto ruleCount = m_db.rules().size();

    m_table->setRowCount(folders.size());
    for (int row = 0; row < folders.size(); ++row) {
        const auto &folder = folders[row];
        m_table->setItem(row, 0, new QTableWidgetItem(folder.path));
        m_table->setItem(row, 1, new QTableWidgetItem(folder.enabled ? "● Active" : "○ Paused"));
        m_table->setItem(row, 2, new QTableWidgetItem(QString("%1 custom rule(s)").arg(ruleCount)));
        m_table->setItem(row, 3, new QTableWidgetItem(folder.destinationMode == "custom"
                                                        ? folder.customDestination : "Subfolders (auto)"));
        m_table->setItem(row, 4, new QTableWidgetItem(QString::number(folder.filesProcessed)));
    }
}

} // namespace bytes::ui
