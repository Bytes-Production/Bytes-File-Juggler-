#pragma once

#include "../core/RuleEngine.h"

#include <QDialog>
#include <QLabel>

namespace bytes::ui {

/// Mandatory "Rule Tester": pick any file and see exactly how Bytes would
/// classify and route it, without touching the file.
class RuleTesterDialog : public QDialog {
    Q_OBJECT
public:
    explicit RuleTesterDialog(core::RuleEngine &ruleEngine, QWidget *parent = nullptr);

private:
    core::RuleEngine &m_ruleEngine;

    QLabel *m_fileLabel = nullptr;
    QLabel *m_extensionLabel = nullptr;
    QLabel *m_typeLabel = nullptr;
    QLabel *m_ruleLabel = nullptr;
    QLabel *m_destinationLabel = nullptr;
    QLabel *m_resultLabel = nullptr;

    void chooseFile();
    void testFile(const QString &path);
};

} // namespace bytes::ui
