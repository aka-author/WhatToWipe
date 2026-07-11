set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

if(NOT DEFINED QT_MINGW_ROOT)
    message(FATAL_ERROR "QT_MINGW_ROOT must be set (path to Qt's mingw*_64 toolchain).")
endif()

set(CMAKE_C_COMPILER "${QT_MINGW_ROOT}/bin/gcc.exe")
set(CMAKE_CXX_COMPILER "${QT_MINGW_ROOT}/bin/g++.exe")
set(CMAKE_RC_COMPILER "${QT_MINGW_ROOT}/bin/windres.exe")
