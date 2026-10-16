#ifndef CXAUDIO_H
#define CXAUDIO_H

#include <stdint.h>

/* Public C ABI export decoration. All cxaudio_* entry points are exported even
 * when the translation unit is built with -fvisibility=hidden (Android/qmake).
 * Define CXAUDIO_BUILD while compiling the library itself (see cxaudio_c.cpp);
 * on Windows that yields dllexport, consumers stay neutral (no dllimport, so
 * static linking keeps working). */
#if defined(_WIN32) || defined(__CYGWIN__)
#  if defined(CXAUDIO_BUILD)
#    define CXAUDIO_API __declspec(dllexport)
#  else
#    define CXAUDIO_API
#  endif
#elif defined(__GNUC__) && __GNUC__ >= 4
#  define CXAUDIO_API __attribute__((visibility("default")))
#else
#  define CXAUDIO_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct cxaudio_player cxaudio_player;

/* event codes delivered to cxaudio_cb(ev, a, b, msg, user) */
enum {
    CXAUDIO_EV_STATE    = 0, /* a = state (see CXAUDIO_STOPPED..)            */
    CXAUDIO_EV_POSITION = 1, /* a = position ms, b = duration ms             */
    CXAUDIO_EV_DURATION = 2, /* a = duration ms                              */
    CXAUDIO_EV_EOF      = 3, /* a = 0                                        */
    CXAUDIO_EV_ERROR    = 4  /* msg = error text                             */
};

/* player states */
enum {
    CXAUDIO_STOPPED   = 0,
    CXAUDIO_PLAYING   = 1,
    CXAUDIO_PAUSED    = 2,
    CXAUDIO_BUFFERING = 3,
    CXAUDIO_ERROR     = 4
};

typedef void (*cxaudio_cb)(int ev, int64_t a, int64_t b, const char* msg, void* user);

CXAUDIO_API cxaudio_player* cxaudio_create(void);
CXAUDIO_API void            cxaudio_destroy(cxaudio_player* p);

/* id: "auto" | "ffmpeg" | "faad" | "mpv"; takes effect on next open() */
CXAUDIO_API int  cxaudio_set_backend(cxaudio_player* p, const char* id);

CXAUDIO_API int  cxaudio_open(cxaudio_player* p, const char* uri);
CXAUDIO_API int  cxaudio_play(cxaudio_player* p);
CXAUDIO_API int  cxaudio_pause(cxaudio_player* p);
CXAUDIO_API int  cxaudio_stop(cxaudio_player* p);
CXAUDIO_API int  cxaudio_seek(cxaudio_player* p, int64_t ms);
CXAUDIO_API int  cxaudio_set_volume(cxaudio_player* p, int volume); /* 0..100 */

CXAUDIO_API int64_t cxaudio_position(cxaudio_player* p);
CXAUDIO_API int64_t cxaudio_duration(cxaudio_player* p);
CXAUDIO_API int     cxaudio_state(cxaudio_player* p);

CXAUDIO_API void cxaudio_set_callback(cxaudio_player* p, cxaudio_cb cb, void* user);

CXAUDIO_API const char* cxaudio_backend_id(cxaudio_player* p);
CXAUDIO_API const char* cxaudio_last_error(cxaudio_player* p);

#ifdef __cplusplus
}
#endif

#endif /* CXAUDIO_H */
