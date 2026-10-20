#include "FileClassifier.h"

#include <QFileInfo>
#include <QFile>
#include <QByteArray>

namespace bytes::core {

namespace {

const QHash<QString, QString> &buildExtensionMap() {
    static const QHash<QString, QString> map = [] {
        QHash<QString, QString> m;
        auto add = [&m](const char *category, std::initializer_list<const char *> exts) {
            for (const char *e : exts)
                m.insert(QString::fromLatin1(e), QString::fromLatin1(category));
        };

        add("Images", {"jpg","jpeg","png","gif","webp","bmp","tif","tiff","svg","ico","heic","heif",
                        "avif","raw","cr2","cr3","nef","arw","dng","orf","rw2"});
        add("Videos", {"mp4","mkv","avi","mov","wmv","webm","flv","mpeg","mpg","m4v","3gp","ts","mts","m2ts"});
        add("Music", {"mp3","wav","flac","aac","m4a","ogg","oga","opus","wma","aiff","alac"});
        add("Documents", {"doc","docx","odt","rtf","txt","md","tex","pages"});
        add("Documents", {"pdf"});                                        // subPath -> PDF
        add("Documents", {"xls","xlsx","xlsm","csv","ods","numbers","tsv"}); // subPath -> Spreadsheets
        add("Documents", {"ppt","pptx","pptm","odp","key"});               // subPath -> Presentations
        add("Archives", {"zip","rar","7z","tar","gz","bz2","xz","tgz","cab"});
        add("Installers", {"exe","msi","msix","appx","appxbundle"});
        add("Disk Images", {"iso","img","bin","vhd","vhdx"});
        add("Development", {"py","js","ts","jsx","tsx","java","c","cpp","h","hpp","cs","go","rs","php",
                             "rb","swift","kt","kts","dart","lua","sql","html","htm","css","scss","sass",
                             "json","xml","yaml","yml","toml","ini","env","sh","bat","cmd","ps1"});
        add("3D & CAD", {"blend","fbx","obj","stl","3ds","dae","gltf","glb","step","stp","iges","igs","dwg","dxf","skp"});
        add("Fonts", {"ttf","otf","woff","woff2","eot"});
        add("Ebooks", {"epub","mobi","azw","azw3","cbz","cbr"});
        add("Database", {"db","sqlite","sqlite3","mdb","accdb","dbf"});
        add("Backups", {"bak","old","backup"});
        add("Torrents", {"torrent"});
        add("Shortcuts", {"lnk","url"});
        add("Data", {"vcf","ics"});
        return m;
    }();
    return map;
}

const QHash<QString, QString> &buildDocumentSubMap() {
    static const QHash<QString, QString> map = [] {
        QHash<QString, QString> m;
        auto add = [&m](const char *sub, std::initializer_list<const char *> exts) {
            for (const char *e : exts)
                m.insert(QString::fromLatin1(e), QString::fromLatin1(sub));
        };
        add("Word", {"doc","docx","odt","rtf","txt","md","tex","pages"});
        add("PDF", {"pdf"});
        add("Spreadsheets", {"xls","xlsx","xlsm","csv","ods","numbers","tsv"});
        add("Presentations", {"ppt","pptx","pptm","odp","key"});
        return m;
    }();
    return map;
}

} // namespace

FileClassifier::FileClassifier() = default;

QString FileClassifier::normalizeExtension(const QString &fileNameOrExt) {
    const int dot = fileNameOrExt.lastIndexOf('.');
    if (dot < 0)
        return QString(); // no extension at all -- e.g. "README"
    return fileNameOrExt.mid(dot + 1).trimmed().toLower();
}

const QStringList &FileClassifier::temporaryDownloadExtensions() {
    static const QStringList exts = {"crdownload", "part", "tmp", "download", "partial"};
    return exts;
}

bool FileClassifier::isTemporaryDownloadName(const QString &fileName) {
    const QString ext = normalizeExtension(fileName);
    return temporaryDownloadExtensions().contains(ext);
}

const QHash<QString, QString> &FileClassifier::builtinExtensionMap() {
    return buildExtensionMap();
}

const QHash<QString, QString> &FileClassifier::builtinDocumentSubMap() {
    return buildDocumentSubMap();
}

ClassificationResult FileClassifier::classifyByName(const QString &fileName) const {
    ClassificationResult result;
    result.extension = normalizeExtension(fileName);

    if (result.extension.isEmpty() || !builtinExtensionMap().contains(result.extension)) {
        result.category = "Other";
        return result;
    }

    result.category = builtinExtensionMap().value(result.extension);
    if (result.category == "Documents")
        result.subPath = builtinDocumentSubMap().value(result.extension);
    return result;
}

ClassificationResult FileClassifier::classifyBySignature(const QString &filePath) const {
    ClassificationResult result;
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly)) {
        result.category = "Other";
        return result;
    }
    const QByteArray head = f.read(16);
    result.matchedBySignature = true;

    auto starts = [&](const char *sig, int len) {
        return head.size() >= len && head.startsWith(QByteArray(sig, len));
    };

    if (starts("\x89PNG", 4)) { result.category = "Images"; return result; }
    if (starts("\xFF\xD8\xFF", 3)) { result.category = "Images"; return result; }
    if (starts("GIF87a", 6) || starts("GIF89a", 6)) { result.category = "Images"; return result; }
    if (starts("BM", 2)) { result.category = "Images"; return result; }
    if (starts("%PDF", 4)) { result.category = "Documents"; result.subPath = "PDF"; return result; }
    if (starts("PK\x03\x04", 4)) { result.category = "Archives"; return result; }
    if (starts("Rar!", 4)) { result.category = "Archives"; return result; }
    if (starts("7z\xBC\xAF\x27\x1C", 6)) { result.category = "Archives"; return result; }
    if (starts("MZ", 2)) { result.category = "Installers"; return result; }
    if (starts("ID3", 3)) { result.category = "Music"; return result; }
    if (starts("OggS", 4)) { result.category = "Music"; return result; }
    if (starts("SQLite format 3", 15)) { result.category = "Database"; return result; }

    result.category = "Other";
    result.matchedBySignature = false; // uncertain -> Other, no false confidence
    return result;
}

ClassificationResult FileClassifier::classify(const QString &filePath, bool useSignatureDetection) const {
    const QFileInfo info(filePath);
    ClassificationResult byName = classifyByName(info.fileName());

    if (byName.category != "Other")
        return byName;

    // Extension missing or unrecognized. Never rename the file based on
    // this -- we only use it to choose a *destination* category, and if
    // we cannot be reasonably sure, it goes to "Other".
    if (useSignatureDetection && info.exists() && info.isReadable() && info.size() > 0) {
        ClassificationResult bySig = classifyBySignature(filePath);
        bySig.extension = byName.extension;
        return bySig;
    }

    return byName;
}

QString FileClassifier::destinationForResult(const ClassificationResult &result) {
    if (!result.subPath.isEmpty())
        return result.category + "/" + result.subPath;
    return result.category;
}

} // namespace bytes::core
