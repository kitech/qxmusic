#include "mpvapi.h"
#include "dynload.h"

#ifdef CXAUDIO_MPVAPI_HEADERS

#include <cstdlib>
#include <new>
#include <string>
#include <vector>

namespace cx {
namespace {

const char* const kCandidates[] = {
#if defined(_WIN32)
    "mpv-2.dll", "libmpv-2.dll", "mpv-1.dll", "libmpv-1.dll", "libmpv.dll",
#elif defined(__APPLE__)
    "libmpv.2.dylib", "libmpv.1.dylib", "libmpv.dylib",
#else
    "libmpv.so.2", "libmpv.so.1", "libmpv.so",
#endif
};

bool tryLoad(Library* lib, MpvApi* api) {
    api->create = (mpv_handle * (*)()) lib->sym("mpv_create");
    api->initialize = (int (*)(mpv_handle*))lib->sym("mpv_initialize");
    api->set_option_string =
        (int (*)(mpv_handle*, const char*, const char*))lib->sym("mpv_set_option_string");
    api->command = (int (*)(mpv_handle*, const char**))lib->sym("mpv_command");
    api->command_string = (int (*)(mpv_handle*, const char*))lib->sym("mpv_command_string");
    api->set_property =
        (int (*)(mpv_handle*, const char*, mpv_format, void*))lib->sym("mpv_set_property");
    api->get_property =
        (int (*)(mpv_handle*, const char*, mpv_format, void*))lib->sym("mpv_get_property");
    api->observe_property =
        (int (*)(mpv_handle*, uint64_t, const char*, mpv_format))lib->sym("mpv_observe_property");
    api->wait_event = (mpv_event * (*)(mpv_handle*, double)) lib->sym("mpv_wait_event");
    api->error_string = (const char* (*)(int))lib->sym("mpv_error_string");
    api->terminate_destroy = (void (*)(mpv_handle*))lib->sym("mpv_terminate_destroy");
    api->set_wakeup_callback =
        (void (*)(mpv_handle*, void (*)(void*), void*))lib->sym("mpv_set_wakeup_callback");
    api->client_api_version = (unsigned long (*)())lib->sym("mpv_client_api_version");
    return api->valid();
}

/* Reject a libmpv whose client API major differs from the header we compiled
 * against; only that major is ABI-stable. MPV_MAKE_VERSION(major, minor) packs
 * major in the high 16 bits. */
bool mpvVersionOk(MpvApi* api, std::string* err) {
    const unsigned long runtime = api->client_api_version();
    const unsigned long header = MPV_CLIENT_API_VERSION;
    if ((runtime >> 16) != (header >> 16)) {
        const char* allow = std::getenv("CXAUDIO_MPV_ALLOW_VERSION_MISMATCH");
        if (!(allow && *allow)) {
            if (err) {
                *err = "libmpv client API major version mismatch (runtime " +
                       std::to_string(runtime >> 16) + "." +
                       std::to_string(runtime & 0xffffUL) + " != header " +
                       std::to_string(header >> 16) + "." +
                       std::to_string(header & 0xffffUL) +
                       "; set CXAUDIO_MPV_ALLOW_VERSION_MISMATCH=1 to override)";
            }
            return false;
        }
    }
    return true;
}

} /* namespace */

bool loadMpvApi(MpvApi* api, std::string* libPath, std::string* err) {
    api->create = 0;
    api->initialize = 0;
    api->set_option_string = 0;
    api->command = 0;
    api->command_string = 0;
    api->set_property = 0;
    api->get_property = 0;
    api->observe_property = 0;
    api->wait_event = 0;
    api->error_string = 0;
    api->terminate_destroy = 0;
    api->set_wakeup_callback = 0;
    api->client_api_version = 0;

    Library* lib = new Library();

    const char* explicitPath = std::getenv("CXAUDIO_MPV_LIB");
    if (explicitPath && *explicitPath) {
        if (lib->open(explicitPath) && tryLoad(lib, api)) {
            if (mpvVersionOk(api, err)) {
                if (libPath) *libPath = lib->path();
                return true;
            }
            delete lib;
            return false;
        }
    }

    std::string lastErr = "libmpv not found";
    std::vector<std::string> dirs = librarySearchDirs("CXAUDIO_MPV_DIR");
    for (size_t d = 0; d < dirs.size(); ++d) {
        for (size_t c = 0; c < sizeof(kCandidates) / sizeof(kCandidates[0]); ++c) {
            std::string full = dirs[d];
            if (!full.empty() && full[full.size() - 1] != '/' && full[full.size() - 1] != '\\')
                full += "/";
            full += kCandidates[c];
            if (!fileExists(full)) continue;
            if (lib->open(full) && tryLoad(lib, api)) {
                if (!mpvVersionOk(api, &lastErr)) continue;
                if (libPath) *libPath = lib->path();
                return true;
            }
        }
    }

    delete lib;
    if (err) *err = lastErr;
    return false;
}

} /* namespace cx */

#endif /* CXAUDIO_MPVAPI_HEADERS */
