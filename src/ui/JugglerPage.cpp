#include "JugglerPage.h"
#include "../windows/ShellUtils.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QPushButton>
#include <QFileDialog>
#include <QMessageBox>
#include <QInputDialog>
#include <QDir>
#include <QLabel>
#include <QFont>

namespace bytes::ui {

JugglerPage::JugglerPage(database::Database &db, core::FileOrganizer &organizer, QWidget *parent)
    : QWidget(parent), m_db(db), m_organizer(organizer) {

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 14, 16, 14);

    auto *title = new QLabel("File Juggler", this);
    QFont f = title->font();
    f.setPointSize(14);
    f.setBold(true);
    title->setFont(f);
    layout->addWidget(title);

    auto *buttonRow = new QHBoxLayout();
    auto *addBtn = new QPushButton("Add Folder", this);
    auto *editBtn = new QPushButton("Edit", this);
    auto *removeBtn = new QPushButton("Remove", this);
    auto *pauseBtn = new QPushButton("Pause / Resume", this);
    buttonRow->addWidget(addBtn);
    buttonRow->addWidget(editBtn);
    buttonRow->addWidget(removeBtn);
    buttonRow->addWidget(pauseBtn);
    buttonRow->addStretch();
    layout->addLayout(buttonRow);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(5);
    m_table->setHorizontalHeaderLabels({"Folder", "Status", "Mode", "Grace Period", "Files Processed"});
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    layout->addWidget(m_table, 1);

    connect(addBtn, &QPushButton::clicked, this, &JugglerPage::addFolder);
    connect(editBtn, &QPushButton::clicked, this, &JugglerPage::editSelected);
    connect(removeBtn, &QPushButton::clicked, this, &JugglerPage::removeSelected);
    connect(pauseBtn, &QPushButton::clicked, this, &JugglerPage::togglePauseSelected);

    refresh();
}

int JugglerPage::selectedFolderId() const {
    const auto items = m_table->selectedItems();
    if (items.isEmpty())
        return -1;
    return m_table->item(items.first()->row(), 0)->data(Qt::UserRole).toInt();
}

void JugglerPage::addFolder() {
    const QString path = QFileDialog::getExistingDirectory(this, "Choose a folder for Bytes to monitor");
    if (path.isEmpty())
        return;

    database::MonitoredFolder folder;
    folder.path = QDir::toNativeSeparators(path);
    folder.enabled = true;
    folder.gracePeriodSeconds = m_db.getSettingInt("grace_period_seconds", 60);

    const int id = m_db.addMonitoredFolder(folder);
    if (id < 0) {
        QMessageBox::warning(this, "Bytes", "This folder is already monitored, or could not be added.");
        return;
    }

    m_organizer.reloadFolders();
    m_organizer.start(); // re-syncs watchers with the freshly reloaded folder list
    refresh();
}

void JugglerPage::editSelected() {
    const int id = selectedFolderId();
    if (id < 0)
        return;
    auto folder = m_db.monitoredFolder(id);
    if (!folder)
        return;

    bool ok = false;
    const int newGrace = QInputDialog::getInt(this, "Grace Period",
        "Seconds to wait before organizing new files:", folder->gracePeriodSeconds, 0, 3600, 5, &ok);
    if (!ok)
        return;

    folder->gracePeriodSeconds = newGrace;
    m_db.updateMonitoredFolder(*folder);
    m_organizer.reloadFolders();
    refresh();
}

void JugglerPage::removeSelected() {
    const int id = selectedFolderId();
    if (id < 0)
        return;
    if (QMessageBox::question(this, "Remove Folder",
            "Stop monitoring this folder? Files already organized will not be moved back.") != QMessageBox::Yes)
        return;

    m_db.removeMonitoredFolder(id);
    m_organizer.reloadFolders();
    m_organizer.start();
    refresh();
}

void JugglerPage::togglePauseSelected() {
    const int id = selectedFolderId();
    if (id < 0)
        return;
    auto folder = m_db.monitoredFolder(id);
    if (!folder)
        return;

    m_db.setMonitoredFolderEnabled(id, !folder->enabled);
    m_organizer.reloadFolders();
    m_organizer.start();
    refresh();
}

void JugglerPage::refresh() {
    const auto folders = m_db.monitoredFolders();
    m_table->setRowCount(folders.size());

    for (int row = 0; row < folders.size(); ++row) {
        const auto &folder = folders[row];

        auto *nameItem = new QTableWidgetItem(folder.path);
        nameItem->setData(Qt::UserRole, folder.id);
        m_table->setItem(row, 0, nameItem);

        m_table->setItem(row, 1, new QTableWidgetItem(folder.enabled ? "● Active" : "○ Paused"));
        m_table->setItem(row, 2, new QTableWidgetItem(folder.destinationMode == "custom" ? "Custom Destination" : "Subfolders"));
        m_table->setItem(row, 3, new QTableWidgetItem(QString("%1s").arg(folder.gracePeriodSeconds)));
        m_table->setItem(row, 4, new QTableWidgetItem(QString::number(folder.filesProcessed)));
    }
}

} // namespace bytes::ui
