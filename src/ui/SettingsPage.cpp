#include "SettingsPage.h"
#include "../windows/StartupManager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QTabWidget>
#include <QPushButton>
#include <QLabel>
#include <QMessageBox>
#include <QFileDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QUrl>
#include <QFont>

namespace bytes::ui {

SettingsPage::SettingsPage(database::Database &db, core::FileOrganizer &organizer,
                            logging::MarkdownLogger &markdownLogger, QWidget *parent)
    : QWidget(parent), m_db(db), m_organizer(organizer), m_markdownLogger(markdownLogger) {

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 14, 16, 14);

    auto *title = new QLabel("Settings", this);
    QFont f = title->font();
    f.setPointSize(14);
    f.setBold(true);
    title->setFont(f);
    layout->addWidget(title);

    auto *tabs = new QTabWidget(this);
    auto *general = new QWidget(); buildGeneralTab(general); tabs->addTab(general, "General");
    auto *juggler = new QWidget(); buildJugglerTab(juggler); tabs->addTab(juggler, "Juggler");
    auto *cleanerTab = new QWidget(); buildCleanerTab(cleanerTab); tabs->addTab(cleanerTab, "Cleaner");
    auto *appearance = new QWidget(); buildAppearanceTab(appearance); tabs->addTab(appearance, "Appearance");
    auto *loggingTab = new QWidget(); buildLoggingTab(loggingTab); tabs->addTab(loggingTab, "Logging");
    auto *advanced = new QWidget(); buildAdvancedTab(advanced); tabs->addTab(advanced, "Advanced");
    layout->addWidget(tabs, 1);

    auto *buttonRow = new QHBoxLayout();
    auto *saveBtn = new QPushButton("Save Settings", this);
    auto *exportBtn = new QPushButton("Export Configuration", this);
    auto *importBtn = new QPushButton("Import Configuration", this);
    auto *resetBtn = new QPushButton("Reset Configuration", this);
    buttonRow->addWidget(saveBtn);
    buttonRow->addStretch();
    buttonRow->addWidget(exportBtn);
    buttonRow->addWidget(importBtn);
    buttonRow->addWidget(resetBtn);
    layout->addLayout(buttonRow);

    connect(saveBtn, &QPushButton::clicked, this, [this] { saveValues(); QMessageBox::information(this, "Bytes", "Settings saved."); });
    connect(exportBtn, &QPushButton::clicked, this, &SettingsPage::exportConfiguration);
    connect(importBtn, &QPushButton::clicked, this, &SettingsPage::importConfiguration);
    connect(resetBtn, &QPushButton::clicked, this, &SettingsPage::resetConfiguration);

    loadValues();
}

void SettingsPage::buildGeneralTab(QWidget *tab) {
    auto *form = new QFormLayout(tab);
    m_startWithWindows = new QCheckBox("Start Bytes with Windows", tab);
    m_minimizeToTray = new QCheckBox("Close window to tray", tab);
    m_notifications = new QCheckBox("Show notifications", tab);
    form->addRow(m_startWithWindows);
    form->addRow(m_minimizeToTray);
    form->addRow(m_notifications);
}

void SettingsPage::buildJugglerTab(QWidget *tab) {
    auto *form = new QFormLayout(tab);
    m_gracePeriod = new QComboBox(tab);
    m_gracePeriod->addItem("Immediately", 0);
    m_gracePeriod->addItem("10 seconds", 10);
    m_gracePeriod->addItem("30 seconds", 30);
    m_gracePeriod->addItem("1 minute", 60);
    m_gracePeriod->addItem("2 minutes", 120);
    m_gracePeriod->addItem("5 minutes", 300);
    m_gracePeriod->addItem("10 minutes", 600);
    form->addRow("Grace Period:", m_gracePeriod);

    m_stabilityChecks = new QSpinBox(tab);
    m_stabilityChecks->setRange(1, 10);
    form->addRow("Stability checks before moving:", m_stabilityChecks);

    m_duplicateHandling = new QComboBox(tab);
    m_duplicateHandling->addItem("Rename automatically", "rename");
    m_duplicateHandling->addItem("Skip", "skip");
    m_duplicateHandling->addItem("Ask", "ask");
    m_duplicateHandling->addItem("Replace", "replace");
    form->addRow("Duplicate Handling:", m_duplicateHandling);

    m_dryRun = new QCheckBox("Preview Mode (Dry Run) — never actually move files", tab);
    form->addRow(m_dryRun);
}

