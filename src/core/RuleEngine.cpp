#include "RuleEngine.h"

#include <QRegularExpression>
#include <QDateTime>
#include <QMutexLocker>
#include <QDir>
#include <algorithm>

namespace bytes::core {

using bytes::database::Rule;
using bytes::database::Category;

RuleEngine::RuleEngine(database::Database &db) : m_db(db) {
    reload();
}

void RuleEngine::reload() {
    auto rules = m_db.rules();
    auto categories = m_db.categories();

    QMutexLocker lock(&m_mutex);
    m_rules = std::move(rules);
    m_categories = std::move(categories);
}

bool RuleEngine::wildcardMatch(const QString &pattern, const QString &text) {
    const QRegularExpression re(QRegularExpression::wildcardToRegularExpression(pattern),
                                 QRegularExpression::CaseInsensitiveOption);
    return re.match(text).hasMatch();
}

bool RuleEngine::conditionMatches(const Rule &rule, const QFileInfo &fileInfo, const QString &sourceFolder) const {
    const QString name = fileInfo.fileName();
    const QString ext = FileClassifier::normalizeExtension(name);
    const QString value = rule.conditionValue;

    if (rule.conditionType == "extension") {
        const QStringList list = value.split(',', Qt::SkipEmptyParts);
        for (QString e : list) {
            e = e.trimmed();
            if (e.startsWith('.'))
                e = e.mid(1);
            if (e.compare(ext, Qt::CaseInsensitive) == 0)
                return true;
        }
        return false;
    }
    if (rule.conditionType == "filetype") {
        FileClassifier classifier;
        const auto result = classifier.classifyByName(name);
        return result.category.compare(value, Qt::CaseInsensitive) == 0;
    }
    if (rule.conditionType == "contains")
        return name.contains(value, Qt::CaseInsensitive);
    if (rule.conditionType == "startswith")
        return name.startsWith(value, Qt::CaseInsensitive);
    if (rule.conditionType == "endswith")
        return name.endsWith(value, Qt::CaseInsensitive);
    if (rule.conditionType == "wildcard")
        return wildcardMatch(value, name);
    if (rule.conditionType == "source_folder")
        return QDir::cleanPath(sourceFolder).compare(QDir::cleanPath(value), Qt::CaseInsensitive) == 0;
    if (rule.conditionType == "size") {
        // value format: "<10MB", ">1GB", ">=500KB", "<=2GB"
        QRegularExpression re(R"(^(<=|>=|<|>)\s*(\d+(?:\.\d+)?)\s*(B|KB|MB|GB)?$)",
                               QRegularExpression::CaseInsensitiveOption);
        auto m = re.match(value.trimmed());
        if (!m.hasMatch())
            return false;
        const QString op = m.captured(1);
        double num = m.captured(2).toDouble();
        const QString unit = m.captured(3).toUpper();
        qint64 multiplier = 1;
        if (unit == "KB") multiplier = 1024LL;
        else if (unit == "MB") multiplier = 1024LL * 1024;
        else if (unit == "GB") multiplier = 1024LL * 1024 * 1024;
        const qint64 threshold = static_cast<qint64>(num * multiplier);
        const qint64 size = fileInfo.size();
        if (op == "<") return size < threshold;
        if (op == "<=") return size <= threshold;
        if (op == ">") return size > threshold;
        if (op == ">=") return size >= threshold;
        return false;
    }
    if (rule.conditionType == "date") {
        // value format: "older_than:<days>" or "newer_than:<days>"
        const QStringList parts = value.split(':');
        if (parts.size() != 2)
            return false;
        bool ok = false;
        const int days = parts[1].toInt(&ok);
        if (!ok)
            return false;
        const qint64 ageDays = fileInfo.lastModified().daysTo(QDateTime::currentDateTime());
        if (parts[0] == "older_than")
            return ageDays >= days;
        if (parts[0] == "newer_than")
            return ageDays <= days;
        return false;
    }

    return false;
}

RuleDecision RuleEngine::evaluate(const QFileInfo &fileInfo, const QString &sourceFolderPath) const {
    QMutexLocker lock(&m_mutex);
    RuleDecision decision;

    for (const Rule &rule : m_rules) {
        if (!rule.enabled)
            continue;
        if (!rule.scopeFolder.isEmpty() &&
            QDir::cleanPath(rule.scopeFolder).compare(QDir::cleanPath(sourceFolderPath), Qt::CaseInsensitive) != 0)
            continue; // this rule only applies to a different monitored folder
        if (!conditionMatches(rule, fileInfo, sourceFolderPath))
            continue;

        decision.matchedRuleName = rule.name;
        if (rule.actionType == "skip") {
            decision.skip = true;
            return decision;
        }
        if (rule.actionType == "custom_destination") {
            const QString dest = rule.actionValue;
            if (QDir::isAbsolutePath(dest))
                decision.destinationAbsolute = QDir::toNativeSeparators(dest);
            else
                decision.destinationRelative = dest;
            decision.detectedType = rule.name;
            return decision;
        }
        // "move" with no explicit destination: fall through to built-in
        // classification for the actual folder name, but keep credit for
        // this rule name.
        break;
    }

    // No rule decided the outcome explicitly -- use built-in classifier +
    // categories table (which may have user-edited destinations/priorities).
    FileClassifier classifier;
    const ClassificationResult result = classifier.classifyByName(fileInfo.fileName());

    // Prefer a category row (user may have renamed the destination folder
    // or disabled the category entirely). If a matching category exists
    // but is disabled, Bytes leaves the file alone rather than silently
    // falling back to the raw built-in mapping -- "disabled" must mean
    // disabled.
    for (const Category &cat : m_categories) {
        if (cat.name.compare(result.category, Qt::CaseInsensitive) != 0)
            continue;
        if (!cat.enabled) {
            decision.skip = true;
            decision.detectedType = cat.name;
            return decision;
        }
        decision.destinationRelative = cat.destination.isEmpty()
            ? FileClassifier::destinationForResult(result)
            : cat.destination;
        decision.detectedType = cat.name;
        decision.usedFallbackClassifier = true;
        return decision;
    }

    decision.destinationRelative = FileClassifier::destinationForResult(result);
    decision.detectedType = result.category;
    decision.usedFallbackClassifier = true;
    return decision;
}

int RuleEngine::specificityScore(const Rule &rule) {
    int score = 0;
    const QString &v = rule.conditionValue;

    if (rule.conditionType == "wildcard") {
        // Count literal (non "*"/"?") characters as specificity.
        for (QChar c : v)
            if (c != '*' && c != '?')
                score++;
    } else if (rule.conditionType == "contains" || rule.conditionType == "startswith" ||
               rule.conditionType == "endswith") {
        score = 50 + v.length();
    } else if (rule.conditionType == "source_folder") {
        score = 40;
    } else if (rule.conditionType == "size" || rule.conditionType == "date") {
        score = 30;
    } else if (rule.conditionType == "filetype") {
        score = 20;
    } else if (rule.conditionType == "extension") {
        // Fewer extensions listed = more specific; base score modest so
        // filename-based rules generally win over plain extension rules.
        const int count = v.split(',', Qt::SkipEmptyParts).count();
        score = 10 + qMax(0, 5 - count);
    }
    return score;
}

QVector<Rule> RuleEngine::autoOptimized(QVector<Rule> rules) {
    std::stable_sort(rules.begin(), rules.end(), [](const Rule &a, const Rule &b) {
        return specificityScore(a) > specificityScore(b);
    });

    // Re-assign priority numbers with generous spacing so users can still
    // manually nudge individual rules between the computed slots later.
    int priority = 1000;
    for (Rule &r : rules) {
        r.priority = priority;
        priority -= 10;
    }
    return rules;
}

} // namespace bytes::core
