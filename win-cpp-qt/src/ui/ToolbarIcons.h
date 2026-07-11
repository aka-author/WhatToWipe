#pragma once

#include <QIcon>
#include <QSize>

class QToolButton;

namespace wtw::ui {

QSize toolbarIconSize();
QIcon toolbarOpenIcon();
QIcon toolbarUpIcon();
QIcon toolbarManageIcon();
QIcon toolbarUpdateIcon();
QIcon toolbarStopIcon();

void applyToolbarButtonStyle(QToolButton* button);

}  // namespace wtw::ui
