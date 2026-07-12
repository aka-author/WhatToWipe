#include "app/Application.h"

#include "app/MainWindow.h"
#include "app/Product.h"
#include "config/ConfigStore.h"
#include "platform/AppVersion.h"
#include "ui/AppIcon.h"

#include <QApplication>

namespace wtw::app {

void WtwApplication::configure(QApplication& app) {
    QApplication::setApplicationName(productDisplayName());
    QApplication::setApplicationDisplayName(productDisplayName());
    QApplication::setOrganizationName(productDisplayName());
    QApplication::setApplicationVersion(platform::fileVersionDotted());
    app.setStyle(QStringLiteral("Fusion"));
    const QIcon windowIcon = ui::appWindowIcon();
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
