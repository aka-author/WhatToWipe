#pragma once

#include <QIcon>
#include <QSize>

namespace wtw::ui {

QSize toolbarIconSize();
QIcon toolbarOpenIcon();
QIcon toolbarUpIcon();
QIcon toolbarManageIcon();
QIcon toolbarUpdateIcon();
QIcon toolbarStopIcon();

void applyToolbarButtonStyle(class QToolButton* button);

}  // namespace wtw::ui
