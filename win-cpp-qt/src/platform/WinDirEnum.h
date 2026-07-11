#pragma once

#include "scan/ScanTypes.h"

#include <functional>

namespace wtw::platform {

using CancelPredicate = std::function<bool()>;

scan::DirectoryReadResult enumerateDirectory(const QString& path, CancelPredicate isCancelled);

#ifdef WTW_UNIT_TEST
int testOpenFindHandles();
void testResetFindHandleState();
#endif

}  // namespace wtw::platform
