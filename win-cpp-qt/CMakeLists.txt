cmake_minimum_required(VERSION 3.21)
project(EraseAndRewrite VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets Svg Test)

set(WTW_SOURCES
    src/main.cpp
    src/app/Application.cpp
    src/app/MainWindow.cpp
    src/app/Session.cpp
    src/model/FolderDescriptor.cpp
    src/config/TreemapConfig.cpp
    src/config/ConfigStore.cpp
    src/scan/ScanTypes.cpp
    src/scan/ScanResult.cpp
    src/scan/ScanWorker.cpp
    src/scan/ArchiveClassifier.cpp
    src/scan/SubtreeMerge.cpp
    src/treemap/TreemapProjection.cpp
    src/treemap/TreemapLayout.cpp
    src/treemap/TreemapWidget.cpp
    src/treemap/LabelFit.cpp
    src/platform/AppVersion.cpp
    src/platform/VolumeInfo.cpp
    src/platform/ShellOpen.cpp
    src/platform/WinDirEnum.cpp
    src/ui/SettingsSchema.cpp
    src/ui/SettingsDialog.cpp
    src/ui/AboutDialog.cpp
    src/ui/AlertDialogs.cpp
    src/ui/ToolbarIcons.cpp
    src/util/Format.cpp
)

qt_add_resources(WTW_RESOURCES resources/toolbar.qrc resources/app.qrc)
list(APPEND WTW_SOURCES ${WTW_RESOURCES})

if(WIN32)
    list(APPEND WTW_SOURCES resources/app.rc)
endif()

qt_add_executable(EraseAndRewrite ${WTW_SOURCES})

target_include_directories(EraseAndRewrite PRIVATE src)

target_link_libraries(EraseAndRewrite PRIVATE Qt6::Widgets Qt6::Svg)

if(WIN32)
    target_link_libraries(EraseAndRewrite PRIVATE shell32 ole32 version)
endif()

if(MINGW)
    target_link_options(EraseAndRewrite PRIVATE
        -static-libgcc
        -static-libstdc++
        -Wl,-Bstatic
        -lwinpthread
        -Wl,-Bdynamic
    )
endif()

install(TARGETS EraseAndRewrite RUNTIME DESTINATION .)

enable_testing()

set(WTW_TEST_SOURCES
    tests/test_phase1.cpp
    src/app/Session.cpp
    src/model/FolderDescriptor.cpp
    src/config/TreemapConfig.cpp
    src/scan/ScanTypes.cpp
    src/scan/ScanResult.cpp
    src/scan/ScanWorker.cpp
    src/scan/ArchiveClassifier.cpp
    src/scan/SubtreeMerge.cpp
    src/treemap/TreemapProjection.cpp
    src/platform/WinDirEnum.cpp
    src/util/Format.cpp
)

qt_add_executable(phase1_tests ${WTW_TEST_SOURCES})
target_include_directories(phase1_tests PRIVATE src)
target_compile_definitions(phase1_tests PRIVATE WTW_UNIT_TEST)
target_link_libraries(phase1_tests PRIVATE Qt6::Test Qt6::Core Qt6::Gui)
if(WIN32)
    target_link_libraries(phase1_tests PRIVATE shell32 ole32 version)
endif()

add_test(NAME phase1_tests COMMAND phase1_tests)
