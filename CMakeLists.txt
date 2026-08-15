cmake_minimum_required(VERSION 3.16)

project(Arbitrium VERSION 0.9.2 LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Svg Network LinguistTools)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Svg Network LinguistTools)

set(TS_FILES Arbitrium_tr_TR.ts)

set(PROJECT_SOURCES
    src/main.cpp

    src/theme.h                       src/theme.cpp
    src/css.h                         src/css.cpp
    src/icons.h                       src/icons.cpp
    src/catalog.h                     src/catalog.cpp
    src/appstate.h                    src/appstate.cpp
    src/sysinfo.h                     src/sysinfo.cpp
    src/monitor.h                     src/monitor.cpp
    src/registry.h                    src/registry.cpp
    src/preset.h                      src/preset.cpp
    src/settings.h                    src/settings.cpp
    src/updater.h                     src/updater.cpp
    src/tweakengine.h                 src/tweakengine.cpp
    src/framelesswindow.h             src/framelesswindow.cpp
    src/mainwindow.h                  src/mainwindow.cpp

    src/widgets/accentpicker.h        src/widgets/accentpicker.cpp
    src/widgets/buttons.h             src/widgets/buttons.cpp
    src/widgets/categoryrow.h         src/widgets/categoryrow.cpp
    src/widgets/livechart.h           src/widgets/livechart.cpp
    src/widgets/overviewblocks.h      src/widgets/overviewblocks.cpp
    src/widgets/searchfield.h         src/widgets/searchfield.cpp
    src/widgets/sectionheader.h       src/widgets/sectionheader.cpp
    src/widgets/settingrow.h          src/widgets/settingrow.cpp
    src/widgets/segmentedcontrol.h    src/widgets/segmentedcontrol.cpp
    src/widgets/smoothscrollarea.h    src/widgets/smoothscrollarea.cpp
    src/widgets/toggleswitch.h        src/widgets/toggleswitch.cpp
    src/widgets/tweakrow.h            src/widgets/tweakrow.cpp

    src/views/contentheader.h         src/views/contentheader.cpp
    src/views/overviewpage.h          src/views/overviewpage.cpp
    src/views/settingspage.h          src/views/settingspage.cpp
    src/views/sidebar.h               src/views/sidebar.cpp
    src/views/statusbar.h             src/views/statusbar.cpp
    src/views/titlebar.h              src/views/titlebar.cpp
    src/views/tweakpage.h             src/views/tweakpage.cpp

    resources/resources.qrc
    ${TS_FILES}
)

# app.rc carries the executable icon and version block.
if(WIN32)
    list(APPEND PROJECT_SOURCES app.rc)
endif()

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(Arbitrium
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
    qt_create_translation(QM_FILES ${CMAKE_SOURCE_DIR}/src ${TS_FILES})
else()
    add_executable(Arbitrium ${PROJECT_SOURCES})
    qt5_create_translation(QM_FILES ${CMAKE_SOURCE_DIR}/src ${TS_FILES})
endif()

target_include_directories(Arbitrium PRIVATE src)

target_link_libraries(Arbitrium PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Svg
    Qt${QT_VERSION_MAJOR}::Network
)

# Win32 bits used by src/sysinfo.cpp and src/monitor.cpp. TBS and netapi32 are loaded
# dynamically at runtime; these are the ones that are linked.
if(WIN32)
    target_link_libraries(Arbitrium PRIVATE advapi32 user32 kernel32 iphlpapi)
endif()

# The UI is Turkish; keep the source and execution charsets on UTF-8 so ı/İ/ş/ğ survive.
if(MSVC)
    target_compile_options(Arbitrium PRIVATE /utf-8)
else()
    target_compile_options(Arbitrium PRIVATE -finput-charset=UTF-8 -fexec-charset=UTF-8)
endif()

# Copy the Qt runtime next to the executable after every link.
#
# Without this the build only runs from a shell that already has Qt's bin directory on
# PATH; launched from Explorer or from Qt Creator's deploy step it dies with
# "Qt6Svg.dll not found" (or whichever import the loader happens to resolve first).
# windeployqt brings the Qt DLLs, the platform/imageformats/styles plugins and — because
# this is a MinGW build — libgcc / libstdc++ / libwinpthread as well.
if(WIN32 AND QT_VERSION_MAJOR EQUAL 6)
    get_target_property(_qt_qmake Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(_qt_bin_dir "${_qt_qmake}" DIRECTORY)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${_qt_bin_dir}")

    if(WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET Arbitrium POST_BUILD
            COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                    --no-translations
                    --no-system-d3d-compiler
                    --no-opengl-sw
                    --no-quick-import
                    "$<TARGET_FILE:Arbitrium>"
            COMMENT "Deploying the Qt runtime next to Arbitrium.exe"
            VERBATIM)
    else()
        message(WARNING "windeployqt not found — Arbitrium.exe will only run with Qt's bin directory on PATH")
    endif()
endif()

set_target_properties(Arbitrium PROPERTIES
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS Arbitrium
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(Arbitrium)
endif()
