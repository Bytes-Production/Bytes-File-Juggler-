#include "CleanerPage.h"
#include "../cleaner/TempCleaner.h"
#include "../cleaner/RecycleBinManager.h"
#include "../cleaner/CleanerScheduler.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QMessageBox>
#include <QMetaObject>
#include <QRunnable>
#include <QCoreApplication>
#include <QTime>
#include <QFont>

namespace bytes::ui {

using bytes::cleaner::TempCleaner;
using bytes::cleaner::RecycleBinManager;
using bytes::cleaner::CleanerScheduler;
using bytes::cleaner::ScheduleFrequency;

CleanerPage::CleanerPage(database::Database &db, logging::ActivityLogger &activityLogger, QWidget *parent)
    : QWidget(parent), m_db(db), m_activityLogger(activityLogger) {

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 14, 16, 14);

    auto *title = new QLabel("Cleaner", this);
    QFont f = title->font();
    f.setPointSize(14);
    f.setBold(true);
    title->setFont(f);
    layout->addWidget(title);

    auto *sizesBox = new QGroupBox("Disk Space", this);
    auto *sizesLayout = new QVBoxLayout(sizesBox);

    auto makeRow = [&](QCheckBox *&check, QLabel *&sizeLabel, const QString &name) {
        auto *row = new QHBoxLayout();
        check = new QCheckBox(name, this);
        check->setChecked(true);
        sizeLabel = new QLabel("Calculating…", this);
        row->addWidget(check);
        row->addStretch();
        row->addWidget(sizeLabel);
        sizesLayout->addLayout(row);
    };

    makeRow(m_userTempCheck, m_userTempSizeLabel, "User Temp");
    makeRow(m_windowsTempCheck, m_windowsTempSizeLabel, "Windows Temp");
    makeRow(m_recycleBinCheck, m_recycleBinSizeLabel, "Recycle Bin");
    m_recycleBinCheck->setChecked(false); // off by default per the no-surprises rule

    auto *potentialRow = new QHBoxLayout();
    auto *potentialTitle = new QLabel("Potential Cleanup:", this);
    QFont bold = potentialTitle->font();
    bold.setBold(true);
    potentialTitle->setFont(bold);
    m_potentialLabel = new QLabel("0 MB", this);
    m_potentialLabel->setFont(bold);
    potentialRow->addWidget(potentialTitle);
    potentialRow->addWidget(m_potentialLabel);
    potentialRow->addStretch();
    sizesLayout->addLayout(potentialRow);

    for (auto *c : {m_userTempCheck, m_windowsTempCheck, m_recycleBinCheck})
        connect(c, &QCheckBox::toggled, this, &CleanerPage::updatePotentialLabel);

    layout->addWidget(sizesBox);

    auto *buttonRow = new QHBoxLayout();
    auto *cleanSelectedBtn = new QPushButton("Clean Selected", this);
    auto *cleanEverythingBtn = new QPushButton("Clean Everything", this);
    auto *refreshBtn = new QPushButton("Refresh", this);
    buttonRow->addWidget(cleanSelectedBtn);
    buttonRow->addWidget(cleanEverythingBtn);
    buttonRow->addWidget(refreshBtn);
    buttonRow->addStretch();
    layout->addLayout(buttonRow);

    connect(cleanSelectedBtn, &QPushButton::clicked, this, &CleanerPage::cleanSelected);
    connect(cleanEverythingBtn, &QPushButton::clicked, this, &CleanerPage::cleanEverything);
    connect(refreshBtn, &QPushButton::clicked, this, &CleanerPage::refreshSizes);

    auto *scheduleBox = new QGroupBox("Automatic Cleanup Schedule", this);
    auto *scheduleForm = new QFormLayout(scheduleBox);

    m_scheduleCombo = new QComboBox(this);
    m_scheduleCombo->addItems({"Off", "Hourly", "Daily", "Weekly", "Monthly"});
    m_scheduleTime = new QTimeEdit(QTime(2, 0), this);
    m_scheduleTime->setDisplayFormat("HH:mm");
    m_scheduleHours = new QSpinBox(this);
    m_scheduleHours->setRange(1, 23);
    m_scheduleHours->setSuffix(" hour(s)");
    m_scheduleHours->setValue(6);

    scheduleForm->addRow("Frequency:", m_scheduleCombo);
    m_scheduleTimeLabel = new QLabel("Time:", this);
    scheduleForm->addRow(m_scheduleTimeLabel, m_scheduleTime);
    m_scheduleHoursLabel = new QLabel("Every:", this);
    scheduleForm->addRow(m_scheduleHoursLabel, m_scheduleHours);
    layout->addWidget(scheduleBox);

    const QString freq = m_db.getSetting("cleaner_schedule", "daily");
    m_scheduleCombo->setCurrentText(freq.left(1).toUpper() + freq.mid(1));
    m_scheduleTime->setTime(QTime::fromString(m_db.getSetting("cleaner_schedule_time", "02:00"), "HH:mm"));
    m_scheduleHours->setValue(m_db.getSettingInt("cleaner_schedule_hours", 6));

