#pragma once

#include <QString>

namespace bytes::cleaner {

enum class ScheduleFrequency { Off, Hourly, Daily, Weekly, Monthly };

/// Registers Bytes's automatic cleanup with the Windows Task Scheduler
/// (via `schtasks.exe`, which needs no elevation for a per-user, limited
/// task). The scheduled task simply relaunches Bytes with
/// `--cleanup-scheduled`, which performs the configured cleanup and exits
/// (or, if Bytes is already running, signals the running instance).
class CleanerScheduler {
public:
    static QString taskName();

    /// Creates/updates or removes (Frequency::Off) the scheduled task.
    /// `timeHHmm` is the daily/weekly/monthly start time and is ignored for
    /// Hourly; `everyNHours` (1-23) is used only for Hourly.
    static bool apply(ScheduleFrequency frequency, const QString &timeHHmm, const QString &executablePath,
                       int everyNHours = 6);
    static bool remove();
    static bool isScheduled();

    static QString frequencyToString(ScheduleFrequency f);
    static ScheduleFrequency frequencyFromString(const QString &s);
};

} // namespace bytes::cleaner
