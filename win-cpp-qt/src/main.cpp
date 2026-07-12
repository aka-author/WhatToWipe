#include "app/Application.h"
#include "platform/WinShell.h"

#ifdef Q_OS_WIN
#include <shobjidl.h>
#endif

int main(int argc, char* argv[]) {
#ifdef Q_OS_WIN
    SetCurrentProcessExplicitAppUserModelID(wtw::platform::kAppUserModelId);
#endif
    return wtw::app::WtwApplication::run(argc, argv);
}
