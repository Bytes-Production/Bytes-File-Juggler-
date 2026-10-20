#include "ShellUtils.h"

#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>
#include <QProcess>

#include <Windows.h>
#include <shlobj.h>
#include <KnownFolders.h>

namespace bytes::windows::ShellUtils {

namespace {
QString knownFolder(REFKNOWNFOLDERID id, QStandardPaths::StandardLocation fallback) {
    PWSTR path = nullptr;
    QString result;
    if (SUCCEEDED(SHGetKnownFolderPath(id, 0, nullptr, &path))) {
        result = QDir::fromNativeSeparators(QString::fromWCharArray(path));
        CoTaskMemFree(path);
    }
    if (result.isEmpty())
        result = QStandardPaths::writableLocation(fallback);
    return result;
}
}

QString knownDownloadsFolder() {
    return knownFolder(FOLDERID_Downloads, QStandardPaths::DownloadLocation);
}

QString knownDesktopFolder() {
    return knownFolder(FOLDERID_Desktop, QStandardPaths::DesktopLocation);
}

QString knownDocumentsFolder() {
    return knownFolder(FOLDERID_Documents, QStandardPaths::DocumentsLocation);
}

bool openInExplorer(const QString &path) {
    return QProcess::startDetached("explorer.exe", {QDir::toNativeSeparators(path)});
}

bool isProtectedUserFolder(const QString &path) {
    const QString clean = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
    const QStringList protectedFolders = {
        QDir::cleanPath(knownDownloadsFolder()),
        QDir::cleanPath(knownDesktopFolder()),
        QDir::cleanPath(knownDocumentsFolder()),
    };
    for (const QString &p : protectedFolders) {
        if (!p.isEmpty() && clean.compare(p, Qt::CaseInsensitive) == 0)
            return true;
    }
    return false;
}

bool isFileInUse(const QString &path) {
    const std::wstring wpath = QDir::toNativeSeparators(path).toStdWString();
    HANDLE h = CreateFileW(wpath.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE)
        return GetLastError() == ERROR_SHARING_VIOLATION;
    CloseHandle(h);
    return false;
}

bool deleteFilePermanently(const QString &path, QString &errorOut) {
    const std::wstring wpath = QDir::toNativeSeparators(path).toStdWString();

    DWORD attrs = GetFileAttributesW(wpath.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        errorOut = "File not found";
        return false;
    }
    if (attrs & FILE_ATTRIBUTE_READONLY)
        SetFileAttributesW(wpath.c_str(), attrs & ~FILE_ATTRIBUTE_READONLY);

    if (DeleteFileW(wpath.c_str()))
        return true;

    const DWORD err = GetLastError();
    if (err == ERROR_SHARING_VIOLATION || err == ERROR_ACCESS_DENIED || err == ERROR_LOCK_VIOLATION) {
        errorOut = "SKIPPED — FILE IN USE";
    } else {
        wchar_t buffer[512] = {};
        FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, err, 0, buffer, 511, nullptr);
        errorOut = QString::fromWCharArray(buffer).trimmed();
    }
    return false;
}

} // namespace bytes::windows::ShellUtils
