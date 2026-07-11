cmake_minimum_required(VERSION 3.21)
project(WhatToWipe VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets Svg)

set(WTW_SOURCES
    src/main.cpp
    src/app/Application.cpp
    src/app/MainWindow.cpp
    src/app/Session.cpp
    src/model/FolderDescriptor.cpp
    src/config/TreemapConfig.cpp
    src/config/ConfigStore.cpp
    src/scan/ScanWorker.cpp
    src/scan/ArchiveClassifier.cpp
    src/scan/SubtreeMerge.cpp
    src/treemap/TreemapProjection.cpp
    src/treemap/TreemapLayout.cpp
    src/treemap/TreemapWidget.cpp
    src/treemap/LabelFit.cpp
    src/platform/VolumeInfo.cpp
    src/platform/ShellOpen.cpp
    src/ui/SettingsSchema.cpp
    src/ui/SettingsDialog.cpp
    src/ui/AboutDialog.cpp
    src/ui/AlertDialogs.cpp
    src/ui/ToolbarIcons.cpp
    src/util/Format.cpp
)

qt_add_resources(WTW_RESOURCES resources/toolbar.qrc)
list(APPEND WTW_SOURCES ${WTW_RESOURCES})

if(WIN32)
    list(APPEND WTW_SOURCES resources/app.rc)
endif()

qt_add_executable(WhatToWipe ${WTW_SOURCES})

target_include_directories(WhatToWipe PRIVATE src)

target_link_libraries(WhatToWipe PRIVATE Qt6::Widgets Qt6::Svg)

if(WIN32)
    target_link_libraries(WhatToWipe PRIVATE shell32 ole32)
endif()

# App links C++ runtime statically so deployed Qt libstdc++-6.dll only serves Qt6*.dll.
if(MINGW)
    target_link_options(WhatToWipe PRIVATE
        -static-libgcc
        -static-libstdc++
        -Wl,-Bstatic
        -lwinpthread
        -Wl,-Bdynamic
    )
endif()

install(TARGETS WhatToWipe RUNTIME DESTINATION .)