void SettingsPage::buildCleanerTab(QWidget *tab) {
    auto *form = new QFormLayout(tab);
    auto *note = new QLabel("Configure schedule and per-target defaults on the Cleaner page.", tab);
    note->setWordWrap(true);
    form->addRow(note);
}

void SettingsPage::buildAppearanceTab(QWidget *tab) {
    auto *form = new QFormLayout(tab);
    m_theme = new QComboBox(tab);
    m_theme->addItems({"Light", "Dark", "System"});
    form->addRow("Theme:", m_theme);
    connect(m_theme, &QComboBox::currentTextChanged, this, [this](const QString &t) {
        emit appearanceChanged(t.toLower());
    });
}

void SettingsPage::buildLoggingTab(QWidget *tab) {
    auto *form = new QFormLayout(tab);
    m_markdownLogging = new QCheckBox("Enable Markdown activity logs", tab);
    form->addRow(m_markdownLogging);

    m_logRetention = new QComboBox(tab);
    m_logRetention->addItem("30 days", 30);
    m_logRetention->addItem("90 days", 90);
    m_logRetention->addItem("180 days", 180);
    m_logRetention->addItem("1 year", 365);
    m_logRetention->addItem("Forever", 0);
    form->addRow("Log Retention:", m_logRetention);

    auto *openLogsBtn = new QPushButton("Open Logs Folder", tab);
    connect(openLogsBtn, &QPushButton::clicked, this, [this] {
        QDesktopServices::openUrl(QUrl::fromLocalFile(m_markdownLogger.logDirectory()));
    });
    form->addRow(openLogsBtn);
}

void SettingsPage::buildAdvancedTab(QWidget *tab) {
    auto *form = new QFormLayout(tab);
    m_debugLogging = new QCheckBox("Debug logging", tab);
    form->addRow(m_debugLogging);

    m_retryCount = new QSpinBox(tab);
    m_retryCount->setRange(1, 20);
    form->addRow("Retry count for locked files:", m_retryCount);

    m_workerCount = new QSpinBox(tab);
    m_workerCount->setRange(1, 16);
    form->addRow("Worker threads:", m_workerCount);

    m_folderIcons = new QCheckBox("Use Bytes folder icons for created folders", tab);
    form->addRow(m_folderIcons);

    auto *dbPathLabel = new QLabel(m_db.databaseFilePath(), tab);
    dbPathLabel->setWordWrap(true);
    form->addRow("Database location:", dbPathLabel);
}

void SettingsPage::loadValues() {
    m_startWithWindows->setChecked(m_db.getSettingBool("start_with_windows", false));
    m_minimizeToTray->setChecked(m_db.getSettingBool("close_window_to_tray", true));
    m_notifications->setChecked(m_db.getSettingBool("notifications_enabled", true));

    const int grace = m_db.getSettingInt("grace_period_seconds", 60);
    int idx = m_gracePeriod->findData(grace);
    m_gracePeriod->setCurrentIndex(idx >= 0 ? idx : m_gracePeriod->findData(60));

    m_stabilityChecks->setValue(m_db.getSettingInt("stability_check_count", 3));
    m_duplicateHandling->setCurrentIndex(qMax(0, m_duplicateHandling->findData(m_db.getSetting("duplicate_handling", "rename"))));
    m_dryRun->setChecked(m_db.getSettingBool("dry_run_enabled", false));

    const QString theme = m_db.getSetting("appearance_theme", "system");
    m_theme->setCurrentText(theme.left(1).toUpper() + theme.mid(1));

    m_markdownLogging->setChecked(m_db.getSettingBool("markdown_logging_enabled", true));
    const int retention = m_db.getSettingInt("log_retention_days", 180);
    int ri = m_logRetention->findData(retention);
    m_logRetention->setCurrentIndex(ri >= 0 ? ri : 2);

    m_debugLogging->setChecked(m_db.getSettingBool("debug_logging_enabled", false));
    m_retryCount->setValue(m_db.getSettingInt("retry_count", 10));
    m_workerCount->setValue(m_db.getSettingInt("worker_count", 2));
    m_folderIcons->setChecked(m_db.getSettingBool("use_bytes_folder_icons", true));
}

