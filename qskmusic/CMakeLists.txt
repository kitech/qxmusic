cmake_minimum_required(VERSION 3.16)
project(qskmusic LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Quick Gui Network Multimedia)
find_package(QT NAMES Qt6 REQUIRED COMPONENTS LinguistTools)
find_package(QSkinny REQUIRED)

# qskcomp：QSKinny 自绘组件库（与 anystik 同源共享）
include(${CMAKE_CURRENT_SOURCE_DIR}/../qskcomp/qskcomp.cmake)

qt_add_executable(qskmusic
    src/main.cpp
    src/page.h
    src/page.cpp
    src/pagemanager.h
    src/pagemanager.cpp
    src/myi18n.h
    src/myi18n.cpp
    src/mainpage.h
    src/mainpage.cpp
    src/playerbar.h
    src/playerbar.cpp
    src/queuepopup.h
    src/queuepopup.cpp
    src/logpage.h
    src/logpage.cpp
    src/settingspage.h
    src/settingspage.cpp
    src/aboutpage.h
    src/aboutpage.cpp
    src/toastpopup.h
    src/toastpopup.cpp
    src/progresspopup.h
    src/progresspopup.cpp
    src/version_config.h
    ${QSKCOMP_SOURCES}
)

# ── i18n：.qm 经 .qrc 内嵌（先跑 build 脚本里的 lrelease） ──
qt6_add_resources(qskmusic_resources "i18n/i18n.qrc")
target_sources(qskmusic PRIVATE ${qskmusic_resources})

target_include_directories(qskmusic PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${QSKCOMP_INCLUDE_DIRS}
)

target_link_libraries(qskmusic PRIVATE Qsk::QSkinny Qt6::Core Qt6::Gui Qt6::Quick Qt6::Network Qt6::Multimedia Qt6::QuickPrivate)

# 系统 /usr/lib 若存在更新版 Qt6 的未版本化 libQt6*.so，ld 解析 QSkinny 导入目标引入的
# -lQt6* 时会命中错误版本（符号不匹配）。把 vendored Qt 库目录前置到链接搜索路径。
set(_VENDORED_QT_LIB "")
if(DEFINED Qt6_DIR)
    get_filename_component(_VENDORED_QT_LIB "${Qt6_DIR}/../../../lib" ABSOLUTE)
endif()
if(_VENDORED_QT_LIB AND EXISTS "${_VENDORED_QT_LIB}/libQt6Core.so")
    target_link_options(qskmusic PRIVATE "-L${_VENDORED_QT_LIB}")
endif()
