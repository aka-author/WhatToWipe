#pragma once

#include <QWidget>

namespace wtw::ui {

void showError001(QWidget* parent);
void showError002(QWidget* parent);
void showError003(QWidget* parent);
void showError004(QWidget* parent, const QString& folderPath);
void showInterruptionAlert(QWidget* parent);

}  // namespace wtw::ui
