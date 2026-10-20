#include "MainWindow.h"
#include "DashboardPage.h"
#include "JugglerPage.h"
#include "RulesPage.h"
#include "FoldersPage.h"
#include "CleanerPage.h"
#include "ActivityPage.h"
#include "SettingsPage.h"
#include "AboutPage.h"
#include "../core/UndoManager.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QCloseEvent>
#include <QMessageBox>
#include <QIcon>
#include <QApplication>
#include <QPalette>
#include <QStyleFactory>
#include <QList>
#include <QEvent>
#include <QMetaObject>

namespace bytes::ui {

namespace {

void applyBytesTheme(const QString &theme) {
    static QStyle *fusionStyle = QStyleFactory::create("Fusion"); // created once, ownership passed to Qt via setStyle

    if (theme == "dark") {
        if (fusionStyle)
            QApplication::setStyle(fusionStyle);
        QPalette palette;
        palette.setColor(QPalette::Window, QColor(30, 34, 42));
        palette.setColor(QPalette::WindowText, Qt::white);
        palette.setColor(QPalette::Base, QColor(24, 27, 34));
        palette.setColor(QPalette::AlternateBase, QColor(38, 43, 53));
        palette.setColor(QPalette::ToolTipBase, Qt::white);
        palette.setColor(QPalette::ToolTipText, Qt::white);
        palette.setColor(QPalette::Text, Qt::white);
        palette.setColor(QPalette::Button, QColor(42, 47, 58));
        palette.setColor(QPalette::ButtonText, Qt::white);
        palette.setColor(QPalette::BrightText, Qt::red);
        palette.setColor(QPalette::Highlight, QColor(47, 128, 237));
        palette.setColor(QPalette::HighlightedText, Qt::white);
        palette.setColor(QPalette::Disabled, QPalette::Text, QColor(120, 120, 120));
        palette.setColor(QPalette::Disabled, QPalette::WindowText, QColor(120, 120, 120));
        qApp->setPalette(palette);
    } else {
        // "light" and "system" both defer to the platform's own look; this
        // simply undoes any dark palette override.
        if (fusionStyle)
            QApplication::setStyle(fusionStyle);
        qApp->setPalette(QApplication::style()->standardPalette());
    }
}

} // namespace

MainWindow::MainWindow(database::Database &db,
                        logging::ActivityLogger &activityLogger,
                        logging::MarkdownLogger &markdownLogger,
                        core::FileOrganizer &organizer,
                        windows::FolderIconManager &folderIconManager,
                        QWidget *parent)
    : QMainWindow(parent), m_db(db), m_activityLogger(activityLogger), m_markdownLogger(markdownLogger),
      m_organizer(organizer), m_folderIconManager(folderIconManager), m_undoManager(db) {

    setWindowTitle("Bytes — File Juggler & Cleaner");
    setWindowIcon(QIcon(":/icons/bytes.ico"));
    resize(980, 620);
    setMinimumSize(820, 520);

    buildUi();
    applyStyle();
    applyBytesTheme(m_db.getSetting("appearance_theme", "system"));
}

void MainWindow::buildUi() {
    auto *central = new QWidget(this);
    auto *rootLayout = new QHBoxLayout(central);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    m_nav = new QListWidget(central);
    m_nav->setObjectName("sidebar");
    m_nav->setFixedWidth(150);
    m_nav->addItems({"Dashboard", "File Juggler", "Rules", "Folders", "Cleaner", "Activity", "Settings", "About"});
    m_nav->setCurrentRow(0);

    m_stack = new QStackedWidget(central);

    m_dashboardPage = new DashboardPage(m_db, m_activityLogger, m_organizer, m_stack);
    m_jugglerPage = new JugglerPage(m_db, m_organizer, m_stack);
    m_rulesPage = new RulesPage(m_db, m_organizer, m_activityLogger, m_stack);
    m_foldersPage = new FoldersPage(m_db, m_organizer, m_stack);
    m_cleanerPage = new CleanerPage(m_db, m_activityLogger, m_stack);
    m_activityPage = new ActivityPage(m_db, m_activityLogger, m_undoManager, m_stack);
    m_settingsPage = new SettingsPage(m_db, m_organizer, m_markdownLogger, m_stack);
    m_aboutPage = new AboutPage(m_stack);

    const QList<QWidget *> pages = {m_dashboardPage, m_jugglerPage, m_rulesPage, m_foldersPage,
                                     m_cleanerPage, m_activityPage, m_settingsPage, m_aboutPage};
    for (QWidget *page : pages)
        m_stack->addWidget(page);

    connect(m_nav, &QListWidget::currentRowChanged, m_stack, &QStackedWidget::setCurrentIndex);
    connect(m_nav, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row == 0) m_dashboardPage->refresh();
        if (row == 1) m_jugglerPage->refresh();
        if (row == 2) m_rulesPage->refresh();
        if (row == 3) m_foldersPage->refresh();
        if (row == 4) m_cleanerPage->refreshSizes();
        if (row == 5) m_activityPage->refresh();
    });

    connect(m_dashboardPage, &DashboardPage::runCleanupRequested, m_cleanerPage, &CleanerPage::runCleanupNow);
    connect(m_settingsPage, &SettingsPage::appearanceChanged, this, [](const QString &theme) {
        applyBytesTheme(theme);
    });

    rootLayout->addWidget(m_nav);
    rootLayout->addWidget(m_stack, 1);
    setCentralWidget(central);
}

