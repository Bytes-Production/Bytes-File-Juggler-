#include "RuleEditDialog.h"

#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDialogButtonBox>
#include <QFileDialog>

namespace bytes::ui {

RuleEditDialog::RuleEditDialog(const QStringList &monitoredFolders, QWidget *parent) : QDialog(parent) {
    setWindowTitle("Rule");
    setMinimumWidth(420);

    auto *mainLayout = new QVBoxLayout(this);
    auto *form = new QFormLayout();

    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setPlaceholderText("e.g. Invoices");
    form->addRow("Name:", m_nameEdit);

    m_scopeFolder = new QComboBox(this);
    m_scopeFolder->addItem("All Monitored Folders", "");
    for (const QString &path : monitoredFolders)
        m_scopeFolder->addItem(path, path);
    form->addRow("Applies To:", m_scopeFolder);

    m_conditionType = new QComboBox(this);
    m_conditionType->addItem("File Extension", "extension");
    m_conditionType->addItem("File Type (category)", "filetype");
    m_conditionType->addItem("Filename Contains", "contains");
    m_conditionType->addItem("Filename Starts With", "startswith");
    m_conditionType->addItem("Filename Ends With", "endswith");
    m_conditionType->addItem("Filename Matches Wildcard", "wildcard");
    m_conditionType->addItem("File Size", "size");
    m_conditionType->addItem("Source Folder", "source_folder");
    m_conditionType->addItem("Date", "date");
    form->addRow("IF:", m_conditionType);

    m_conditionValue = new QLineEdit(this);
    form->addRow("IS:", m_conditionValue);

    m_hintLabel = new QLabel(this);
    m_hintLabel->setWordWrap(true);
    m_hintLabel->setStyleSheet("color: gray; font-size: 11px;");
    form->addRow("", m_hintLabel);

    m_actionType = new QComboBox(this);
    m_actionType->addItem("Move To", "move");
    m_actionType->addItem("Skip", "skip");
    m_actionType->addItem("Move To Custom Destination", "custom_destination");
    form->addRow("THEN:", m_actionType);

    auto *destRow = new QHBoxLayout();
    m_actionValue = new QLineEdit(this);
    m_actionValue->setPlaceholderText("e.g. Images  or  D:\\Organized\\Images");
    m_browseButton = new QPushButton("Browse…", this);
    destRow->addWidget(m_actionValue);
    destRow->addWidget(m_browseButton);
    form->addRow("Destination:", destRow);

    m_prioritySpin = new QSpinBox(this);
    m_prioritySpin->setRange(0, 10000);
    m_prioritySpin->setValue(100);
    form->addRow("Priority:", m_prioritySpin);

    m_enabledCheck = new QCheckBox("Enabled", this);
    m_enabledCheck->setChecked(true);
    form->addRow("", m_enabledCheck);

    mainLayout->addLayout(form);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttons);

    connect(m_conditionType, &QComboBox::currentIndexChanged, this, &RuleEditDialog::updateHint);
    connect(m_actionType, &QComboBox::currentIndexChanged, this, [this] {
        const bool needsDest = m_actionType->currentData().toString() != "skip";
        m_actionValue->setEnabled(needsDest);
        m_browseButton->setEnabled(needsDest);
    });
    connect(m_browseButton, &QPushButton::clicked, this, &RuleEditDialog::browseDestination);

    updateHint();
}

void RuleEditDialog::updateHint() {
    const QString type = m_conditionType->currentData().toString();
    QString hint;
    if (type == "extension") hint = "Comma-separated, without dots: pdf, docx, xlsx";
    else if (type == "filetype") hint = "A built-in category name: Images, Videos, Documents, Installers, ...";
    else if (type == "wildcard") hint = "e.g. invoice*.pdf — evaluated before generic *.pdf rules";
    else if (type == "size") hint = "e.g. >10MB, <=500KB, >1GB";
    else if (type == "source_folder") hint = "Absolute path of a monitored folder";
    else if (type == "date") hint = "older_than:30  or  newer_than:7  (days)";
    else hint = "Plain text to match against the file name";
    m_hintLabel->setText(hint);
}

void RuleEditDialog::browseDestination() {
    const QString dir = QFileDialog::getExistingDirectory(this, "Choose destination folder");
    if (!dir.isEmpty())
        m_actionValue->setText(dir);
}

void RuleEditDialog::setRule(const database::Rule &rule) {
    m_ruleId = rule.id;
    m_nameEdit->setText(rule.name);
    const int ci = m_conditionType->findData(rule.conditionType);
    if (ci >= 0) m_conditionType->setCurrentIndex(ci);
    m_conditionValue->setText(rule.conditionValue);
    const int ai = m_actionType->findData(rule.actionType);
    if (ai >= 0) m_actionType->setCurrentIndex(ai);
    m_actionValue->setText(rule.actionValue);
    m_prioritySpin->setValue(rule.priority);
    m_enabledCheck->setChecked(rule.enabled);
    const int si = m_scopeFolder->findData(rule.scopeFolder);
    m_scopeFolder->setCurrentIndex(si >= 0 ? si : 0);
    updateHint();
}

database::Rule RuleEditDialog::rule() const {
    database::Rule r;
    r.id = m_ruleId;
    r.name = m_nameEdit->text().trimmed();
    r.conditionType = m_conditionType->currentData().toString();
    r.conditionValue = m_conditionValue->text().trimmed();
    r.actionType = m_actionType->currentData().toString();
    r.actionValue = m_actionValue->text().trimmed();
    r.priority = m_prioritySpin->value();
    r.enabled = m_enabledCheck->isChecked();
    r.scopeFolder = m_scopeFolder->currentData().toString();
    return r;
}

} // namespace bytes::ui
