#ifndef CXAUDIO_CORE_H
#define CXAUDIO_CORE_H

#include "cxcommon.h"

#include <functional>
#include <memory>
#include <mutex>
#include <string>

namespace cx {

IBackend* createMpvBackend();

/* Engine shared by the C ABI and the C++ wrapper. */
class Core {
public:
    using Callback = std::function<void(int ev, int64_t a, int64_t b, const std::string& msg)>;

    Core();
    ~Core();

    bool setBackend(const std::string& id); /* returns false if id unknown */
    bool open(const std::string& uri);

    void play();
    void pause();
    void stop();
    void seek(int64_t ms);
    void setVolume(int volume);

    int64_t position() const;
    int64_t duration() const;
    int state() const;

    std::string backendId() const;
    std::string lastError() const;

    void setCallback(Callback cb);

private:
    Core(const Core&);
    Core& operator=(const Core&);

    void dispatch(int ev, int64_t a, int64_t b, const std::string& msg);
    std::string resolveId() const;

    std::string requested_ = "auto";
    std::string resolved_ = "";
    std::unique_ptr<IBackend> backend_;
    BackendCallbacks cbs_;

    mutable std::mutex mutex_;
    Callback cb_;
    std::string lastError_;
};

} /* namespace cx */

#endif /* CXAUDIO_CORE_H */
