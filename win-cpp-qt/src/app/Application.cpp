#include "app/Application.h"

#include "app/MainWindow.h"
#include "app/Product.h"
#include "config/ConfigStore.h"
#include "platform/AppVersion.h"

#include <QApplication>
#include <QIcon>

namespace wtw::app {

void WtwApplication::configure(QApplication& app) {
    QApplication::setApplicationName(productDisplayName());
    QApplication::setApplicationDisplayName(productDisplayName());
    QApplication::setOrganizationName(productDisplayName());
    QApplication::setApplicationVersion(platform::fileVersionDotted());
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
