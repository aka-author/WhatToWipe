set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

if(NOT QT_MINGW_ROOT)
    if(DEFINED ENV{QT_MINGW_ROOT} AND NOT "$ENV{QT_MINGW_ROOT}" STREQUAL "")
        set(QT_MINGW_ROOT "$ENV{QT_MINGW_ROOT}")
    endif()
endif()

if(NOT QT_MINGW_ROOT)
    message(FATAL_ERROR "QT_MINGW_ROOT must be set (path to Qt's mingw*_64 toolchain).")
endif()

file(TO_CMAKE_PATH "${QT_MINGW_ROOT}/bin/gcc.exe" _wtw_gcc)
file(TO_CMAKE_PATH "${QT_MINGW_ROOT}/bin/g++.exe" _wtw_gxx)
file(TO_CMAKE_PATH "${QT_MINGW_ROOT}/bin/windres.exe" _wtw_windres)

set(CMAKE_C_COMPILER "${_wtw_gcc}" CACHE FILEPATH "Qt MinGW C compiler" FORCE)
set(CMAKE_CXX_COMPILER "${_wtw_gxx}" CACHE FILEPATH "Qt MinGW C++ compiler" FORCE)
set(CMAKE_RC_COMPILER "${_wtw_windres}" CACHE FILEPATH "Qt MinGW resource compiler" FORCE)
