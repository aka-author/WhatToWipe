#include "app/Application.h"

#include <QCoreApplication>

int main(int argc, char* argv[]) {
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    return wtw::app::WtwApplication::run(argc, argv);
}
