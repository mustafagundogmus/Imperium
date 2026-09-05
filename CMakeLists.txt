cmake_minimum_required(VERSION 3.16)

project(Arbitrium VERSION 0.15.2 LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Svg Network)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Svg Network)

set(PROJECT_SOURCES
    src/main.cpp

    src/theme.h                       src/theme.cpp
    src/i18n.h                         src/i18n.cpp
    src/css.h                         src/css.cpp
    src/console.h                     src/console.cpp
    src/winpaths.h                    src/winpaths.cpp
    src/progress.h
    src/icons.h                       src/icons.cpp
    src/catalog.h                     src/catalog.cpp
    src/action.h                      src/action.cpp
    src/settingslinks.h               src/settingslinks.cpp
    src/actionengine.h                src/actionengine.cpp
    src/appstate.h                    src/appstate.cpp
    src/sysinfo.h                     src/sysinfo.cpp
    src/deepinfo.h                    src/deepinfo.cpp
    src/services.h                    src/services.cpp
    src/startup.h                     src/startup.cpp
    src/tasks.h                       src/tasks.cpp
    src/cleaner.h                     src/cleaner.cpp
    src/apps.h                        src/apps.cpp
    src/features.h                    src/features.cpp
    src/monitor.h                     src/monitor.cpp
    src/registry.h                    src/registry.cpp
    src/migration.h                   src/migration.cpp
    src/ownership.h                   src/ownership.cpp
    src/trustedinstaller.h            src/trustedinstaller.cpp
    src/debloat.h                     src/debloat.cpp
    src/preset.h                      src/preset.cpp
    src/settings.h                    src/settings.cpp
    src/updater.h                     src/updater.cpp
    src/shell.h                       src/shell.cpp
    src/tweakengine.h                 src/tweakengine.cpp
    src/officedownloader.h            src/officedownloader.cpp
    src/framelesswindow.h             src/framelesswindow.cpp
    src/mainwindow.h                  src/mainwindow.cpp
    src/views/setupwizard.h           src/views/setupwizard.cpp

    src/widgets/accentpicker.h        src/widgets/accentpicker.cpp
    src/widgets/actioncard.h          src/widgets/actioncard.cpp
    src/widgets/downloadprogresswidget.h src/widgets/downloadprogresswidget.cpp
    src/widgets/officepackagedialog.h src/widgets/officepackagedialog.cpp
    src/widgets/officeimagedialog.h   src/widgets/officeimagedialog.cpp
    src/widgets/yaoctrudialog.h       src/widgets/yaoctrudialog.cpp
    src/widgets/languagepicker.h      src/widgets/languagepicker.cpp
    src/widgets/applyoverlay.h        src/widgets/applyoverlay.cpp
    src/widgets/borderglow.h          src/widgets/borderglow.cpp
    src/widgets/buttons.h             src/widgets/buttons.cpp
    src/widgets/dialog.h              src/widgets/dialog.cpp
    src/widgets/categoryrow.h         src/widgets/categoryrow.cpp
    src/widgets/debloatrow.h          src/widgets/debloatrow.cpp
    src/widgets/livechart.h           src/widgets/livechart.cpp
    src/widgets/overviewblocks.h      src/widgets/overviewblocks.cpp
    src/widgets/rangeslider.h         src/widgets/rangeslider.cpp
    src/widgets/searchfield.h         src/widgets/searchfield.cpp
    src/widgets/splashscreen.h        src/widgets/splashscreen.cpp
    src/widgets/sectionheader.h       src/widgets/sectionheader.cpp
    src/widgets/settingrow.h          src/widgets/settingrow.cpp
    src/widgets/themeswitch.h         src/widgets/themeswitch.cpp
    src/widgets/typefacepicker.h      src/widgets/typefacepicker.cpp
    src/widgets/segmentedcontrol.h    src/widgets/segmentedcontrol.cpp
    src/widgets/smoothscrollarea.h    src/widgets/smoothscrollarea.cpp
    src/widgets/toggleswitch.h        src/widgets/toggleswitch.cpp
    src/widgets/updatedialog.h        src/widgets/updatedialog.cpp
    src/widgets/tweakrow.h            src/widgets/tweakrow.cpp
    src/widgets/cleanerrow.h          src/widgets/cleanerrow.cpp
    src/widgets/appentry.h            src/widgets/appentry.cpp
    src/widgets/flowlayout.h          src/widgets/flowlayout.cpp
    src/widgets/featurerow.h          src/widgets/featurerow.cpp

    src/views/contentheader.h         src/views/contentheader.cpp
    src/views/overviewpage.h          src/views/overviewpage.cpp
    src/views/actionpage.h            src/views/actionpage.cpp
    src/views/officepage.h            src/views/officepage.cpp
    src/views/tilauncherpage.h        src/views/tilauncherpage.cpp
    src/views/debloatpage.h           src/views/debloatpage.cpp
    src/views/cleanerpage.h           src/views/cleanerpage.cpp
    src/views/appspage.h              src/views/appspage.cpp
    src/views/featurespage.h          src/views/featurespage.cpp
    src/views/godmodepage.h           src/views/godmodepage.cpp
    src/views/journalpage.h           src/views/journalpage.cpp
    src/views/settingspage.h          src/views/settingspage.cpp
    src/views/aboutpage.h             src/views/aboutpage.cpp
    src/views/sidebar.h               src/views/sidebar.cpp
    src/views/statusbar.h             src/views/statusbar.cpp
    src/views/titlebar.h              src/views/titlebar.cpp
    src/views/tweakpage.h             src/views/tweakpage.cpp
    src/views/chrome.h
    src/views/classicchrome.h         src/views/classicchrome.cpp

    # The Fluent shell — design_handoff_fluent_ui. Its own widgets rather than variants of
    # the classic ones: every measurement differs, and a row that asks "which shell?" on
    # each line is two rows written on top of each other.
    src/fluent/fluentchrome.h         src/fluent/fluentchrome.cpp
    src/fluent/fluenttitlebar.h       src/fluent/fluenttitlebar.cpp
    src/fluent/iconrail.h             src/fluent/iconrail.cpp
    src/fluent/categorypane.h         src/fluent/categorypane.cpp
    src/fluent/fluentheader.h         src/fluent/fluentheader.cpp
    src/fluent/applybar.h             src/fluent/applybar.cpp
    src/fluent/fluentcontent.h        src/fluent/fluentcontent.cpp
    src/fluent/fluentbutton.h         src/fluent/fluentbutton.cpp
    src/fluent/fluenttoggle.h         src/fluent/fluenttoggle.cpp
    src/fluent/fluentslider.h         src/fluent/fluentslider.cpp
    src/fluent/fluentsearchbox.h      src/fluent/fluentsearchbox.cpp
    src/fluent/fluenttweakrow.h       src/fluent/fluenttweakrow.cpp
    src/fluent/fluentgroupcard.h      src/fluent/fluentgroupcard.cpp
    src/fluent/fluenticons.h          src/fluent/fluenticons.cpp

    resources/resources.qrc
)

