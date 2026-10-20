#pragma once

#include <QWidget>

namespace bytes::ui {

/// About page: branding, version, and credits.
class AboutPage : public QWidget {
    Q_OBJECT
public:
    explicit AboutPage(QWidget *parent = nullptr);
};

} // namespace bytes::ui
