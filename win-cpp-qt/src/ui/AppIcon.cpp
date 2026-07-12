#include "ui/AppIcon.h"

#include <QIcon>
#include <QSize>

namespace wtw::ui {

QIcon appWindowIcon() {
    static const QIcon svgIcon(QStringLiteral(":/app/app.svg"));
    if (!svgIcon.isNull()) {
        return svgIcon;
    }

    QIcon icon;
    static const struct {
        const char* resource;
    } kLayers[] = {
        {":/app/icons/app-16.png"},
        {":/app/icons/app-20.png"},
        {":/app/icons/app-24.png"},
        {":/app/icons/app-32.png"},
        {":/app/icons/app-48.png"},
        {":/app/icons/app-64.png"},
        {":/app/icons/app-256.png"},
    };
    for (const auto& layer : kLayers) {
        icon.addFile(QString::fromLatin1(layer.resource), QSize(), QIcon::Normal, QIcon::Off);
    }
    if (!icon.isNull()) {
        return icon;
    }
    return QIcon(QStringLiteral(":/app/app.ico"));
}

}  // namespace wtw::ui
