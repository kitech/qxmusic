# qlmusic audio subsystem (cxaudio core) — compiled directly into the app.
# Mirrors cxaudio/cxaudio.cmake; keep the two source lists in sync.

CXROOT = $$PWD
AUDIO = $$CXROOT/../cxaudio
THIRD = $$CXROOT/../third_party

INCLUDEPATH += $$AUDIO/include $$AUDIO/src
INCLUDEPATH += $$THIRD/miniaudio $$THIRD/stb $$THIRD/minimp4
INCLUDEPATH += $$THIRD/faad2/include $$THIRD/faad2/libfaad
INCLUDEPATH += $$THIRD/mpv/include
INCLUDEPATH += $$CXROOT/src/audio

DEFINES += HAVE_INTTYPES_H=1 HAVE_MEMCPY=1 HAVE_STRING_H=1 HAVE_STRINGS_H=1 \
           HAVE_SYS_STAT_H=1 HAVE_SYS_TYPES_H=1 HAVE_LRINTF=1 APPLY_DRC

# PACKAGE_VERSION must reach the compiler as a C string literal. Qt3's qmake
# escapes backslashes differently from Qt4/5/6, so pick the right form per Qt.
CX_PKGVER = \\\"2.11.3\\\"
isEmpty(QT_VERSION): CX_PKGVER = \"2.11.3\"
DEFINES += PACKAGE_VERSION=$$CX_PKGVER

# minimp4 uses strdup(); strict -std=c11 hides it on glibc
unix: DEFINES += _GNU_SOURCE

unix:!macx: QMAKE_CFLAGS += -ffloat-store

SOURCES += \
    $$AUDIO/src/core.cpp \
    $$AUDIO/src/dynload.cpp \
    $$AUDIO/src/audioutil.cpp \
    $$AUDIO/src/pcm_backend.cpp \
    $$AUDIO/src/decoder_faad.cpp \
    $$AUDIO/src/decoder_ffmpeg.cpp \
    $$AUDIO/src/ffmpegapi.cpp \
    $$AUDIO/src/backend_mpv.cpp \
    $$AUDIO/src/mpvapi.cpp \
    $$AUDIO/src/cxaudio_c.cpp \
    $$AUDIO/src/cxaudio_cpp.cpp \
    $$AUDIO/src/miniaudio_impl.c \
    $$AUDIO/src/minimp4_impl.c \
    $$AUDIO/src/stb_vorbis_impl.c

HEADERS += \
    $$AUDIO/include/cxaudio.h \
    $$AUDIO/include/cxaudio.hpp \
    $$AUDIO/src/cxcommon.h \
    $$AUDIO/src/dynload.h \
    $$AUDIO/src/audioutil.h \
    $$AUDIO/src/pcm_decoder.h \
    $$AUDIO/src/pcm_backend.h \
    $$AUDIO/src/ffmpegapi.h \
    $$AUDIO/src/mpvapi.h \
    $$AUDIO/src/stb_vorbis_iface.h

# FAAD2 library sources (explicit list: Qt3 qmake has no $$files() globbing)
SOURCES += \
    $$THIRD/faad2/libfaad/bits.c \
    $$THIRD/faad2/libfaad/cfft.c \
    $$THIRD/faad2/libfaad/common.c \
    $$THIRD/faad2/libfaad/decoder.c \
    $$THIRD/faad2/libfaad/drc.c \
    $$THIRD/faad2/libfaad/drm_dec.c \
    $$THIRD/faad2/libfaad/error.c \
    $$THIRD/faad2/libfaad/filtbank.c \
    $$THIRD/faad2/libfaad/hcr.c \
    $$THIRD/faad2/libfaad/huffman.c \
    $$THIRD/faad2/libfaad/ic_predict.c \
    $$THIRD/faad2/libfaad/is.c \
    $$THIRD/faad2/libfaad/lt_predict.c \
    $$THIRD/faad2/libfaad/mdct.c \
    $$THIRD/faad2/libfaad/mp4.c \
    $$THIRD/faad2/libfaad/ms.c \
    $$THIRD/faad2/libfaad/output.c \
    $$THIRD/faad2/libfaad/pns.c \
    $$THIRD/faad2/libfaad/ps_dec.c \
    $$THIRD/faad2/libfaad/ps_syntax.c \
    $$THIRD/faad2/libfaad/pulse.c \
    $$THIRD/faad2/libfaad/rvlc.c \
    $$THIRD/faad2/libfaad/sbr_dct.c \
    $$THIRD/faad2/libfaad/sbr_dec.c \
    $$THIRD/faad2/libfaad/sbr_e_nf.c \
    $$THIRD/faad2/libfaad/sbr_fbt.c \
    $$THIRD/faad2/libfaad/sbr_hfadj.c \
    $$THIRD/faad2/libfaad/sbr_hfgen.c \
    $$THIRD/faad2/libfaad/sbr_huff.c \
    $$THIRD/faad2/libfaad/sbr_qmf.c \
    $$THIRD/faad2/libfaad/sbr_syntax.c \
    $$THIRD/faad2/libfaad/sbr_tf_grid.c \
    $$THIRD/faad2/libfaad/specrec.c \
    $$THIRD/faad2/libfaad/ssr.c \
    $$THIRD/faad2/libfaad/ssr_fb.c \
    $$THIRD/faad2/libfaad/ssr_ipqf.c \
    $$THIRD/faad2/libfaad/syntax.c \
    $$THIRD/faad2/libfaad/tns.c

# AudioEngineAdapter（QtCore 包装，仅桌面壳使用；控制台 smoke 工具用
# CONFIG+=audio_smoke 跳过，避免依赖 QtWidgets）
!contains(CONFIG, audio_smoke) {
    HEADERS += $$CXROOT/src/audio/AudioEngineAdapter.h
    SOURCES += $$CXROOT/src/audio/AudioEngineAdapter.cpp
}

unix:!macx: LIBS += -ldl -lpthread -lm
macx: LIBS += -framework CoreFoundation -framework CoreAudio -framework AudioToolbox
win32: LIBS += -lole32 -lwinmm -luser32
