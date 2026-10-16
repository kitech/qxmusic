#ifndef CXAUDIO_HPP
#define CXAUDIO_HPP

#include <cstdint>
#include <functional>
#include <string>

namespace cxaudio {

enum Event {
    EvState    = 0,
    EvPosition = 1,
    EvDuration = 2,
    EvEof      = 3,
    EvError    = 4
};

enum State {
    Stopped   = 0,
    Playing   = 1,
    Paused    = 2,
    Buffering = 3,
    Error     = 4
};

/* Thin C++ wrapper over the same engine used by the C ABI.
 * Not thread safe: call from one thread (the UI thread). */
class Player {
public:
    using Callback = std::function<void(int ev, int64_t a, int64_t b, const std::string& msg)>;

    Player();
    ~Player();
    Player(const Player&) = delete;
    Player& operator=(const Player&) = delete;

    /* id: "auto" | "ffmpeg" | "faad" | "mpv"; takes effect on next open() */
    bool setBackend(const std::string& id);

    bool open(const std::string& uri);
    void play();
    void pause();
    void stop();
    void seek(int64_t ms);
    void setVolume(int volume); /* 0..100 */

    int64_t position() const;
    int64_t duration() const;
    int state() const;

    std::string backendId() const;
    std::string lastError() const;

    void setCallback(Callback cb);

private:
    struct Impl;
    Impl* d_;
};

} /* namespace cxaudio */

#endif /* CXAUDIO_HPP */
