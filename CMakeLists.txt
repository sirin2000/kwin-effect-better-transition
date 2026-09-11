cmake_minimum_required(VERSION 3.16.0)

project(kwin-effect-better-transition
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose Release or Debug" FORCE)
endif()

# Dependencies
# The explicit version is important: ECM only defaults to Qt 6 when it knows it
# was found with a recent enough version. Otherwise it probes for Qt 5 first,
# which in turn poisons QT_DEFAULT_MAJOR_VERSION and breaks the Qt 6 Qml macros.
find_package(ECM 5.240 REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${ECM_MODULE_PATH} ${ECM_KDE_MODULE_DIR})

# Make sure ECM helpers pick Qt 6 no matter what.
set(QT_MAJOR_VERSION 6)

include(FeatureSummary)
include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)

find_package(Qt6 6.6 REQUIRED COMPONENTS
    Core
    DBus
    Gui
    Widgets
)

find_package(KF6 6.6 REQUIRED COMPONENTS
    Config
    ConfigWidgets
    CoreAddons
    I18n
    KCMUtils
)

find_package(KWin REQUIRED COMPONENTS kwineffects)
find_package(KWinDBusInterface CONFIG REQUIRED)

# The effect uses the paint API that was introduced with the 6.7 development
# cycle: prePaintScreen(ScreenPrePaintData &) and
# prePaintWindow(RenderView *, EffectWindow *, WindowPrePaintData &).
if(KWin_VERSION VERSION_LESS 6.6.90)
    message(FATAL_ERROR
        "kwin-effect-better-transition requires KWin 6.7 or newer (found ${KWin_VERSION}).")
endif()

add_subdirectory(src)

feature_summary(WHAT ALL)
