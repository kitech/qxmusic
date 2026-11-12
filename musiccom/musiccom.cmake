# musiccom：qskmusic / qlmusic 共享的封面磁盘缓存与兜底图标（Qt3/4/6 通用）
set(MUSICCOM_DIR ${CMAKE_CURRENT_LIST_DIR})

set(MUSICCOM_SOURCES
    ${MUSICCOM_DIR}/iconcachecore.h
    ${MUSICCOM_DIR}/iconcachecore.cpp
    ${MUSICCOM_DIR}/iconmem.h
    ${MUSICCOM_DIR}/iconmem.cpp
    ${MUSICCOM_DIR}/icongen.h
    ${MUSICCOM_DIR}/icongen.cpp
)

set(MUSICCOM_INCLUDE_DIRS ${MUSICCOM_DIR})