    auto updateScheduleFieldVisibility = [this] {
        const bool hourly = m_scheduleCombo->currentText() == "Hourly";
        m_scheduleTime->setVisible(!hourly);
        m_scheduleTimeLabel->setVisible(!hourly);
        m_scheduleHours->setVisible(hourly);
        m_scheduleHoursLabel->setVisible(hourly);
    };
    updateScheduleFieldVisibility();

    connect(m_scheduleCombo, &QComboBox::currentTextChanged, this, [this, updateScheduleFieldVisibility] {
        updateScheduleFieldVisibility();
        onScheduleChanged();
    });
    connect(m_scheduleTime, &QTimeEdit::timeChanged, this, &CleanerPage::onScheduleChanged);
    connect(m_scheduleHours, &QSpinBox::valueChanged, this, &CleanerPage::onScheduleChanged);

    layout->addStretch();
    refreshSizes();
}

void CleanerPage::refreshSizes() {
    m_userTempSizeLabel->setText("Calculating…");
    m_windowsTempSizeLabel->setText("Calculating…");
    m_recycleBinSizeLabel->setText("Calculating…");

    QThreadPool *pool = &m_pool;
    pool->start(QRunnable::create([this] {
        const qint64 userTemp = TempCleaner::calculateSize(TempCleaner::userTempPath());
        const qint64 winTemp = TempCleaner::calculateSize(TempCleaner::windowsTempPath());
        const auto bin = RecycleBinManager::query();

        QMetaObject::invokeMethod(this, [this, userTemp, winTemp, bin] {
            m_userTempBytes = userTemp;
            m_windowsTempBytes = winTemp;
            m_recycleBinBytes = bin.totalBytes;

            auto fmt = [](qint64 bytes) { return QString::number(bytes / (1024.0 * 1024.0), 'f', 1) + " MB"; };
            m_userTempSizeLabel->setText(fmt(userTemp));
            m_windowsTempSizeLabel->setText(fmt(winTemp));
            m_recycleBinSizeLabel->setText(fmt(bin.totalBytes));
            updatePotentialLabel();
        }, Qt::QueuedConnection);
    }));
}

void CleanerPage::updatePotentialLabel() {
    qint64 total = 0;
    if (m_userTempCheck->isChecked()) total += m_userTempBytes;
    if (m_windowsTempCheck->isChecked()) total += m_windowsTempBytes;
    if (m_recycleBinCheck->isChecked()) total += m_recycleBinBytes;
    m_potentialLabel->setText(QString::number(total / (1024.0 * 1024.0 * 1024.0), 'f', 2) + " GB");
}

void CleanerPage::cleanSelected() {
    performCleanup(m_userTempCheck->isChecked(), m_windowsTempCheck->isChecked(), m_recycleBinCheck->isChecked());
}

void CleanerPage::cleanEverything() {
    performCleanup(true, true, true);
}

void CleanerPage::runCleanupNow() {
    performCleanup(m_db.getSettingBool("cleaner_user_temp_enabled", true),
                    m_db.getSettingBool("cleaner_windows_temp_enabled", true),
                    m_db.getSettingBool("cleaner_recycle_bin_enabled", false));
}

void CleanerPage::performCleanup(bool userTemp, bool windowsTemp, bool recycleBin) {
    if (recycleBin) {
        if (QMessageBox::question(this, "Empty Recycle Bin",
                "This will permanently empty the Recycle Bin. Continue?") != QMessageBox::Yes)
            recycleBin = false;
    }

    QThreadPool *pool = &m_pool;
    pool->start(QRunnable::create([this, userTemp, windowsTemp, recycleBin] {
        TempCleaner cleaner;
        if (userTemp) {
            const auto result = cleaner.clean(TempCleaner::userTempPath(), false);
            QMetaObject::invokeMethod(this, [this, result] {
                m_activityLogger.logCleanup("User Temp", result.bytesFreed, result.filesDeleted, result.filesSkipped, true);
                refreshSizes();
            }, Qt::QueuedConnection);
        }
        if (windowsTemp) {
            const auto result = cleaner.clean(TempCleaner::windowsTempPath(), false);
            QMetaObject::invokeMethod(this, [this, result] {
                m_activityLogger.logCleanup("Windows Temp", result.bytesFreed, result.filesDeleted, result.filesSkipped, true);
                refreshSizes();
            }, Qt::QueuedConnection);
        }
        if (recycleBin) {
            const auto before = RecycleBinManager::query();
            const bool ok = RecycleBinManager::empty();
            QMetaObject::invokeMethod(this, [this, before, ok] {
                m_activityLogger.logCleanup("Recycle Bin", ok ? before.totalBytes : 0, ok ? before.itemCount : 0, ok ? 0 : before.itemCount, ok);
                refreshSizes();
            }, Qt::QueuedConnection);
        }
    }));
}

void CleanerPage::onScheduleChanged() {
    const QString freq = m_scheduleCombo->currentText().toLower();
    const QString time = m_scheduleTime->time().toString("HH:mm");
    const int hours = m_scheduleHours->value();

    m_db.setSetting("cleaner_schedule", freq);
    m_db.setSetting("cleaner_schedule_time", time);
    m_db.setSetting("cleaner_schedule_hours", QString::number(hours));

    const ScheduleFrequency f = CleanerScheduler::frequencyFromString(freq);
    CleanerScheduler::apply(f, time, QCoreApplication::applicationFilePath(), hours);
}

} // namespace bytes::ui
