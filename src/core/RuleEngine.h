#pragma once

#include "FileClassifier.h"
#include "../database/Database.h"

#include <QFileInfo>
#include <QMutex>
#include <QVector>

namespace bytes::core {

/// Outcome of evaluating a file against the rule set / built-in classifier.
struct RuleDecision {
    bool skip = false;                 // true if a rule explicitly says "skip"
    QString destinationRelative;       // relative to the monitored folder root, e.g. "Images" or "Documents/PDF"
    QString destinationAbsolute;       // set only when the matching rule/category uses an absolute custom destination
    QString matchedRuleName;           // "" if fell back to built-in classification
    QString detectedType;              // category name, for logging/UI
    bool usedFallbackClassifier = false;
};

/// Evaluates user-defined rules (in priority order, most specific first)
/// against a candidate file, falling back to the built-in FileClassifier
/// plus the categories table when no rule matches.
class RuleEngine {
public:
    explicit RuleEngine(database::Database &db);

    /// Re-reads rules and categories from the database. Cheap enough to
    /// call after any rule/category edit; the watcher does not need to
    /// reload on every file.
    void reload();

    RuleDecision evaluate(const QFileInfo &fileInfo, const QString &sourceFolderPath) const;

    /// Re-numbers rule priorities so more specific conditions are checked
    /// first (e.g. "invoice*.pdf" before "*.pdf"). Specificity heuristic:
    /// longer literal (non-wildcard) prefix/content wins; exact filename
    /// matches (contains/startswith/endswith without wildcards) outrank
    /// bare extension rules.
    static QVector<database::Rule> autoOptimized(QVector<database::Rule> rules);

    bool conditionMatches(const database::Rule &rule, const QFileInfo &fileInfo, const QString &sourceFolder) const;

private:
    database::Database &m_db;
    mutable QMutex m_mutex;
    QVector<database::Rule> m_rules;       // cached, priority-sorted desc
    QVector<database::Category> m_categories;
    FileClassifier m_classifier;

    static int specificityScore(const database::Rule &rule);
    static bool wildcardMatch(const QString &pattern, const QString &text);
};

} // namespace bytes::core
