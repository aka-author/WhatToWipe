#pragma once

#include "scan/ScanTypes.h"

#include <functional>
#include <windows.h>

namespace wtw::platform {

using CancelPredicate = std::function<bool()>;

scan::DirectoryReadResult enumerateDirectory(const QString& path, CancelPredicate isCancelled);

#ifdef WTW_UNIT_TEST
using FindNextHook = std::function<BOOL(HANDLE, WIN32_FIND_DATAW*)>;

int testOpenFindHandles();
void testResetFindHandleState();
void testSetFindNextHook(FindNextHook hook);
void testClearFindNextHook();
#endif

}  // namespace wtw::platform
