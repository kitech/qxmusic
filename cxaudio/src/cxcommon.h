#ifndef CXAUDIO_CXCOMMON_H
#define CXAUDIO_CXCOMMON_H

#include <cstdint>
#include <functional>
#include <string>

namespace cx {

enum PlayerState {
    kStopped   = 0,
    kPlaying   = 1,
    kPaused    = 2,
    kBuffering = 3,
    kError     = 4
};

enum EventType {
    kEvState    = 0,
    kEvPosition = 1,
    kEvDuration = 2,
    kEvEof      = 3,
    kEvError    = 4
};

struct BackendCallbacks {
    std::function<void(int state)> onState;
    std::function<void(int64_t posMs, int64_t durMs)> onPosition;
    std::function<void()> onEof;
    std::function<void(const std::string&)> onError;
};

/* A playback backend. A/B decode to PCM and feed a device; C hands off to mpv. */
class IBackend {
public:
    virtual ~IBackend() {}

    virtual const char* id() const = 0;
    virtual bool available(std::string* err) = 0;
    virtual bool open(const std::string& uri, std::string* err) = 0;

    virtual void play() = 0;
    virtual void pause() = 0;
    virtual void stop() = 0;
    virtual void seek(int64_t ms) = 0;
    virtual void setVolume(int volume) = 0;

    virtual int64_t position() const = 0;
    virtual int64_t duration() const = 0;
    virtual int state() const = 0;

    void setCallbacks(BackendCallbacks* cb) { cb_ = cb; }

protected:
    BackendCallbacks* cb_ = nullptr;
};

} /* namespace cx */

#endif /* CXAUDIO_CXCOMMON_H */
