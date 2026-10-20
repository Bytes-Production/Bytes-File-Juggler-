#include "RulesPage.h"
#include "RuleEditDialog.h"
#include "RuleTesterDialog.h"
#include <QFont>
#include <utility>

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QGroupBox>
#include <QHeaderView>
#include <QPushButton>
#include <QLabel>
#include <QMessageBox>

namespace bytes::ui {

RulesPage::RulesPage(database::Database &db, core::FileOrganizer &organizer,
                      logging::ActivityLogger &activityLogger, QWidget *parent)
    : QWidget(parent), m_db(db), m_organizer(organizer), m_activityLogger(activityLogger) {

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 14, 16, 14);

    auto *title = new QLabel("Rules", this);
    QFont f = title->font();
    f.setPointSize(14);
    f.setBold(true);
    title->setFont(f);
    layout->addWidget(title);

    auto *splitter = new QSplitter(Qt::Horizontal, this);

    auto *categoryBox = new QGroupBox("Categories", splitter);
    auto *categoryLayout = new QVBoxLayout(categoryBox);
    m_categoryList = new QListWidget(categoryBox);
    categoryLayout->addWidget(m_categoryList);
    connect(m_categoryList, &QListWidget::itemChanged, this, &RulesPage::toggleCategoryEnabled);

    auto *ruleBox = new QGroupBox("Custom Rules (evaluated before categories, most specific first)", splitter);
    auto *ruleLayout = new QVBoxLayout(ruleBox);

    // Two rows so labels never truncate/collapse, even at Bytes's compact
    // default window width.
    auto *buttonRow1 = new QHBoxLayout();
    auto *addBtn = new QPushButton("+ Add Rule", this);
    auto *editBtn = new QPushButton("Edit", this);
    auto *dupBtn = new QPushButton("Duplicate", this);
    auto *delBtn = new QPushButton("Delete", this);
    auto *toggleBtn = new QPushButton("Enable/Disable", this);
    for (auto *b : {addBtn, editBtn, dupBtn, delBtn, toggleBtn})
        buttonRow1->addWidget(b);
    buttonRow1->addStretch();
    ruleLayout->addLayout(buttonRow1);

    auto *buttonRow2 = new QHBoxLayout();
    auto *upBtn = new QPushButton("Move Up", this);
    auto *downBtn = new QPushButton("Move Down", this);
    auto *optimizeBtn = new QPushButton("Auto Optimize Priority", this);
    auto *testerBtn = new QPushButton("Rule Tester", this);
    for (auto *b : {upBtn, downBtn, optimizeBtn, testerBtn})
        buttonRow2->addWidget(b);
    buttonRow2->addStretch();
    ruleLayout->addLayout(buttonRow2);

    m_ruleTable = new QTableWidget(ruleBox);
    m_ruleTable->setColumnCount(7);
    m_ruleTable->setHorizontalHeaderLabels({"Name", "If", "Is", "Then", "Folder", "Priority", "Enabled"});
    m_ruleTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_ruleTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_ruleTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_ruleTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ruleLayout->addWidget(m_ruleTable);

    splitter->addWidget(categoryBox);
    splitter->addWidget(ruleBox);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({220, 480});
    layout->addWidget(splitter, 1);

    connect(addBtn, &QPushButton::clicked, this, &RulesPage::addRule);
    connect(editBtn, &QPushButton::clicked, this, &RulesPage::editRule);
    connect(dupBtn, &QPushButton::clicked, this, &RulesPage::duplicateRule);
    connect(delBtn, &QPushButton::clicked, this, &RulesPage::deleteRule);
    connect(toggleBtn, &QPushButton::clicked, this, &RulesPage::toggleEnabled);
    connect(upBtn, &QPushButton::clicked, this, &RulesPage::moveUp);
    connect(downBtn, &QPushButton::clicked, this, &RulesPage::moveDown);
    connect(optimizeBtn, &QPushButton::clicked, this, &RulesPage::autoOptimize);
    connect(testerBtn, &QPushButton::clicked, this, [this] {
        RuleTesterDialog dialog(m_organizer.ruleEngine(), this);
        dialog.exec();
    });

    refresh();
}

void RulesPage::refresh() {
    refreshCategories();
    refreshRules();
}

void RulesPage::refreshCategories() {
    m_categoryList->blockSignals(true);
    m_categoryList->clear();
    for (const auto &cat : m_db.categories()) {
        const QString text = QString("%1  (%2)  →  %3").arg(cat.name, cat.extensions.join(' '), cat.destination);
        auto *item = new QListWidgetItem(text, m_categoryList);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(cat.enabled ? Qt::Checked : Qt::Unchecked);
        item->setData(Qt::UserRole, cat.id);
    }
    m_categoryList->blockSignals(false);
}

