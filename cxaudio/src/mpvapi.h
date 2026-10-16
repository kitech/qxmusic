#ifndef CXAUDIO_MPVAPI_H
#define CXAUDIO_MPVAPI_H

/* libmpv is hot-loaded and never linked; its client header is only present on
 * toolchains that ship it. When it is absent this header declares nothing and
 * mpvapi.cpp compiles to an empty unit (mirrors ffmpegapi.h). */
#if defined(__has_include)
#  if __has_include(<mpv/client.h>)
#    define CXAUDIO_MPVAPI_HEADERS 1
#  endif
#endif

#ifdef CXAUDIO_MPVAPI_HEADERS

#include <mpv/client.h>

#include <cstdint>
#include <string>

namespace cx {

/* Dynamically resolved subset of the libmpv client API. */
struct MpvApi {
    mpv_handle* (*create)();
    int (*initialize)(mpv_handle*);
    int (*set_option_string)(mpv_handle*, const char*, const char*);
    int (*command)(mpv_handle*, const char**);
    int (*command_string)(mpv_handle*, const char*);
    int (*set_property)(mpv_handle*, const char*, mpv_format, void*);
    int (*get_property)(mpv_handle*, const char*, mpv_format, void*);
    int (*observe_property)(mpv_handle*, uint64_t, const char*, mpv_format);
    mpv_event* (*wait_event)(mpv_handle*, double);
    const char* (*error_string)(int);
    void (*terminate_destroy)(mpv_handle*);
    void (*set_wakeup_callback)(mpv_handle*, void (*)(void*), void*);
    /* Returns the runtime client API version, encoded MPV_MAKE_VERSION(major,minor). */
    unsigned long (*client_api_version)();

    bool valid() const {
        return create && initialize && command && command_string && wait_event &&
               terminate_destroy && set_property && get_property && observe_property &&
               set_option_string && error_string && set_wakeup_callback &&
               client_api_version;
    }
};

/* Locate and dlopen a compatible libmpv. `libPath` receives the resolved path. */
bool loadMpvApi(MpvApi* api, std::string* libPath, std::string* err);

} /* namespace cx */

#endif /* CXAUDIO_MPVAPI_HEADERS */

#endif /* CXAUDIO_MPVAPI_H */

