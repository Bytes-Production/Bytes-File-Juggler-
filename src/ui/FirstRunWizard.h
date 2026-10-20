#pragma once

#include <QWizard>
#include <QCheckBox>
#include <QRadioButton>
#include <QComboBox>

namespace bytes::ui {

struct FirstRunChoices {
    bool monitorDownloads = true;
    bool smartOrganization = true; // vs. "custom rules only"
    int gracePeriodSeconds = 60;
    bool cleanUserTemp = true;
    bool cleanWindowsTemp = true;
    bool cleanRecycleBin = false;
    bool startWithWindows = false;
};

/// The first-launch setup wizard described in the Bytes spec: choose
/// folders, organization mode, grace period, cleaner defaults, and
/// startup behavior.
class FirstRunWizard : public QWizard {
    Q_OBJECT
public:
    explicit FirstRunWizard(QWidget *parent = nullptr);

    FirstRunChoices choices() const;

private:
    QCheckBox *m_downloadsCheck = nullptr;
    QRadioButton *m_smartRadio = nullptr;
    QRadioButton *m_customRadio = nullptr;
    QComboBox *m_graceCombo = nullptr;
    QCheckBox *m_userTempCheck = nullptr;
    QCheckBox *m_windowsTempCheck = nullptr;
    QCheckBox *m_recycleBinCheck = nullptr;
    QCheckBox *m_startupCheck = nullptr;

    void buildWelcomePage();
    void buildFoldersPage();
    void buildOrganizationPage();
    void buildGracePeriodPage();
    void buildCleanerPage();
    void buildStartupPage();
};

} // namespace bytes::ui
