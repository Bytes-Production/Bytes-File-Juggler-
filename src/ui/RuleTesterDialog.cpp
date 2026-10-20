#include "RuleTesterDialog.h"
#include "../core/FileClassifier.h"
#include <QFont>

#include <QVBoxLayout>
#include <QFormLayout>
#include <QPushButton>
#include <QFileDialog>
#include <QFileInfo>

namespace bytes::ui {

RuleTesterDialog::RuleTesterDialog(core::RuleEngine &ruleEngine, QWidget *parent)
    : QDialog(parent), m_ruleEngine(ruleEngine) {
    setWindowTitle("Rule Tester");
    setMinimumWidth(420);

    auto *layout = new QVBoxLayout(this);

    auto *chooseBtn = new QPushButton("Choose File…", this);
    connect(chooseBtn, &QPushButton::clicked, this, &RuleTesterDialog::chooseFile);
    layout->addWidget(chooseBtn);

    auto *form = new QFormLayout();
    m_fileLabel = new QLabel("—", this);
    m_extensionLabel = new QLabel("—", this);
    m_typeLabel = new QLabel("—", this);
    m_ruleLabel = new QLabel("—", this);
    m_destinationLabel = new QLabel("—", this);
    m_resultLabel = new QLabel("—", this);

    for (auto *l : {m_fileLabel, m_extensionLabel, m_typeLabel, m_ruleLabel, m_destinationLabel, m_resultLabel})
        l->setWordWrap(true);

    QFont resultFont = m_resultLabel->font();
    resultFont.setBold(true);
    m_resultLabel->setFont(resultFont);

    form->addRow("File:", m_fileLabel);
    form->addRow("Extension:", m_extensionLabel);
    form->addRow("Detected Type:", m_typeLabel);
    form->addRow("Matched Rule:", m_ruleLabel);
    form->addRow("Destination:", m_destinationLabel);
    form->addRow("Result:", m_resultLabel);
    layout->addLayout(form);
}

void RuleTesterDialog::chooseFile() {
    const QString path = QFileDialog::getOpenFileName(this, "Choose a file to test");
    if (!path.isEmpty())
        testFile(path);
}

void RuleTesterDialog::testFile(const QString &path) {
    const QFileInfo info(path);
    m_fileLabel->setText(info.fileName());
    m_extensionLabel->setText("." + core::FileClassifier::normalizeExtension(info.fileName()));

    const core::RuleDecision decision = m_ruleEngine.evaluate(info, info.absolutePath());
    m_typeLabel->setText(decision.detectedType.isEmpty() ? "Other" : decision.detectedType);
    m_ruleLabel->setText(decision.matchedRuleName.isEmpty() ? "Built-in classifier" : decision.matchedRuleName);

    if (decision.skip) {
        m_destinationLabel->setText("—");
        m_resultLabel->setText("⏭ Will Skip");
    } else {
        const QString dest = !decision.destinationAbsolute.isEmpty()
            ? decision.destinationAbsolute
            : decision.destinationRelative;
        m_destinationLabel->setText(dest);
        m_resultLabel->setText("✓ Will Move");
    }
}

} // namespace bytes::ui
