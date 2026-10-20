#include "AboutPage.h"

#include <QVBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QIcon>
#include <QFont>

namespace bytes::ui {

AboutPage::AboutPage(QWidget *parent) : QWidget(parent) {
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(28, 40, 28, 24);
    layout->setSpacing(10);
    layout->setAlignment(Qt::AlignTop | Qt::AlignHCenter);

    auto *iconLabel = new QLabel(this);
    iconLabel->setPixmap(QIcon(":/icons/bytes.png").pixmap(96, 96));
    iconLabel->setAlignment(Qt::AlignHCenter);
    layout->addWidget(iconLabel);

    auto *name = new QLabel("BYTES", this);
    QFont nameFont = name->font();
    nameFont.setPointSize(28);
    nameFont.setBold(true);
    name->setFont(nameFont);
    name->setAlignment(Qt::AlignHCenter);
    layout->addWidget(name);

    auto *subtitle = new QLabel("File Juggler & Cleaner", this);
    QFont subFont = subtitle->font();
    subFont.setPointSize(13);
    subtitle->setFont(subFont);
    subtitle->setAlignment(Qt::AlignHCenter);
    layout->addWidget(subtitle);

    auto *version = new QLabel("Version 1.0.0", this);
    version->setAlignment(Qt::AlignHCenter);
    layout->addWidget(version);

    auto *company = new QLabel("By Bytes Team", this);
    company->setAlignment(Qt::AlignHCenter);
    layout->addWidget(company);

    auto *tagline = new QLabel("Organize smarter. Clean safer.", this);
    QFont tagFont = tagline->font();
    tagFont.setItalic(true);
    tagline->setFont(tagFont);
    tagline->setAlignment(Qt::AlignHCenter);
    layout->addWidget(tagline);

    auto *disclaimer = new QLabel(
        "Bytes runs entirely on your PC. No cloud, no telemetry, no file uploads.", this);
    disclaimer->setAlignment(Qt::AlignHCenter);
    disclaimer->setStyleSheet("color: gray; margin-top: 24px;");
    layout->addWidget(disclaimer);
}

} // namespace bytes::ui
