#include "ActivityPage.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QPushButton>
#include <QLabel>
#include <QMessageBox>
#include <QFileInfo>
#include <QFont>
#include <QDate>
#include <QTime>
#include <QDateTime>

namespace bytes::ui {

ActivityPage::ActivityPage(database::Database &db, logging::ActivityLogger &activityLogger,
                            core::UndoManager &undoManager, QWidget *parent)
    : QWidget(parent), m_db(db), m_activityLogger(activityLogger), m_undoManager(undoManager) {

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 14, 16, 14);

    auto *title = new QLabel("Activity", this);
    QFont f = title->font();
    f.setPointSize(14);
    f.setBold(true);
    title->setFont(f);
    layout->addWidget(title);

    auto *filterRow = new QHBoxLayout();
    m_filterCombo = new QComboBox(this);
    m_filterCombo->addItems({"All", "moved", "skipped", "error", "would_move"});
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText("Search by filename…");
    m_fromDate = new QDateEdit(QDate::currentDate().addMonths(-1), this);
    m_fromDate->setCalendarPopup(true);
    m_toDate = new QDateEdit(QDate::currentDate(), this);
    m_toDate->setCalendarPopup(true);

    filterRow->addWidget(new QLabel("Status:", this));
    filterRow->addWidget(m_filterCombo);
    filterRow->addWidget(m_searchEdit, 1);
    filterRow->addWidget(new QLabel("From:", this));
    filterRow->addWidget(m_fromDate);
    filterRow->addWidget(new QLabel("To:", this));
    filterRow->addWidget(m_toDate);
    layout->addLayout(filterRow);

    auto *undoRow = new QHBoxLayout();
    auto *undoLastBtn = new QPushButton("Undo Last", this);
    auto *undoSelectedBtn = new QPushButton("Undo Selected", this);
    undoRow->addWidget(undoLastBtn);
    undoRow->addWidget(undoSelectedBtn);
    undoRow->addStretch();
    layout->addLayout(undoRow);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(7);
    m_table->setHorizontalHeaderLabels({"Time", "File", "Type", "Source", "Destination", "Rule", "Status"});
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    layout->addWidget(m_table, 1);

    connect(m_filterCombo, &QComboBox::currentTextChanged, this, &ActivityPage::applyFilters);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &ActivityPage::applyFilters);
    connect(m_fromDate, &QDateEdit::dateChanged, this, &ActivityPage::applyFilters);
    connect(m_toDate, &QDateEdit::dateChanged, this, &ActivityPage::applyFilters);
    connect(m_table, &QTableWidget::cellDoubleClicked, this, &ActivityPage::showDetails);
    connect(undoLastBtn, &QPushButton::clicked, this, &ActivityPage::undoLast);
    connect(undoSelectedBtn, &QPushButton::clicked, this, &ActivityPage::undoSelected);

    m_refreshCoalesceTimer.setSingleShot(true);
    m_refreshCoalesceTimer.setInterval(500);
    connect(&m_refreshCoalesceTimer, &QTimer::timeout, this, &ActivityPage::applyFilters);

    connect(&m_activityLogger, &logging::ActivityLogger::operationLogged, this, [this](const database::FileOperationRecord &) {
        // Rebuilding the whole table per-file made a big sort feel sluggish;
        // coalesce bursts into at most one refresh every 500ms.
        if (!m_refreshCoalesceTimer.isActive())
            m_refreshCoalesceTimer.start();
    });

    refresh();
}

void ActivityPage::refresh() { applyFilters(); }

void ActivityPage::applyFilters() {
    const auto records = m_db.fileOperationsFiltered(
        m_filterCombo->currentText(), m_searchEdit->text(),
        QDateTime(m_fromDate->date(), QTime(0, 0)), QDateTime(m_toDate->date(), QTime(23, 59, 59)), 1000);

    m_table->setRowCount(records.size());
    for (int row = 0; row < records.size(); ++row) {
        const auto &r = records[row];
        m_table->setItem(row, 0, new QTableWidgetItem(r.timestamp.toString("yyyy-MM-dd HH:mm:ss")));
        auto *fileItem = new QTableWidgetItem(QFileInfo(r.sourcePath).fileName());
        fileItem->setData(Qt::UserRole, r.id);
        m_table->setItem(row, 1, fileItem);
        m_table->setItem(row, 2, new QTableWidgetItem(r.fileType));
        m_table->setItem(row, 3, new QTableWidgetItem(r.sourcePath));
        m_table->setItem(row, 4, new QTableWidgetItem(r.destPath));
        m_table->setItem(row, 5, new QTableWidgetItem(r.ruleName));
        QString status = r.status;
        if (r.undone) status += " (undone)";
        m_table->setItem(row, 6, new QTableWidgetItem(status));
    }
}

void ActivityPage::showDetails(int row, int column) {
    Q_UNUSED(column);
    const int id = m_table->item(row, 1)->data(Qt::UserRole).toInt();
    const auto record = m_db.fileOperation(id);
    if (!record)
        return;

    const QString details = QString(
        "File: %1\n\nSource: %2\n\nDestination: %3\n\nType: %4\n\nRule: %5\n\nStatus: %6\n\nError: %7\n\nTime: %8")
        .arg(QFileInfo(record->sourcePath).fileName(), record->sourcePath, record->destPath, record->fileType,
             record->ruleName.isEmpty() ? "Built-in classifier" : record->ruleName, record->status,
             record->errorMessage.isEmpty() ? "—" : record->errorMessage,
             record->timestamp.toString("yyyy-MM-dd HH:mm:ss"));
    QMessageBox::information(this, "Operation Details", details);
}

void ActivityPage::undoLast() {
    const auto outcome = m_undoManager.undoLast();
    m_activityLogger.logUndo(outcome.operationId, "", outcome.restoredPath, outcome.success, outcome.message);
    QMessageBox::information(this, "Undo", outcome.message);
    refresh();
}

void ActivityPage::undoSelected() {
    const auto items = m_table->selectionModel()->selectedRows();
    if (items.isEmpty()) {
        QMessageBox::information(this, "Undo", "Select one or more rows first.");
        return;
    }

    QVector<int> ids;
    for (const auto &index : items)
        ids << m_table->item(index.row(), 1)->data(Qt::UserRole).toInt();

    const auto outcomes = m_undoManager.undoMultiple(ids);
    int succeeded = 0;
    for (const auto &o : outcomes) {
        m_activityLogger.logUndo(o.operationId, "", o.restoredPath, o.success, o.message);
        if (o.success)
            succeeded++;
    }
    QMessageBox::information(this, "Undo", QString("Restored %1 of %2 file(s).").arg(succeeded).arg(outcomes.size()));
    refresh();
}

} // namespace bytes::ui
