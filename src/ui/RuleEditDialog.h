#pragma once

#include "../database/Database.h"

#include <QDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>

namespace bytes::ui {

/// Add/Edit dialog for a single rule:
///   IF <condition type> IS <value> THEN <action> <destination>
class RuleEditDialog : public QDialog {
    Q_OBJECT
public:
    /// `monitoredFolders` lists the currently monitored folder paths, so the
    /// user can scope this rule to just one of them (e.g. "only on Desktop")
    /// instead of every monitored folder.
    explicit RuleEditDialog(const QStringList &monitoredFolders, QWidget *parent = nullptr);

    void setRule(const database::Rule &rule);
    database::Rule rule() const;

private:
    QLineEdit *m_nameEdit = nullptr;
    QComboBox *m_conditionType = nullptr;
    QLineEdit *m_conditionValue = nullptr;
    QComboBox *m_actionType = nullptr;
    QLineEdit *m_actionValue = nullptr;
    QPushButton *m_browseButton = nullptr;
    QSpinBox *m_prioritySpin = nullptr;
    QCheckBox *m_enabledCheck = nullptr;
    QLabel *m_hintLabel = nullptr;
    QComboBox *m_scopeFolder = nullptr;

    int m_ruleId = -1;

    void updateHint();
    void browseDestination();
};

} // namespace bytes::ui
