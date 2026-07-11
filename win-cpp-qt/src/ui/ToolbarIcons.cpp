#include "ui/ToolbarIcons.h"

#include <QToolButton>

namespace wtw::ui {

namespace {

constexpr int kToolbarIconPx = 24;
constexpr int kToolbarButtonPx = 32;

QIcon loadIcon(const char* resourceName) {
    QIcon icon(QStringLiteral(":/toolbar/%1").arg(QLatin1String(resourceName)));
    return icon;
}

}  // namespace

QSize toolbarIconSize() { return QSize(kToolbarIconPx, kToolbarIconPx); }

QIcon toolbarOpenIcon() { return loadIcon("toolbar-open.svg"); }
QIcon toolbarUpIcon() { return loadIcon("toolbar-up.svg"); }
QIcon toolbarManageIcon() { return loadIcon("toolbar-manage.svg"); }
QIcon toolbarUpdateIcon() { return loadIcon("toolbar-update.svg"); }
QIcon toolbarStopIcon() { return loadIcon("toolbar-stop.svg"); }

void applyToolbarButtonStyle(QToolButton* button) {
    if (!button) {
        return;
    }
    button->setFixedSize(kToolbarButtonPx, kToolbarButtonPx);
    button->setIconSize(toolbarIconSize());
    button->setToolButtonStyle(Qt::ToolButtonIconOnly);
    button->setAutoRaise(false);
    button->setFocusPolicy(Qt::NoFocus);
}

}  // namespace wtw::ui
