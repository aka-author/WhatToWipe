#include "ui/AppIcon.h"

#include <QIcon>
#include <QSize>

namespace wtw::ui {

QIcon appWindowIcon() {
    QIcon icon;
    static const struct {
        const char* resource;
        int logicalSize;
    } kLayers[] = {
        {":/app/icons/app-16.png", 16},
        {":/app/icons/app-20.png", 20},
        {":/app/icons/app-24.png", 24},
        {":/app/icons/app-32.png", 32},
        {":/app/icons/app-40.png", 40},
        {":/app/icons/app-48.png", 48},
        {":/app/icons/app-64.png", 64},
        {":/app/icons/app-256.png", 256},
    };
    for (const auto& layer : kLayers) {
        icon.addFile(QString::fromLatin1(layer.resource),
                     QSize(layer.logicalSize, layer.logicalSize),
                     QIcon::Normal,
                     QIcon::Off);
    }
    if (!icon.isNull()) {
        return icon;
    }
    return QIcon(QStringLiteral(":/app/app.ico"));
}

}  // namespace wtw::ui
