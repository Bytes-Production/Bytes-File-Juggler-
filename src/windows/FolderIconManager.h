#pragma once

#include <QString>

namespace bytes::windows {

/// Applies Bytes's custom folder icon to folders Bytes itself creates
/// (e.g. Downloads/Images, Downloads/Installers), via a `desktop.ini` +
/// the classic read-only-folder-attribute trick Explorer relies on. Never
/// touches folders it did not create/mark itself.
class FolderIconManager {
public:
    /// `iconResourcePath` is the Qt resource path (e.g. ":/icons/folder.ico")
    /// bundled into the executable; it is copied to a stable on-disk
    /// location once, since desktop.ini needs a real filesystem path.
    explicit FolderIconManager(QString iconResourcePath, QString stableIconDestinationDir);

    /// Marks `folderPath` as Bytes-managed and gives it the Bytes folder
    /// icon. Safe to call repeatedly (idempotent). Returns false only on
    /// unexpected I/O failure; never throws and never touches unrelated
    /// folders.
    bool applyBytesIcon(const QString &folderPath) const;

    /// True if `folderPath`'s desktop.ini carries the Bytes marker,
    /// meaning it is safe for Bytes to modify/remove its icon later.
    static bool isManagedByBytes(const QString &folderPath);

    /// Removes the Bytes-authored desktop.ini and folder attribute,
    /// restoring the default Explorer icon. No-op if not Bytes-managed.
    bool removeBytesIcon(const QString &folderPath) const;

private:
    QString m_iconResourcePath;
    QString m_stableIconDir;

    QString ensureStableIconFile() const;
};

} // namespace bytes::windows
