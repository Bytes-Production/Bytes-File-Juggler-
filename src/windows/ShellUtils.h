#pragma once

#include <QString>

namespace bytes::windows {

/// Small, focused wrappers around Windows Shell APIs used throughout Bytes.
/// Kept free of any Bytes business logic so it can be unit-tested and
/// reused (e.g. by both the Cleaner and the Juggler).
namespace ShellUtils {

/// Resolves well-known user folders via SHGetKnownFolderPath (Unicode,
/// long-path aware), falling back to Qt's QStandardPaths if the shell call
/// fails for any reason.
QString knownDownloadsFolder();
QString knownDesktopFolder();
QString knownDocumentsFolder();

/// Opens `path` in a new Windows Explorer window. Never executes the
/// target as a program -- this only ever passes a folder path to explorer.exe.
bool openInExplorer(const QString &path);

/// True if `path` is (or is an ancestor of) a folder Bytes must never
/// delete or organize away entirely: the user's Downloads, Desktop, or
/// Documents folder themselves (their *contents* may still be organized
/// into subfolders -- this only guards against removing the folders).
bool isProtectedUserFolder(const QString &path);

/// Best-effort permanent delete of a single file using the Unicode Win32
/// API (never SHFileOperation's ANSI path, never a forced/elevated
/// delete). Returns false (and leaves the file alone) if it is locked,
/// missing, or access is denied -- callers must treat that as "skip",
/// never as license to retry more aggressively.
bool deleteFilePermanently(const QString &path, QString &errorOut);

/// True if `path` currently cannot be opened for exclusive access (i.e.
/// something else has it open).
bool isFileInUse(const QString &path);

} // namespace ShellUtils
} // namespace bytes::windows
