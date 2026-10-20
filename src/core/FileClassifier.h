#pragma once

#include <QString>
#include <QHash>
#include <QStringList>

namespace bytes::core {

/// Result of classifying a single file by name/extension (and optionally
/// file signature). Classification NEVER renames or moves anything by
/// itself -- it only answers "what kind of file is this and where would
/// Bytes put it by default".
struct ClassificationResult {
    QString category;      // e.g. "Images", "Documents" (top-level)
    QString subPath;        // e.g. "PDF" for Documents/PDF, empty otherwise
    QString extension;      // normalized, lowercase, no dot
    bool matchedBySignature = false; // true if extension was missing/unknown
                                      // and a file-signature sniff was used
};

/// Built-in, always-available file classifier. This provides the default
/// extension -> category mapping described in the Bytes specification.
/// The RuleEngine consults this as a fallback when no user rule matches.
class FileClassifier {
public:
    FileClassifier();

    /// Classifies by extension alone (cheap, no I/O).
    ClassificationResult classifyByName(const QString &fileName) const;

    /// Classifies by extension first; if the extension is missing or not
    /// recognized, and signature detection is enabled, peeks at the first
    /// bytes of the file to guess a type. Falls back to category "Other".
    ClassificationResult classify(const QString &filePath, bool useSignatureDetection) const;

    /// Full relative destination path for a category, e.g. "Documents/PDF".
    static QString destinationForResult(const ClassificationResult &result);

    /// Extensions Bytes treats as "still downloading" -- never organized.
    static const QStringList &temporaryDownloadExtensions();
    static bool isTemporaryDownloadName(const QString &fileName);

    /// Returns the built-in extension -> category-name map (no dots, lowercase keys).
    static const QHash<QString, QString> &builtinExtensionMap();

    /// Returns the built-in extension -> Documents-subfolder map (pdf->PDF, etc).
    static const QHash<QString, QString> &builtinDocumentSubMap();

    static QString normalizeExtension(const QString &fileNameOrExt);

private:
    ClassificationResult classifyBySignature(const QString &filePath) const;
};

} // namespace bytes::core
