#include "FolderIconManager.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>

#include <Windows.h>
#include <shlobj.h>

namespace bytes::windows {

namespace {
// Marker written into every desktop.ini Bytes creates, so we can safely
// tell "a folder Bytes decorated" apart from any other folder that
// happens to have its own desktop.ini.
constexpr const char *kMarker = "; Managed by Bytes -- File Juggler & Cleaner";
}

FolderIconManager::FolderIconManager(QString iconResourcePath, QString stableIconDestinationDir)
    : m_iconResourcePath(std::move(iconResourcePath)), m_stableIconDir(std::move(stableIconDestinationDir)) {}

QString FolderIconManager::ensureStableIconFile() const {
    QDir().mkpath(m_stableIconDir);
    const QString dest = QDir(m_stableIconDir).filePath("folder.ico");
    if (!QFileInfo::exists(dest))
        QFile::copy(m_iconResourcePath, dest);
    return dest;
}

bool FolderIconManager::applyBytesIcon(const QString &folderPath) const {
    QDir dir(folderPath);
    if (!dir.exists())
        return false;

    const QString iconPath = ensureStableIconFile();
    if (!QFileInfo::exists(iconPath))
        return false; // couldn't stage the icon; leave the folder untouched

    const QString iniPath = dir.filePath("desktop.ini");

    // Never clobber a desktop.ini Bytes did not create.
    if (QFileInfo::exists(iniPath) && !isManagedByBytes(folderPath))
        return false;

    // Use the actual documented Shell API for per-folder custom icons
    // instead of hand-writing desktop.ini and toggling the folder's
    // read-only bit: that old trick is honored inconsistently across
    // Windows versions/builds, which is exactly what caused the icon to
    // silently not show up on some machines. SHGetSetFolderCustomSettings
    // is what Explorer itself uses and reliably manages the file,
    // attributes, and icon-cache refresh.
    const std::wstring wFolder = QDir::toNativeSeparators(folderPath).toStdWString();
    std::wstring wIconPath = QDir::toNativeSeparators(iconPath).toStdWString();

    SHFOLDERCUSTOMSETTINGS fcs = {};
    fcs.dwSize = sizeof(fcs);
    fcs.dwMask = FCSM_ICONFILE;
    fcs.pszIconFile = wIconPath.data();
    fcs.cchIconFile = 0;
    fcs.iIconIndex = 0;

    if (FAILED(SHGetSetFolderCustomSettings(&fcs, wFolder.c_str(), FCS_FORCEWRITE)))
        return false;

    // SHGetSetFolderCustomSettings owns desktop.ini's [.ShellClassInfo]
    // section but doesn't let us add our own marker comment, so prepend it
    // afterwards -- this is how isManagedByBytes() and removeBytesIcon()
    // recognize a Bytes-managed folder later.
    SetFileAttributesW(reinterpret_cast<LPCWSTR>(iniPath.utf16()), FILE_ATTRIBUTE_NORMAL);
    QString existingContent;
    {
        QFile ini(iniPath);
        if (ini.open(QIODevice::ReadOnly | QIODevice::Text))
            existingContent = QString::fromUtf8(ini.readAll());
    }
    if (!existingContent.startsWith(QString::fromLatin1(kMarker))) {
        QFile ini(iniPath);
        if (ini.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
            QTextStream out(&ini);
            out << kMarker << "\n" << existingContent;
        }
    }
    SetFileAttributesW(reinterpret_cast<LPCWSTR>(iniPath.utf16()), FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM);

    // Ask Explorer to actually redraw this folder's icon now, rather than
    // waiting for its own cache to notice.
    SHChangeNotify(SHCNE_UPDATEITEM, SHCNF_PATHW, wFolder.c_str(), nullptr);
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    return true;
}

bool FolderIconManager::isManagedByBytes(const QString &folderPath) {
    const QString iniPath = QDir(folderPath).filePath("desktop.ini");
    QFile ini(iniPath);
    if (!ini.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;
    const QString firstLine = QString::fromUtf8(ini.readLine()).trimmed();
    return firstLine == QString::fromLatin1(kMarker);
}

bool FolderIconManager::removeBytesIcon(const QString &folderPath) const {
    if (!isManagedByBytes(folderPath))
        return false;

    const QString iniPath = QDir(folderPath).filePath("desktop.ini");
    SetFileAttributesW(reinterpret_cast<LPCWSTR>(iniPath.utf16()), FILE_ATTRIBUTE_NORMAL);
    QFile::remove(iniPath);

    const std::wstring wFolder = QDir::toNativeSeparators(folderPath).toStdWString();
    DWORD attrs = GetFileAttributesW(wFolder.c_str());
    if (attrs != INVALID_FILE_ATTRIBUTES)
        SetFileAttributesW(wFolder.c_str(), attrs & ~FILE_ATTRIBUTE_READONLY);

    SHChangeNotify(SHCNE_UPDATEITEM, SHCNF_PATHW, wFolder.c_str(), nullptr);
    return true;
}

} // namespace bytes::windows