# app.rc carries the executable icon and version block; the manifest asks for the elevated
# token at launch. Listing the manifest as a source is also what stops Qt generating its
# own asInvoker one — see the comment in the file.
# A build that does not ask for the elevated token. For taking screenshots and driving the
# window from a shell that is not an administrator's — nothing else: the catalogue's HKLM
# half cannot be written from such a build, and it says so on Apply.
option(ARBITRIUM_ASINVOKER "Embed an asInvoker manifest instead of requireAdministrator" OFF)

if(WIN32)
    list(APPEND PROJECT_SOURCES app.rc)

    # The manifest reaches the executable through app.rc's RT_MANIFEST resource on every
    # toolchain, and only through that. Handing it to CMake as a source as well makes the
    # MSVC link feed it to mt.exe, which then has two manifests to merge: ours, asking for
    # requireAdministrator, and the linker's own UAC fragment, which says asInvoker. mt.exe
    # refuses — "Values of attribute level not equal in different manifest snippets" — and
    # the link fails. /MANIFEST:NO below keeps the linker out of it entirely.
    if(NOT MSVC)
        list(APPEND PROJECT_SOURCES resources/Arbitrium.manifest)
    endif()

    # app.rc and the manifest each keep their own copy of the version, because neither
    # windres nor the manifest can be handed one portably. A copy that nothing checks is
    # a copy that drifts — all three were on different numbers at 0.9.6 — so check them
    # here and stop the build rather than ship an executable that disagrees with itself.
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/app.rc" ARBITRIUM_RC)
    if(NOT ARBITRIUM_RC MATCHES "\"FileVersion\",[^\"]*\"${PROJECT_VERSION}\"")
        message(FATAL_ERROR
            "app.rc does not carry version ${PROJECT_VERSION}. "
            "Update FILEVERSION, PRODUCTVERSION, FileVersion and ProductVersion in app.rc.")
    endif()

    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/resources/Arbitrium.manifest" ARBITRIUM_MANIFEST)
    if(NOT ARBITRIUM_MANIFEST MATCHES "version=\"${PROJECT_VERSION}\\.0\"")
        message(FATAL_ERROR
            "resources/Arbitrium.manifest does not carry version ${PROJECT_VERSION}.0. "
            "Update the assemblyIdentity version.")
    endif()
