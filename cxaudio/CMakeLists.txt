cmake_minimum_required(VERSION 3.15)
project(cxaudio LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

include(${CMAKE_CURRENT_SOURCE_DIR}/cxaudio.cmake)

# Android builds a shared lib (libcxaudio.so) for all ABIs; desktop defaults to
# a static lib (qlmusic compiles the sources directly and does not link it).
if(ANDROID)
    add_library(cxaudio SHARED ${CXAUDIO_SOURCES})
else()
    add_library(cxaudio STATIC ${CXAUDIO_SOURCES})
endif()

target_include_directories(cxaudio PUBLIC ${CXAUDIO_INCLUDE_DIRS})
target_compile_definitions(cxaudio PRIVATE ${CXAUDIO_FAAD_DEFINES})

if(MSVC)
    target_compile_options(cxaudio PRIVATE /wd4244 /wd4305)
elseif(CMAKE_C_COMPILER_ID MATCHES "GNU")
    set_source_files_properties(${CXAUDIO_FAAD_SOURCES}
        PROPERTIES COMPILE_OPTIONS "-ffloat-store")
endif()

if(ANDROID)
    target_link_libraries(cxaudio log OpenSLES)
    target_link_options(cxaudio PRIVATE -Wl,-z,max-page-size=16384)
elseif(WIN32)
    target_link_libraries(cxaudio ole32 winmm user32)
elseif(APPLE)
    target_link_libraries(cxaudio
        "-framework CoreAudio" "-framework CoreFoundation" "-framework AudioToolbox")
else()
    target_link_libraries(cxaudio dl pthread m)
endif()

# Desktop smoke-test CLI (build with -DCXAUDIO_BUILD_SMOKE=ON)
option(CXAUDIO_BUILD_SMOKE "Build the cxaudio_smoke CLI" OFF)
if(CXAUDIO_BUILD_SMOKE AND NOT ANDROID)
    add_executable(cxaudio_smoke ${CMAKE_CURRENT_SOURCE_DIR}/tools/cxaudio_smoke.cpp)
    target_link_libraries(cxaudio_smoke cxaudio)
endif()
