#pragma once

#include <QString>

namespace bytes::windows {

/// Manages "Start Bytes with Windows" via the per-user registry Run key
/// (HKCU\...\Run). Deliberately does NOT use a scheduled task or service
/// for this -- the Run key requires no elevation and is the standard,
/// user-consented mechanism for a tray application to start at logon.
class StartupManager {
public:
    /// Enables or disables startup. `executablePath` should be the full
    /// path to Bytes.exe; Bytes always launches with `--tray` so a
    /// startup launch goes straight to the background/tray, not a visible
    /// window.
    static bool setEnabled(bool enabled, const QString &executablePath);
    static bool isEnabled();

private:
    static const wchar_t *runKeyPath();
    static const wchar_t *valueName();
};

} // namespace bytes::windows
