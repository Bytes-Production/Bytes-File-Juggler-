#include "DashboardPage.h"
#include "../windows/ShellUtils.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QDateTime>
#include <QDate>
#include <QFileInfo>
#include <QFont>
#include <QMessageBox>

namespace bytes::ui {

DashboardPage::DashboardPage(database::Database &db, logging::ActivityLogger &activityLogger,
                              core::FileOrganizer &organizer, QWidget *parent)
    : QWidget(parent), m_db(db), m_activityLogger(activityLogger), m_organizer(organizer) {

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 14, 16, 14);
    mainLayout->setSpacing(16);

    m_greetingLabel = new QLabel(this);
    m_greetingLabel->setObjectName("greetingLabel");
    QFont greetingFont = m_greetingLabel->font();
    greetingFont.setPointSize(20);
    greetingFont.setBold(true);
    m_greetingLabel->setFont(greetingFont);
    mainLayout->addWidget(m_greetingLabel);

    auto *subtitle = new QLabel("Bytes is protecting your files", this);
    subtitle->setObjectName("subtitleLabel");
    mainLayout->addWidget(subtitle);

    auto *cardsLayout = new QHBoxLayout();
    cardsLayout->setSpacing(12);
    cardsLayout->addWidget(buildStatCard("Files Today", m_todayCard));
    cardsLayout->addWidget(buildStatCard("Files This Week", m_weekCard));
    cardsLayout->addWidget(buildStatCard("Files Total", m_totalCard));
    cardsLayout->addWidget(buildStatCard("Folders Created", m_foldersCard));
    mainLayout->addLayout(cardsLayout);

    auto *cardsLayout2 = new QHBoxLayout();
    cardsLayout2->setSpacing(12);
    cardsLayout2->addWidget(buildStatCard("Files Skipped", m_skippedCard));
    cardsLayout2->addWidget(buildStatCard("Errors", m_errorsCard));
    cardsLayout2->addWidget(buildStatCard("Space Cleaned", m_cleanedCard));
    cardsLayout2->addWidget(buildStatCard("Last Activity", m_lastActivityCard));
    mainLayout->addLayout(cardsLayout2);

    auto *quickActionsBox = new QGroupBox("Quick Actions", this);
    auto *quickActionsLayout = new QHBoxLayout(quickActionsBox);

    auto *openDownloadsBtn = new QPushButton("Open Downloads", this);
    connect(openDownloadsBtn, &QPushButton::clicked, this, [] {
        windows::ShellUtils::openInExplorer(windows::ShellUtils::knownDownloadsFolder());
    });

    auto *runOrganizerBtn = new QPushButton("Run Organizer", this);
    connect(runOrganizerBtn, &QPushButton::clicked, this, [this] {
        // Summarize what would happen per folder first -- "Run Organizer"
        // touches every existing file already sitting in every monitored
        // folder, which is exactly the kind of surprise the "first run
        // must be safe" rule exists to prevent. Never do this silently.
        struct Pending { database::MonitoredFolder folder; core::ExistingFilesSummary summary; };
        QVector<Pending> pending;
        for (const auto &folder : m_db.monitoredFolders()) {
            if (!folder.enabled)
                continue;
            const auto summary = m_organizer.summarizeExistingFiles(folder.path);
            if (summary.totalFiles > 0)
                pending.push_back({folder, summary});
        }

        if (pending.isEmpty()) {
            QMessageBox::information(this, "Run Organizer", "No existing files need organizing right now.");
            return;
        }

        QString details;
        int totalFiles = 0;
        for (const auto &p : pending) {
            details += QString("\n%1 — %2 file(s)").arg(p.folder.path).arg(p.summary.totalFiles);
            totalFiles += p.summary.totalFiles;
        }

        if (QMessageBox::question(this, "Run Organizer",
                QString("This will organize %1 existing file(s):\n%2\n\nContinue?").arg(totalFiles).arg(details))
            != QMessageBox::Yes)
            return;

        for (const auto &p : pending)
            m_organizer.organizeExistingFiles(p.folder.id);
    });

    auto *runCleanupBtn = new QPushButton("Run Cleanup", this);
    connect(runCleanupBtn, &QPushButton::clicked, this, &DashboardPage::runCleanupRequested);

    m_pauseResumeButton = new QPushButton(this);
    connect(m_pauseResumeButton, &QPushButton::clicked, this, [this] {
        m_organizer.setPaused(!m_organizer.isPaused());
        updatePauseButton();
    });

