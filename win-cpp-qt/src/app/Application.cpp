#include "app/Application.h"

#include "app/MainWindow.h"
#include "config/ConfigStore.h"

#include <QApplication>
#include <QIcon>

namespace wtw::app {

void WtwApplication::configure(QApplication& app) {
    QApplication::setApplicationName(QStringLiteral("WhatToWipe"));
    QApplication::setApplicationDisplayName(QStringLiteral("WhatToWipe"));
    QApplication::setOrganizationName(QStringLiteral("WhatToWipe"));
    QApplication::setApplicationVersion(QStringLiteral("1.0.0.0000"));
    app.setStyle(QStringLiteral("Fusion"));
    const QIcon windowIcon(QStringLiteral(":/app/app.ico"));
    if (!windowIcon.isNull()) {
        app.setWindowIcon(windowIcon);
    }
}

int WtwApplication::run(int argc, char* argv[]) {
    QApplication app(argc, argv);
    configure(app);

    config::TreemapSettings settings;
    if (!config::loadOrInitTreemap(settings)) {
        settings = config::defaultTreemapSettings();
    }

    MainWindow window(settings);
    window.show();
    return app.exec();
}

}  // namespace wtw::app
