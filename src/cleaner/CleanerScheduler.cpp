#include "CleanerScheduler.h"

#include <QProcess>
#include <QDir>
#include <QtGlobal>

namespace bytes::cleaner {

QString CleanerScheduler::taskName() { return "BytesCleanupTask"; }

QString CleanerScheduler::frequencyToString(ScheduleFrequency f) {
    switch (f) {
    case ScheduleFrequency::Hourly: return "hourly";
    case ScheduleFrequency::Daily: return "daily";
    case ScheduleFrequency::Weekly: return "weekly";
    case ScheduleFrequency::Monthly: return "monthly";
    case ScheduleFrequency::Off: default: return "off";
    }
}

ScheduleFrequency CleanerScheduler::frequencyFromString(const QString &s) {
    const QString v = s.trimmed().toLower();
    if (v == "hourly") return ScheduleFrequency::Hourly;
    if (v == "daily") return ScheduleFrequency::Daily;
    if (v == "weekly") return ScheduleFrequency::Weekly;
    if (v == "monthly") return ScheduleFrequency::Monthly;
    return ScheduleFrequency::Off;
}

bool CleanerScheduler::apply(ScheduleFrequency frequency, const QString &timeHHmm, const QString &executablePath,
                              int everyNHours) {
    if (frequency == ScheduleFrequency::Off)
        return remove();

    const QString command = QString("\"%1\" --cleanup-scheduled").arg(QDir::toNativeSeparators(executablePath));

    QStringList args = {
        "/Create", "/F",
        "/TN", taskName(),
        "/TR", command,
        "/RL", "LIMITED",
    };

    if (frequency == ScheduleFrequency::Hourly) {
        everyNHours = qBound(1, everyNHours, 23);
        // /SC HOURLY /MO <N> repeats every N hours, starting from /ST
        // today; schtasks needs SOME start time, so "now-ish" (00:00) plus
        // the recurrence handles the rest.
        args += {"/SC", "HOURLY", "/MO", QString::number(everyNHours), "/ST", "00:00"};
    } else {
        QString sc;
        switch (frequency) {
        case ScheduleFrequency::Daily: sc = "DAILY"; break;
        case ScheduleFrequency::Weekly: sc = "WEEKLY"; break;
        case ScheduleFrequency::Monthly: sc = "MONTHLY"; break;
        default: return remove();
        }
        args += {"/SC", sc, "/ST", timeHHmm};
    }

    QProcess proc;
    proc.start("schtasks.exe", args);
    proc.waitForFinished(10000);
    return proc.exitCode() == 0;
}

bool CleanerScheduler::remove() {
    QProcess proc;
    proc.start("schtasks.exe", {"/Delete", "/TN", taskName(), "/F"});
    proc.waitForFinished(10000);
    // A task that doesn't exist yet is not a failure from our point of view.
    return proc.exitCode() == 0 || proc.exitCode() == 1;
}

bool CleanerScheduler::isScheduled() {
    QProcess proc;
    proc.start("schtasks.exe", {"/Query", "/TN", taskName()});
    proc.waitForFinished(10000);
    return proc.exitCode() == 0;
}

} // namespace bytes::cleaner
