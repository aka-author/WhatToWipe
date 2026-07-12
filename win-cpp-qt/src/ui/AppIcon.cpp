#include "ui/AppIcon.h"

#include <QPixmap>
#include <QSize>

namespace wtw::ui {

QIcon appWindowIcon() {
    QIcon icon;
    static const struct {
        const char* resource;
        int size;
    } kLayers[] = {
        {":/app/icons/app-16.png", 16},
        {":/app/icons/app-20.png", 20},
        {":/app/icons/app-24.png", 24},
        {":/app/icons/app-32.png", 32},
        {":/app/icons/app-48.png", 48},
        {":/app/icons/app-64.png", 64},
        {":/app/icons/app-256.png", 256},
    };
    for (const auto& layer : kLayers) {
        const QPixmap pixmap(QString::fromLatin1(layer.resource));
        if (!pixmap.isNull()) {
            icon.addPixmap(pixmap);
        }
    }
    if (!icon.isNull()) {
        return icon;
    }
    return QIcon(QStringLiteral(":/app/app.ico"));
}

}  // namespace wtw::ui
