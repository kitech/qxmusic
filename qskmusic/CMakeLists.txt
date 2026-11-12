cmake_minimum_required(VERSION 3.16)
project(qskmusic LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Quick Gui Network Multimedia)
find_package(QT NAMES Qt6 REQUIRED COMPONENTS LinguistTools)
find_package(QSkinny REQUIRED)

if(ANDROID)
    # /opt/qt/qskinny-arm64 的导出目标把 Qt 库写成裸名（Qt6Gui_arm64-v8a 等），
    # 不映射回真实目标的话链接期会变成 -lQt6Gui_arm64-v8a 找不到库
    macro(_fix_qskinny_abi_target old_target new_target)
        if(NOT TARGET "${old_target}" AND TARGET "${new_target}")
            add_library("${old_target}" INTERFACE IMPORTED)
            target_link_libraries("${old_target}" INTERFACE "${new_target}")
        endif()
    endmacro()
    _fix_qskinny_abi_target(Qt6Gui_arm64-v8a   Qt6::Gui)
    _fix_qskinny_abi_target(Qt6Qml_arm64-v8a   Qt6::Qml)
    _fix_qskinny_abi_target(Qt6Quick_arm64-v8a Qt6::Quick)
    _fix_qskinny_abi_target(Qt6OpenGL_arm64-v8a Qt6::OpenGL)
endif()

# qskcomp：QSKinny 自绘组件库（与 anystik 同源共享）
include(${CMAKE_CURRENT_SOURCE_DIR}/../qskcomp/qskcomp.cmake)

# ── libncm：NCM 客户端共享库（go163-music-core 预构建，build 脚本先行产出） ──
# 各平台产物先由 build 脚本拷进 ${CMAKE_BINARY_DIR}，IMPORTED 指向该副本。
# ELF 无 SONAME 时必须 IMPORTED_NO_SONAME，让 ld 用 -lncm 链接；
# 否则 DT_NEEDED 会记成链接命令行上的绝对路径，APK 运行期 dlopen 必崩。
# mac 不设：dylib 自带 install_name(@rpath/libncm.dylib)，走常规链接+rpath。
if(ANDROID)
    set(NCM_SRC_SO ${CMAKE_CURRENT_SOURCE_DIR}/../go163-music-core/out/android/arm64-v8a/libncm.so)
    set(NCM_SO "${CMAKE_BINARY_DIR}/libncm.so")
elseif(APPLE)
    set(NCM_SRC_SO ${CMAKE_CURRENT_SOURCE_DIR}/../go163-music-core/out/libncm.dylib)
    set(NCM_SO "${CMAKE_BINARY_DIR}/libncm.dylib")
else()
    set(NCM_SRC_SO ${CMAKE_CURRENT_SOURCE_DIR}/../go163-music-core/out/libncm.so)
    set(NCM_SO "${CMAKE_BINARY_DIR}/libncm.so")
endif()
if(NOT EXISTS "${NCM_SRC_SO}")
    message(FATAL_ERROR "libncm 产物缺失: ${NCM_SRC_SO}（先跑 build 脚本里的 libncm 构建步）")
endif()
if(NOT EXISTS "${NCM_SO}")
    message(FATAL_ERROR "libncm 副本缺失: ${NCM_SO}（build 脚本应先把它拷进构建目录）")
endif()
add_library(ncm SHARED IMPORTED)
set_target_properties(ncm PROPERTIES IMPORTED_LOCATION "${NCM_SO}")
if(NOT APPLE)
    set_target_properties(ncm PROPERTIES IMPORTED_NO_SONAME TRUE)
endif()

# ELF e_machine 守卫（offset 0x12，LE）：aarch64=B7、x86-64=3E。
# 防错链跨架构产物（如把宿主 x86-64 库链进 Android 包）。mac 产物是
# Mach-O 不是 ELF，整体跳过（否则 arm64 会误命中 aarch64 分支）。
if(NOT APPLE)
    file(READ "${NCM_SO}" NCM_EMachine HEX LIMIT 2 OFFSET 18)
    if(ANDROID OR CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
        set(NCM_EXPECT_EM "^[bB]7")
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64)")
        set(NCM_EXPECT_EM "^[3eE]")
    endif()
    if(DEFINED NCM_EXPECT_EM AND NOT NCM_EMachine MATCHES "${NCM_EXPECT_EM}")
        message(FATAL_ERROR "libncm 架构与目标不符: ${NCM_SO} (e_machine=${NCM_EMachine}，期望 ${NCM_EXPECT_EM})")
    endif()
