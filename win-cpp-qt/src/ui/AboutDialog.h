#pragma once

#include <QDialog>

namespace wtw::ui {

class AboutDialog : public QDialog {
    Q_OBJECT

public:
    explicit AboutDialog(QWidget* parent = nullptr);
};

}  // namespace wtw::ui