    quickActionsLayout->addWidget(openDownloadsBtn);
    quickActionsLayout->addWidget(runOrganizerBtn);
    quickActionsLayout->addWidget(runCleanupBtn);
    quickActionsLayout->addWidget(m_pauseResumeButton);
    mainLayout->addWidget(quickActionsBox);

    auto *activityBox = new QGroupBox("Recent Activity", this);
    auto *activityLayout = new QVBoxLayout(activityBox);
    m_recentList = new QListWidget(this);
    m_recentList->setAlternatingRowColors(true);
    activityLayout->addWidget(m_recentList);
    mainLayout->addWidget(activityBox, 1);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setObjectName("monitoringStatusLabel");
    mainLayout->addWidget(m_statusLabel);

    m_refreshCoalesceTimer.setSingleShot(true);
    m_refreshCoalesceTimer.setInterval(500);
    connect(&m_refreshCoalesceTimer, &QTimer::timeout, this, &DashboardPage::refresh);

    connect(&m_activityLogger, &logging::ActivityLogger::operationLogged, this, [this](const database::FileOperationRecord &) {
        // Bytes can process dozens of files per second during a big sort;
        // rebuilding the whole dashboard for every single one made the app
        // feel sluggish. Coalesce into at most one refresh per burst.
        if (!m_refreshCoalesceTimer.isActive())
            m_refreshCoalesceTimer.start();
    });
    connect(&m_organizer, &core::FileOrganizer::monitoringStateChanged, this, [this](bool) {
        updatePauseButton();
    });

    const int hour = QDateTime::currentDateTime().time().hour();
    const QString greeting = hour < 12 ? "Good morning" : (hour < 18 ? "Good afternoon" : "Good evening");
    m_greetingLabel->setText(greeting);

    updatePauseButton();
    refresh();
}

QWidget *DashboardPage::buildStatCard(const QString &title, QLabel *&valueLabelOut) {
    auto *card = new QWidget(this);
    card->setObjectName("statCard");
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(16, 14, 16, 14);

    auto *value = new QLabel("0", card);
    value->setObjectName("statCardValue");
    QFont f = value->font();
    f.setPointSize(14);
    f.setBold(true);
    value->setFont(f);

    auto *label = new QLabel(title, card);
    label->setObjectName("statCardLabel");

    layout->addWidget(value);
    layout->addWidget(label);
    valueLabelOut = value;
    return card;
}

void DashboardPage::updatePauseButton() {
    const bool paused = m_organizer.isPaused();
    m_pauseResumeButton->setText(paused ? "Resume Monitoring" : "Pause Monitoring");
    m_statusLabel->setText(paused ? "○ Monitoring Paused" : "● Monitoring Active");
}

void DashboardPage::refresh() {
    const QString today = QDate::currentDate().toString("yyyy-MM-dd");
    const auto stats = m_db.statisticsForDate(today);

    m_todayCard->setText(QString::number(stats.filesOrganized));
    m_weekCard->setText(QString::number(m_db.filesOrganizedSince(QDateTime::currentDateTime().addDays(-7))));
    m_totalCard->setText(QString::number(m_db.totalFilesOrganized()));
    m_foldersCard->setText(QString::number(stats.foldersCreated));
    m_skippedCard->setText(QString::number(stats.filesSkipped));
    m_errorsCard->setText(QString::number(stats.errors));

    qint64 totalCleaned = 0;
    for (const auto &c : m_db.recentCleanupOperations(1000))
        totalCleaned += c.bytesFreed;
    m_cleanedCard->setText(QString::number(totalCleaned / (1024.0 * 1024.0), 'f', 1) + " MB");

    const auto recent = m_db.recentFileOperations(25);
    m_recentList->clear();
    for (const auto &op : recent) {
        if (op.status != "moved" && op.status != "error" && op.status != "skipped")
            continue;
        QString icon = op.status == "moved" ? "✓" : (op.status == "error" ? "✗" : "⏭");
        const QString text = QString("%1 %2 → %3").arg(icon, QFileInfo(op.sourcePath).fileName(),
                                                          op.status == "moved" ? op.fileType : op.status);
        m_recentList->addItem(text);
    }

    if (!recent.isEmpty())
        m_lastActivityCard->setText(recent.first().timestamp.toString("HH:mm"));
    else
        m_lastActivityCard->setText("—");
}

} // namespace bytes::ui
