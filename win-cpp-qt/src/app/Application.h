#pragma once

#include <QApplication>

namespace wtw::app {

class WtwApplication {
public:
    static int run(int argc, char* argv[]);

private:
    static void configure(QApplication& app);
};

}  // namespace wtw::app
