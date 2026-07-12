#include "app/Application.h"

#ifdef Q_OS_WIN
#include <shobjidl.h>
#endif

int main(int argc, char* argv[]) {
#ifdef Q_OS_WIN
    // Distinct AppUserModelID so the shell uses this process icon, not a generic bucket.
    SetCurrentProcessExplicitAppUserModelID(L"EraseAndRewrite.Application.1");
#endif
    return wtw::app::WtwApplication::run(argc, argv);
}
