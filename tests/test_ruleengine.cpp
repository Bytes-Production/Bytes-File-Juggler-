#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QFileInfo>
#include "core/RuleEngine.h"
#include "database/Database.h"

using namespace bytes::core;
using namespace bytes::database;

/// Covers: custom rule matching (extension/wildcard/contains), and rule
/// priority ordering (a specific wildcard rule must win over a generic
/// extension rule for the same file).
class TestRuleEngine : public QObject {
    Q_OBJECT
private slots:
    void init();
    void cleanup();

    void customRuleMatch();
    void rulePriorityOrdering();
    void skipRule();
    void autoOptimizeOrdersSpecificFirst();

private:
    QTemporaryDir *m_dir = nullptr;
    Database *m_db = nullptr;
};

void TestRuleEngine::init() {
    m_dir = new QTemporaryDir();
    m_db = new Database();
    QVERIFY(m_db->openAt(m_dir->filePath("test.db")));
}

void TestRuleEngine::cleanup() {
    m_db->close();
    delete m_db;
    delete m_dir;
}

void TestRuleEngine::customRuleMatch() {
    Rule rule;
    rule.name = "Invoices";
    rule.conditionType = "extension";
    rule.conditionValue = "pdf";
    rule.actionType = "custom_destination";
    rule.actionValue = "Documents/Invoices";
    rule.priority = 500;
    rule.enabled = true;
    m_db->addRule(rule);

    RuleEngine engine(*m_db);
    QFileInfo info(m_dir->filePath("invoice.pdf"));
    QFile f(info.absoluteFilePath());
    f.open(QIODevice::WriteOnly);
    f.write("test");
    f.close();
    info.refresh();

    const auto decision = engine.evaluate(info, m_dir->path());
    QCOMPARE(decision.matchedRuleName, QString("Invoices"));
    QCOMPARE(decision.destinationRelative, QString("Documents/Invoices"));
}

void TestRuleEngine::rulePriorityOrdering() {
    Rule specific;
    specific.name = "Invoice Wildcard";
    specific.conditionType = "wildcard";
    specific.conditionValue = "invoice*.pdf";
    specific.actionType = "custom_destination";
    specific.actionValue = "Documents/Invoices";
    specific.priority = 500;
    specific.enabled = true;
    m_db->addRule(specific);

    Rule generic;
    generic.name = "All PDFs";
    generic.conditionType = "wildcard";
    generic.conditionValue = "*.pdf";
    generic.actionType = "custom_destination";
    generic.actionValue = "Documents/PDF";
    generic.priority = 100;
    generic.enabled = true;
    m_db->addRule(generic);

    RuleEngine engine(*m_db);
    QFileInfo info(m_dir->filePath("invoice-2024.pdf"));
    QFile f(info.absoluteFilePath());
    f.open(QIODevice::WriteOnly);
    f.close();
    info.refresh();

    const auto decision = engine.evaluate(info, m_dir->path());
    QCOMPARE(decision.matchedRuleName, QString("Invoice Wildcard"));
    QCOMPARE(decision.destinationRelative, QString("Documents/Invoices"));
}

void TestRuleEngine::skipRule() {
    Rule rule;
    rule.name = "Ignore logs";
    rule.conditionType = "endswith";
    rule.conditionValue = ".log";
    rule.actionType = "skip";
    rule.priority = 500;
    rule.enabled = true;
    m_db->addRule(rule);

    RuleEngine engine(*m_db);
    QFileInfo info(m_dir->filePath("debug.log"));
    QFile f(info.absoluteFilePath());
    f.open(QIODevice::WriteOnly);
    f.close();
    info.refresh();

    const auto decision = engine.evaluate(info, m_dir->path());
    QVERIFY(decision.skip);
}

void TestRuleEngine::autoOptimizeOrdersSpecificFirst() {
    Rule generic;
    generic.id = 1;
    generic.conditionType = "extension";
    generic.conditionValue = "pdf";

    Rule specific;
    specific.id = 2;
    specific.conditionType = "wildcard";
    specific.conditionValue = "invoice-2024-final.pdf";

    QVector<Rule> rules = {generic, specific};
    const auto optimized = RuleEngine::autoOptimized(rules);

    QCOMPARE(optimized.first().id, specific.id);
}

QTEST_MAIN(TestRuleEngine)
#include "test_ruleengine.moc"