void MainWindow::applyStyle() {
    // Applied at the QApplication level (not just this window) so every
    // dialog, wizard, and popup in Bytes shares the same red/black theme.
    qApp->setStyleSheet(R"(
        QWidget {
            font-size: 11px;
        }
        QMainWindow, QWidget#centralArea, QDialog, QWizard, QWizardPage, QMessageBox {
            background-color: #141414;
            color: #eaeaea;
        }
        QStackedWidget {
            background-color: #161616;
        }
        QStackedWidget, QStackedWidget > QWidget {
            color: #eaeaea;
        }
        QListWidget#sidebar {
            background-color: #0a0a0a;
            color: #cfcfcf;
            border: none;
            border-right: 1px solid #2a0d0d;
            font-size: 12px;
            padding-top: 10px;
        }
        QListWidget#sidebar::item {
            padding: 9px 16px;
        }
        QListWidget#sidebar::item:hover {
            background-color: #1c0e0e;
        }
        QListWidget#sidebar::item:selected {
            background-color: #d62828;
            color: #ffffff;
            font-weight: 600;
        }
        QWidget#statCard {
            background-color: #1c1c1c;
            border: 1px solid #3a1414;
            border-radius: 6px;
        }
        QLabel#statCardValue {
            color: #ff4d4d;
        }
        QGroupBox {
            font-weight: 600;
            color: #eaeaea;
            border: 1px solid #3a1414;
            border-radius: 6px;
            margin-top: 8px;
            padding-top: 12px;
            background-color: #181818;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 8px;
            padding: 0 4px;
            color: #ff4d4d;
        }
        QPushButton {
            padding: 4px 10px;
            border-radius: 4px;
            border: 1px solid #3a1414;
            background-color: #1e1e1e;
            color: #eaeaea;
        }
        QPushButton:hover {
            background-color: #d62828;
            border-color: #d62828;
            color: #ffffff;
        }
        QPushButton:pressed {
            background-color: #a91f1f;
        }
        QTableWidget, QListWidget, QTreeWidget {
            background-color: #1a1a1a;
            color: #eaeaea;
            gridline-color: #3a1414;
            border: 1px solid #3a1414;
            alternate-background-color: #202020;
        }
        QHeaderView::section {
            background-color: #0f0f0f;
            color: #ff4d4d;
            padding: 4px;
            border: none;
            border-bottom: 1px solid #3a1414;
        }
        QLineEdit, QComboBox, QSpinBox, QDateEdit, QTimeEdit, QTextEdit {
            background-color: #1e1e1e;
            color: #eaeaea;
            border: 1px solid #3a1414;
            border-radius: 4px;
            padding: 3px 6px;
        }
        QComboBox QAbstractItemView {
            background-color: #1e1e1e;
            color: #eaeaea;
            border: 1px solid #d62828;
            selection-background-color: #d62828;
            selection-color: #ffffff;
            outline: none;
        }
        QSpinBox::up-button, QSpinBox::down-button,
        QDateEdit::up-button, QDateEdit::down-button,
        QTimeEdit::up-button, QTimeEdit::down-button {
            background-color: #2a2a2a;
        }
        QCalendarWidget QWidget {
            background-color: #1e1e1e;
            color: #eaeaea;
        }
        QCalendarWidget QAbstractItemView:enabled {
            color: #eaeaea;
            background-color: #1e1e1e;
            selection-background-color: #d62828;
            selection-color: #ffffff;
        }
        QTabWidget::pane {
            border: 1px solid #3a1414;
            background-color: #181818;
        }
        QTabBar::tab {
            background-color: #101010;
            color: #cfcfcf;
            padding: 6px 12px;
        }
        QTabBar::tab:selected {
            background-color: #d62828;
            color: white;
        }
        QLabel {
            color: #eaeaea;
        }
        QCheckBox, QRadioButton {
            color: #eaeaea;
            spacing: 6px;
        }
        QCheckBox:disabled, QRadioButton:disabled {
            color: #777777;
        }
        QMenu {
            background-color: #1a1a1a;
            color: #eaeaea;
            border: 1px solid #3a1414;
        }
        QMenu::item:selected {
            background-color: #d62828;
            color: white;
        }
        QToolTip {
            background-color: #1a1a1a;
            color: #eaeaea;
            border: 1px solid #d62828;
        }
    )");
}

void MainWindow::navigateToActivity() {
    m_nav->setCurrentRow(5);
}

void MainWindow::navigateToSettings() {
    m_nav->setCurrentRow(6);
}

void MainWindow::navigateToDashboard() {
    m_nav->setCurrentRow(0);
}

void MainWindow::showAndRaise() {
    showNormal();
    raise();
    activateWindow();
}

void MainWindow::closeEvent(QCloseEvent *event) {
    if (m_db.getSettingBool("close_window_to_tray", true)) {
        hide();
        event->ignore();
    } else {
        event->accept();
        qApp->quit();
    }
}

void MainWindow::changeEvent(QEvent *event) {
    // Minimizing behaves like closing to tray: Bytes disappears from the
    // taskbar entirely and keeps running via the tray icon, rather than
    // sitting minimized in the taskbar.
    if (event->type() == QEvent::WindowStateChange && isMinimized()) {
        QMetaObject::invokeMethod(this, [this] { hide(); }, Qt::QueuedConnection);
        return;
    }
    QMainWindow::changeEvent(event);
}

} // namespace bytes::ui