void SettingsPage::saveValues() {
    m_db.setSetting("start_with_windows", m_startWithWindows->isChecked() ? "1" : "0");
    m_db.setSetting("close_window_to_tray", m_minimizeToTray->isChecked() ? "1" : "0");
    m_db.setSetting("notifications_enabled", m_notifications->isChecked() ? "1" : "0");
    bytes::windows::StartupManager::setEnabled(m_startWithWindows->isChecked(), QCoreApplication::applicationFilePath());

    m_db.setSetting("grace_period_seconds", QString::number(m_gracePeriod->currentData().toInt()));
    m_db.setSetting("stability_check_count", QString::number(m_stabilityChecks->value()));
    m_db.setSetting("duplicate_handling", m_duplicateHandling->currentData().toString());
    m_db.setSetting("dry_run_enabled", m_dryRun->isChecked() ? "1" : "0");
    m_organizer.setPreviewMode(m_dryRun->isChecked());

    m_db.setSetting("appearance_theme", m_theme->currentText().toLower());

    m_db.setSetting("markdown_logging_enabled", m_markdownLogging->isChecked() ? "1" : "0");
    m_db.setSetting("log_retention_days", QString::number(m_logRetention->currentData().toInt()));

    m_db.setSetting("debug_logging_enabled", m_debugLogging->isChecked() ? "1" : "0");
    m_db.setSetting("retry_count", QString::number(m_retryCount->value()));
    m_db.setSetting("worker_count", QString::number(m_workerCount->value()));
    m_db.setSetting("use_bytes_folder_icons", m_folderIcons->isChecked() ? "1" : "0");
}

void SettingsPage::exportConfiguration() {
    const QString path = QFileDialog::getSaveFileName(this, "Export Configuration", "Bytes_Backup.json", "JSON (*.json)");
    if (path.isEmpty())
        return;

    QJsonObject root;
    QJsonArray rulesArray;
    for (const auto &r : m_db.rules()) {
        QJsonObject o;
        o["name"] = r.name;
        o["conditionType"] = r.conditionType;
        o["conditionValue"] = r.conditionValue;
        o["actionType"] = r.actionType;
        o["actionValue"] = r.actionValue;
        o["priority"] = r.priority;
        o["enabled"] = r.enabled;
        rulesArray.append(o);
    }
    root["rules"] = rulesArray;

    QJsonArray categoriesArray;
    for (const auto &c : m_db.categories()) {
        QJsonObject o;
        o["name"] = c.name;
        o["extensions"] = c.extensions.join(',');
        o["destination"] = c.destination;
        o["enabled"] = c.enabled;
        o["priority"] = c.priority;
        categoriesArray.append(o);
    }
    root["categories"] = categoriesArray;

    QFile file(path);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
        QMessageBox::information(this, "Bytes", "Configuration exported.");
    }
}

void SettingsPage::importConfiguration() {
    const QString path = QFileDialog::getOpenFileName(this, "Import Configuration", QString(), "JSON (*.json)");
    if (path.isEmpty())
        return;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, "Bytes", "Could not open file.");
        return;
    }
    const auto doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) {
        QMessageBox::warning(this, "Bytes", "Invalid configuration file.");
        return;
    }

    const auto root = doc.object();
    for (const auto &value : root["rules"].toArray()) {
        const auto o = value.toObject();
        database::Rule r;
        r.name = o["name"].toString();
        r.conditionType = o["conditionType"].toString();
        r.conditionValue = o["conditionValue"].toString();
        r.actionType = o["actionType"].toString();
        r.actionValue = o["actionValue"].toString();
        r.priority = o["priority"].toInt(100);
        r.enabled = o["enabled"].toBool(true);
        m_db.addRule(r);
    }

    m_organizer.reloadRules();
    QMessageBox::information(this, "Bytes", "Configuration imported.");
}

void SettingsPage::resetConfiguration() {
    if (QMessageBox::question(this, "Reset Configuration",
            "Reset settings, rules, and categories to their defaults? Monitored folders and activity "
            "history will be kept.") != QMessageBox::Yes)
        return;
    m_db.resetToDefaults();
    m_organizer.reloadRules();
    loadValues();
    QMessageBox::information(this, "Bytes", "Configuration reset to defaults.");
}

} // namespace bytes::ui
