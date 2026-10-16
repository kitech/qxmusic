#define CXAUDIO_BUILD
#include "cxaudio.h"

#include "core.h"

#include <new>
#include <string>

struct cxaudio_player {
    cx::Core core;
    cxaudio_cb cb = 0;
    void* user = 0;
};

extern "C" {

cxaudio_player* cxaudio_create(void) {
    return new (std::nothrow) cxaudio_player();
}

void cxaudio_destroy(cxaudio_player* p) {
    if (!p) return;
    /* Tear the backend down (joins its worker threads) while the Core and this
     * wrapper are still fully alive, then drop the callback, then free. This
     * guarantees no in-flight dispatch touches a destroyed object. */
    p->core.stop();
    p->core.setCallback(nullptr);
    delete p;
}

int cxaudio_set_backend(cxaudio_player* p, const char* id) {
    if (!p || !id) return -1;
    return p->core.setBackend(id) ? 0 : -1;
}

int cxaudio_open(cxaudio_player* p, const char* uri) {
    if (!p || !uri) return -1;
    return p->core.open(uri) ? 0 : -1;
}

int cxaudio_play(cxaudio_player* p) { if (!p) return -1; p->core.play(); return 0; }
int cxaudio_pause(cxaudio_player* p) { if (!p) return -1; p->core.pause(); return 0; }
int cxaudio_stop(cxaudio_player* p) { if (!p) return -1; p->core.stop(); return 0; }

int cxaudio_seek(cxaudio_player* p, int64_t ms) {
    if (!p) return -1;
    p->core.seek(ms);
    return 0;
}

int cxaudio_set_volume(cxaudio_player* p, int volume) {
    if (!p) return -1;
    p->core.setVolume(volume);
    return 0;
}

int64_t cxaudio_position(cxaudio_player* p) { return p ? p->core.position() : 0; }
int64_t cxaudio_duration(cxaudio_player* p) { return p ? p->core.duration() : 0; }
int cxaudio_state(cxaudio_player* p) { return p ? p->core.state() : CXAUDIO_STOPPED; }

void cxaudio_set_callback(cxaudio_player* p, cxaudio_cb cb, void* user) {
    if (!p) return;
    p->cb = cb;
    p->user = user;
    if (cb) {
        /* Capture cb/user by value: Core::setCallback/dispatch already
         * synchronize the std::function, so the backend thread never reads the
         * plain cb/user fields concurrently (removes the data race). */
        p->core.setCallback([cb, user](int ev, int64_t a, int64_t b, const std::string& msg) {
            cb(ev, a, b, msg.c_str(), user);
        });
    } else {
        p->core.setCallback(nullptr);
    }
}

const char* cxaudio_backend_id(cxaudio_player* p) {
    static thread_local std::string s;
    s = p ? p->core.backendId() : std::string();
    return s.c_str();
}

const char* cxaudio_last_error(cxaudio_player* p) {
    static thread_local std::string s;
    s = p ? p->core.lastError() : std::string();
    return s.c_str();
}

} /* extern "C" */