void RulesPage::refreshRules() {
    const auto rules = m_db.rules();
    m_ruleTable->setRowCount(rules.size());
    for (int row = 0; row < rules.size(); ++row) {
        const auto &r = rules[row];
        auto *nameItem = new QTableWidgetItem(r.name);
        nameItem->setData(Qt::UserRole, r.id);
        m_ruleTable->setItem(row, 0, nameItem);
        m_ruleTable->setItem(row, 1, new QTableWidgetItem(r.conditionType));
        m_ruleTable->setItem(row, 2, new QTableWidgetItem(r.conditionValue));
        m_ruleTable->setItem(row, 3, new QTableWidgetItem(r.actionType == "skip" ? "Skip" : r.actionValue));
        m_ruleTable->setItem(row, 4, new QTableWidgetItem(r.scopeFolder.isEmpty() ? "All Folders" : r.scopeFolder));
        m_ruleTable->setItem(row, 5, new QTableWidgetItem(QString::number(r.priority)));
        m_ruleTable->setItem(row, 6, new QTableWidgetItem(r.enabled ? "●" : "○"));
    }
}

int RulesPage::selectedRuleId() const {
    const auto items = m_ruleTable->selectedItems();
    if (items.isEmpty())
        return -1;
    return m_ruleTable->item(items.first()->row(), 0)->data(Qt::UserRole).toInt();
}

QStringList RulesPage::monitoredFolderPaths() const {
    QStringList paths;
    for (const auto &f : m_db.monitoredFolders())
        paths << f.path;
    return paths;
}

void RulesPage::addRule() {
    RuleEditDialog dialog(monitoredFolderPaths(), this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    database::Rule rule = dialog.rule();
    if (rule.name.isEmpty()) {
        QMessageBox::warning(this, "Bytes", "Please give the rule a name.");
        return;
    }
    m_db.addRule(rule);
    m_organizer.reloadRules();
    m_activityLogger.logRuleChange(QString("Added rule \"%1\"").arg(rule.name));
    refresh();
}

void RulesPage::editRule() {
    const int id = selectedRuleId();
    if (id < 0)
        return;
    for (const auto &r : m_db.rules()) {
        if (r.id != id)
            continue;
        RuleEditDialog dialog(monitoredFolderPaths(), this);
        dialog.setRule(r);
        if (dialog.exec() != QDialog::Accepted)
            return;
        auto updated = dialog.rule();
        updated.id = id;
        m_db.updateRule(updated);
        m_organizer.reloadRules();
        m_activityLogger.logRuleChange(QString("Edited rule \"%1\"").arg(updated.name));
        refresh();
        return;
    }
}

void RulesPage::duplicateRule() {
    const int id = selectedRuleId();
    if (id < 0)
        return;
    for (const auto &r : m_db.rules()) {
        if (r.id != id)
            continue;
        database::Rule copy = r;
        copy.id = -1;
        copy.name += " (copy)";
        m_db.addRule(copy);
        m_organizer.reloadRules();
        refresh();
        return;
    }
}

void RulesPage::deleteRule() {
    const int id = selectedRuleId();
    if (id < 0)
        return;
    if (QMessageBox::question(this, "Delete Rule", "Delete this rule?") != QMessageBox::Yes)
        return;
    m_db.removeRule(id);
    m_organizer.reloadRules();
    refresh();
}

void RulesPage::toggleEnabled() {
    const int id = selectedRuleId();
    if (id < 0)
        return;
    for (const auto &r : m_db.rules()) {
        if (r.id == id) {
            m_db.setRuleEnabled(id, !r.enabled);
            break;
        }
    }
    m_organizer.reloadRules();
    refresh();
}

void RulesPage::moveUp() {
    const int id = selectedRuleId();
    if (id < 0)
        return;
    auto rules = m_db.rules(); // priority desc
    for (int i = 0; i < rules.size(); ++i) {
        if (rules[i].id == id && i > 0) {
            std::swap(rules[i].priority, rules[i - 1].priority);
            m_db.setRulePriority(rules[i].id, rules[i].priority);
            m_db.setRulePriority(rules[i - 1].id, rules[i - 1].priority);
            break;
        }
    }
    m_organizer.reloadRules();
    refresh();
}

void RulesPage::moveDown() {
    const int id = selectedRuleId();
    if (id < 0)
        return;
    auto rules = m_db.rules();
    for (int i = 0; i < rules.size(); ++i) {
        if (rules[i].id == id && i < rules.size() - 1) {
            std::swap(rules[i].priority, rules[i + 1].priority);
            m_db.setRulePriority(rules[i].id, rules[i].priority);
            m_db.setRulePriority(rules[i + 1].id, rules[i + 1].priority);
            break;
        }
    }
    m_organizer.reloadRules();
    refresh();
}

void RulesPage::autoOptimize() {
    const auto optimized = core::RuleEngine::autoOptimized(m_db.rules());
    for (const auto &r : optimized)
        m_db.setRulePriority(r.id, r.priority);
    m_organizer.reloadRules();
    m_activityLogger.logRuleChange("Auto-optimized rule priority order");
    refresh();
}

void RulesPage::toggleCategoryEnabled(QListWidgetItem *item) {
    const int id = item->data(Qt::UserRole).toInt();
    for (auto cat : m_db.categories()) {
        if (cat.id == id) {
            cat.enabled = item->checkState() == Qt::Checked;
            m_db.updateCategory(cat);
            break;
        }
    }
    m_organizer.reloadRules();
}

} // namespace bytes::ui
