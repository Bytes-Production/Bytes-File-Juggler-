#include "StartupManager.h"

#include <QDir>

#include <Windows.h>

namespace bytes::windows {

const wchar_t *StartupManager::runKeyPath() { return L"Software\\Microsoft\\Windows\\CurrentVersion\\Run"; }
const wchar_t *StartupManager::valueName() { return L"Bytes"; }

bool StartupManager::setEnabled(bool enabled, const QString &executablePath) {
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, runKeyPath(), 0, KEY_SET_VALUE, &key) != ERROR_SUCCESS)
        return false;

    LSTATUS status;
    if (enabled) {
        const QString command = QString("\"%1\" --tray").arg(QDir::toNativeSeparators(executablePath));
        const std::wstring wcommand = command.toStdWString();
        status = RegSetValueExW(key, valueName(), 0, REG_SZ,
                                 reinterpret_cast<const BYTE *>(wcommand.c_str()),
                                 static_cast<DWORD>((wcommand.size() + 1) * sizeof(wchar_t)));
    } else {
        status = RegDeleteValueW(key, valueName());
        if (status == ERROR_FILE_NOT_FOUND)
            status = ERROR_SUCCESS; // already disabled
    }

    RegCloseKey(key);
    return status == ERROR_SUCCESS;
}

bool StartupManager::isEnabled() {
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, runKeyPath(), 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS)
        return false;

    wchar_t buffer[1024] = {};
    DWORD size = sizeof(buffer);
    DWORD type = 0;
    const LSTATUS status = RegQueryValueExW(key, valueName(), nullptr, &type,
                                             reinterpret_cast<BYTE *>(buffer), &size);
    RegCloseKey(key);
    return status == ERROR_SUCCESS && type == REG_SZ;
}

} // namespace bytes::windows