endif()

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
    src/playback.h
    src/playback.cpp
    src/playerbar.h
    src/playerbar.cpp
    src/progressline.h
    src/progressline.cpp
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
    src/ncmapi.h
    src/ncmapi.cpp
    src/lyricpopup.h
    src/lyricpopup.cpp
    src/ncmcatalog.h
    src/ncmcatalog.cpp
    src/searchpage.h
    src/searchpage.cpp
    src/playlistlistpage.h
    src/playlistlistpage.cpp
    src/playlisttrackspage.h
    src/playlisttrackspage.cpp
    src/statusline.h
    src/statusline.cpp
    src/downloadservice.h
    src/downloadservice.cpp
    src/progresspopup.h
    src/progresspopup.cpp
    src/keepalive.h
    src/keepalive.cpp
    src/networkmonitor.h
    src/networkmonitor.cpp
    src/androidutils.h
    src/androidutils.cpp
    src/envprobe.h
    src/envprobe.cpp
    src/version_config.h
    ${QSKCOMP_SOURCES}
)

# ── i18n：.qm 经 .qrc 内嵌（先跑 build 脚本里的 lrelease） ──
qt6_add_resources(qskmusic_resources "i18n/i18n.qrc")
target_sources(qskmusic PRIVATE ${qskmusic_resources})

target_include_directories(qskmusic PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../go163-music-core/out/include
    ${QSKCOMP_INCLUDE_DIRS}
)

target_link_libraries(qskmusic PRIVATE Qsk::QSkinny Qt6::Core Qt6::Gui Qt6::Quick Qt6::Network Qt6::Multimedia Qt6::QuickPrivate ncm)

# ── extras/go 共享库 (c-shared) ──
set(GO_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/extras/go)
set(GO_OUT_DIR ${CMAKE_BINARY_DIR})

if(ANDROID)
    set(GO_ENV
        GOOS=android GOARCH=arm64 CGO_ENABLED=1
        CC=${ANDROID_NDK_ROOT}/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android24-clang
    )
    set(GO_LIB ${GO_OUT_DIR}/libgoso.so)
elseif(APPLE)
    # macos-14+ runner 为 arm64；硬编码 amd64 产出 x86_64 dylib 导致链接架构不符
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
        set(GO_ENV GOOS=darwin GOARCH=arm64 CGO_ENABLED=1)
    else()
        set(GO_ENV GOOS=darwin GOARCH=amd64 CGO_ENABLED=1)
    endif()
    set(GO_LIB ${GO_OUT_DIR}/libgoso.dylib)
elseif(WIN32)
    set(GO_ENV GOOS=windows GOARCH=amd64 CGO_ENABLED=1)
    set(GO_LIB ${GO_OUT_DIR}/libgoso.dll)
else()
    set(GO_ENV GOOS=linux GOARCH=amd64 CGO_ENABLED=1 CC=gcc)
    set(GO_LIB ${GO_OUT_DIR}/libgoso.so)
endif()

set(GO_HEADER ${GO_OUT_DIR}/goso.h)

add_custom_command(
    OUTPUT ${GO_LIB} ${GO_HEADER}
    COMMAND ${CMAKE_COMMAND} -E env ${GO_ENV}
            go build -buildmode=c-shared -o ${GO_LIB} .
    WORKING_DIRECTORY ${GO_SRC_DIR}
    DEPENDS ${GO_SRC_DIR}/goso.go
    COMMENT "Building Go shared library (goso)"
)
add_custom_target(goso_build DEPENDS ${GO_LIB})

add_library(goso SHARED IMPORTED GLOBAL)
set_target_properties(goso PROPERTIES
    IMPORTED_LOCATION ${GO_LIB}
    IMPORTED_NO_SONAME TRUE
)
add_dependencies(goso goso_build)

# ── extras/cpp 共享库 ──
add_library(cso SHARED
    extras/cpp/cso.cpp
    extras/cpp/cso.h
)

# ── 主程序链接 extras ──
target_link_libraries(qskmusic PRIVATE goso cso)

# 系统 /usr/lib 若存在更新版 Qt6 的未版本化 libQt6*.so，ld 解析 QSkinny 导入目标引入的
# -lQt6* 时会命中错误版本（符号不匹配）。把 vendored Qt 库目录前置到链接搜索路径。
set(_VENDORED_QT_LIB "")
if(DEFINED Qt6_DIR)
    get_filename_component(_VENDORED_QT_LIB "${Qt6_DIR}/../../../lib" ABSOLUTE)
endif()
if(_VENDORED_QT_LIB AND EXISTS "${_VENDORED_QT_LIB}/libQt6Core.so")
    target_link_options(qskmusic PRIVATE "-L${_VENDORED_QT_LIB}")
endif()