endif()

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(Arbitrium
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
else()
    add_executable(Arbitrium ${PROJECT_SOURCES})
endif()

target_include_directories(Arbitrium PRIVATE src)

# The version the running app reports — the About page, the --version switch, and the
# update check's "is there anything newer than me" comparison — comes from project()
# above and nowhere else. It used to be typed out again in main.cpp, which is how a
# 0.9.6 build spent its life announcing that 0.9.6 was available to it.
target_compile_definitions(Arbitrium PRIVATE ARBITRIUM_VERSION="${PROJECT_VERSION}")
# Reaches app.rc as well: CMake hands the target's definitions to windres, which is what
# lets the resource script pick the other manifest.
if(ARBITRIUM_ASINVOKER)
    target_compile_definitions(Arbitrium PRIVATE ARBITRIUM_ASINVOKER)
endif()

# Which kind of Qt are we against? Everything below — deployment, runtime linkage,
# stripping — turns on this one answer.
get_target_property(ARBITRIUM_QT_TYPE Qt${QT_VERSION_MAJOR}::Core TYPE)
if(ARBITRIUM_QT_TYPE STREQUAL "STATIC_LIBRARY")
    set(ARBITRIUM_STATIC_QT ON)
else()
    set(ARBITRIUM_STATIC_QT OFF)
endif()

target_link_libraries(Arbitrium PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Svg
    Qt${QT_VERSION_MAJOR}::Network
)

# Win32 bits used by src/sysinfo.cpp and src/monitor.cpp. TBS and netapi32 are loaded
# dynamically at runtime; these are the ones that are linked. ole32/oleaut32 carry the
# Task Scheduler's COM plumbing for src/tasks.cpp, shell32 the recycle bin calls in
# src/cleaner.cpp — all three are Windows' own, so the single-file build stays one file.
if(WIN32)
    target_link_libraries(Arbitrium PRIVATE advapi32 user32 kernel32 iphlpapi shlwapi
                                            ole32 oleaut32 shell32)
endif()

# A static Qt is only half of a single-file build: without this the MinGW runtime still
# follows the exe around as libgcc / libstdc++ / libwinpthread. Stripping the release
# link is most of the difference between a 40MB executable and a 15MB one.
if(MINGW)
    if(ARBITRIUM_STATIC_QT)
        # --gc-sections drops every function the binary never reaches. Kept because it
        # costs nothing, but do not expect much: measured against a Qt built with
        # -gc-binaries it did not move the executable by a single byte. Almost everything
        # in a Qt binary is reachable through the meta-object system and the vtables it
        # hangs off, so there is very little for the linker to prove dead.
        target_compile_options(Arbitrium PRIVATE -ffunction-sections -fdata-sections)
        target_link_options(Arbitrium PRIVATE
            -static -static-libgcc -static-libstdc++ -Wl,--gc-sections)
    endif()
    target_link_options(Arbitrium PRIVATE $<$<CONFIG:Release>:-s>)
endif()

# See the manifest note above: app.rc already embeds it, so the linker must not embed a
# second one. Without this the MSVC build cannot link at all.
#
# Both flags, because they fail differently. /MANIFEST:NO is the one that matters, and it
# only holds while nothing appends /MANIFEST:EMBED after it — CMake does exactly that for
# a target with a .manifest source, and link.exe takes the last flag it is given, which is
# how this was failing. /MANIFESTUAC:NO removes the reason the merge could not succeed:
# the linker's own asInvoker fragment, which cannot be reconciled with app.rc's
# requireAdministrator. If anything ever puts manifest generation back, it now generates
# something mt.exe can merge instead of stopping the link.
if(MSVC)
    target_link_options(Arbitrium PRIVATE /MANIFEST:NO /MANIFESTUAC:NO)
endif()

# The UI is Turkish; keep the source and execution charsets on UTF-8 so ı/İ/ş/ğ survive.
if(MSVC)
    target_compile_options(Arbitrium PRIVATE /utf-8)
else()
    target_compile_options(Arbitrium PRIVATE -finput-charset=UTF-8 -fexec-charset=UTF-8)
endif()

# Warnings, which until now were something to remember to switch on by hand. Measured over
# the whole tree: -Wall -Wextra -Wshadow produce exactly zero warnings across the twenty
# thousand lines in src/, so the state worth protecting is the one that already exists.
#
# Deliberately not fatal. The compiler that finds the first new one should finish the build
# and show it rather than stopping at it, and CI's log is where it gets read. -Werror
# belongs there rather than here, and only once that job has survived a compiler bump —
# MSYS2 ships whichever GCC it ships, and a new major arriving on a Tuesday would otherwise
# fail everybody's commit over a warning nobody wrote. MSVC is left alone: this measurement
# was taken with GCC, and the project ships a MinGW build.
if(NOT MSVC)
    target_compile_options(Arbitrium PRIVATE -Wall -Wextra -Wshadow)
endif()

# Copy the Qt runtime next to the executable after every link.
#
# Without this the build only runs from a shell that already has Qt's bin directory on
# PATH; launched from Explorer or from Qt Creator's deploy step it dies with
# "Qt6Svg.dll not found" (or whichever import the loader happens to resolve first).
# windeployqt brings the Qt DLLs, the platform/imageformats/styles plugins and — because
# this is a MinGW build — libgcc / libstdc++ / libwinpthread as well.
if(WIN32 AND QT_VERSION_MAJOR EQUAL 6 AND NOT ARBITRIUM_STATIC_QT)
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

# Which plugins a static build links in.
#
# A static Qt imports every plugin it has unless told otherwise, and which plugins it has
# depends on how that Qt was built. The one this project has been released from since 0.9.5
# carries exactly four image formats — GIF, ICO, JPEG, SVG — and two TLS backends,
# cert-only and Schannel. That is the set the shipped executable has always had.
#
# A Qt packaged with more of them is not a free upgrade: MSYS2's adds TIFF, WebP and an
# OpenSSL backend, and each drags its own third-party dependencies in behind it. libtiff
# wants jbig and zstd, libcrypto wants a good deal of ws2_32, and satisfying all of that
# means the executable stops being one file — which is the entire point of the static
# build. Naming the exclusions here keeps the payload the same whichever Qt is in front of
# it. Nothing in the app reads a TIFF or a WebP, and the TLS it uses is Schannel.
if(QT_VERSION_MAJOR EQUAL 6 AND ARBITRIUM_STATIC_QT)
    qt_import_plugins(Arbitrium
        EXCLUDE_BY_TYPE sqldrivers
        EXCLUDE Qt6::QTiffPlugin Qt6::QWebpPlugin Qt6::QTgaPlugin Qt6::QWbmpPlugin
                Qt6::QTlsBackendOpenSSLPlugin
    )
endif()

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(Arbitrium)
endif()

if(WIN32)
    add_custom_command(TARGET Arbitrium POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:Arbitrium>"
            "$<TARGET_FILE_DIR:Arbitrium>/Imperium.exe"
        COMMENT "Generating Imperium.exe"
        VERBATIM
    )
endif()
