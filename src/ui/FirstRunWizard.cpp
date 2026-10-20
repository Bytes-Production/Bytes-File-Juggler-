#include "FirstRunWizard.h"
#include "../windows/ShellUtils.h"

#include <QVBoxLayout>
#include <QLabel>
#include <QWizardPage>

namespace bytes::ui {

FirstRunWizard::FirstRunWizard(QWidget *parent) : QWizard(parent) {
    setWindowTitle("Welcome to Bytes");
    setWizardStyle(QWizard::ModernStyle);
    resize(560, 420);

    buildWelcomePage();
    buildFoldersPage();
    buildOrganizationPage();
    buildGracePeriodPage();
    buildCleanerPage();
    buildStartupPage();
}

void FirstRunWizard::buildWelcomePage() {
    auto *page = new QWizardPage(this);
    page->setTitle("Welcome to Bytes");
    auto *layout = new QVBoxLayout(page);
    auto *label = new QLabel(
        "Let's organize your files.\n\n"
        "Bytes will watch the folders you choose and automatically sort new "
        "files into tidy subfolders — safely, and only when you say so.", page);
    label->setWordWrap(true);
    layout->addWidget(label);
    addPage(page);
}

void FirstRunWizard::buildFoldersPage() {
    auto *page = new QWizardPage(this);
    page->setTitle("Step 1 — Choose Folders");
    auto *layout = new QVBoxLayout(page);
    m_downloadsCheck = new QCheckBox(QString("Monitor Downloads (%1)").arg(windows::ShellUtils::knownDownloadsFolder()), page);
    m_downloadsCheck->setChecked(true);
    layout->addWidget(m_downloadsCheck);
    layout->addWidget(new QLabel("You can add more folders later from the Folders page.", page));
    addPage(page);
}

void FirstRunWizard::buildOrganizationPage() {
    auto *page = new QWizardPage(this);
    page->setTitle("Step 2 — Organization");
    auto *layout = new QVBoxLayout(page);
    m_smartRadio = new QRadioButton("Smart Organization (recommended) — sort by file type automatically", page);
    m_customRadio = new QRadioButton("Custom Rules Only — I'll set up my own rules", page);
    m_smartRadio->setChecked(true);
    layout->addWidget(m_smartRadio);
    layout->addWidget(m_customRadio);
    addPage(page);
}

void FirstRunWizard::buildGracePeriodPage() {
    auto *page = new QWizardPage(this);
    page->setTitle("Step 3 — Grace Period");
    auto *layout = new QVBoxLayout(page);
    layout->addWidget(new QLabel("How long should Bytes wait before organizing a new file?\n"
                                  "(This avoids moving files that are still downloading.)", page));
    m_graceCombo = new QComboBox(page);
    m_graceCombo->addItem("Immediately", 0);
    m_graceCombo->addItem("10 seconds", 10);
    m_graceCombo->addItem("30 seconds", 30);
    m_graceCombo->addItem("1 minute", 60);
    m_graceCombo->addItem("2 minutes", 120);
    m_graceCombo->addItem("5 minutes", 300);
    m_graceCombo->addItem("10 minutes", 600);
    m_graceCombo->setCurrentIndex(3); // 1 minute default
    layout->addWidget(m_graceCombo);
    addPage(page);
}

void FirstRunWizard::buildCleanerPage() {
    auto *page = new QWizardPage(this);
    page->setTitle("Step 4 — Cleaner");
    auto *layout = new QVBoxLayout(page);
    m_userTempCheck = new QCheckBox("Clean User Temp automatically", page);
    m_windowsTempCheck = new QCheckBox("Clean Windows Temp automatically", page);
    m_recycleBinCheck = new QCheckBox("Empty Recycle Bin automatically", page);
    m_userTempCheck->setChecked(true);
    m_windowsTempCheck->setChecked(true);
    m_recycleBinCheck->setChecked(false);
    layout->addWidget(m_userTempCheck);
    layout->addWidget(m_windowsTempCheck);
    layout->addWidget(m_recycleBinCheck);
    layout->addWidget(new QLabel("Recycle Bin auto-cleaning is off by default and can be turned on later.", page));
    addPage(page);
}

void FirstRunWizard::buildStartupPage() {
    auto *page = new QWizardPage(this);
    page->setTitle("Step 5 — Startup");
    auto *layout = new QVBoxLayout(page);
    m_startupCheck = new QCheckBox("Start Bytes with Windows", page);
    layout->addWidget(m_startupCheck);
    layout->addWidget(new QLabel("You're all set. Click Finish to start using Bytes.", page));
    addPage(page);
}

FirstRunChoices FirstRunWizard::choices() const {
    FirstRunChoices c;
    c.monitorDownloads = m_downloadsCheck->isChecked();
    c.smartOrganization = m_smartRadio->isChecked();
    c.gracePeriodSeconds = m_graceCombo->currentData().toInt();
    c.cleanUserTemp = m_userTempCheck->isChecked();
    c.cleanWindowsTemp = m_windowsTempCheck->isChecked();
    c.cleanRecycleBin = m_recycleBinCheck->isChecked();
    c.startWithWindows = m_startupCheck->isChecked();
    return c;
}

} // namespace bytes::ui
