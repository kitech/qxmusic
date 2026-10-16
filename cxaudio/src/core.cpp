#include "core.h"
#include "pcm_backend.h"
#include "pcm_decoder.h"

#include <algorithm>
#include <cctype>

namespace cx {

Core::Core() {}

Core::~Core() {
    backend_.reset();
    std::lock_guard<std::mutex> lk(mutex_);
    cb_ = nullptr;
}

std::string Core::resolveId() const {
    std::string id = requested_;
    std::transform(id.begin(), id.end(), id.begin(),
                   [](unsigned char c) { return (char)std::tolower(c); });
    if (id.empty() || id == "auto") return "faad";
    if (id == "b" || id == "miniaudio" || id == "faad" || id == "faad2") return "faad";
    if (id == "a" || id == "ffmpeg") return "ffmpeg";
    if (id == "c" || id == "mpv" || id == "libmpv") return "mpv";
    return "";
}

bool Core::setBackend(const std::string& id) {
    std::string keep = requested_;
    requested_ = id;
    if (resolveId().empty()) {
        requested_ = keep;
        std::lock_guard<std::mutex> lk(mutex_);
        lastError_ = "unknown backend id: " + id;
        return false;
    }
    return true;
}

bool Core::open(const std::string& uri) {
    std::string target = resolveId();
    if (target.empty()) {
        std::lock_guard<std::mutex> lk(mutex_);
        lastError_ = "unknown backend id: " + requested_;
        return false;
    }

    backend_.reset();
    resolved_ = target;

    if (target == "faad") {
        backend_.reset(new PcmBackend(faadProvider()));
    } else if (target == "ffmpeg") {
        backend_.reset(new PcmBackend(ffmpegProvider()));
    } else if (target == "mpv") {
        backend_.reset(createMpvBackend());
    }

    if (!backend_) {
        std::lock_guard<std::mutex> lk(mutex_);
        lastError_ = "backend not compiled: " + target;
        return false;
    }

    cbs_.onState = [this](int st) { dispatch(kEvState, st, 0, ""); };
    cbs_.onPosition = [this](int64_t pos, int64_t dur) {
        dispatch(kEvPosition, pos, dur, "");
    };
    cbs_.onEof = [this]() { dispatch(kEvEof, 0, 0, ""); };
    cbs_.onError = [this](const std::string& msg) {
        dispatch(kEvError, 0, 0, msg);
    };
    backend_->setCallbacks(&cbs_);

    std::string err;
    if (!backend_->available(&err)) {
        std::lock_guard<std::mutex> lk(mutex_);
        lastError_ = err.empty() ? "backend unavailable" : err;
        backend_.reset();
        return false;
    }

    if (!backend_->open(uri, &err)) {
        std::lock_guard<std::mutex> lk(mutex_);
        lastError_ = err.empty() ? "open failed" : err;
        return false;
    }
    return true;
}

void Core::play() { if (backend_) backend_->play(); }
void Core::pause() { if (backend_) backend_->pause(); }
void Core::stop() { if (backend_) backend_->stop(); }
void Core::seek(int64_t ms) { if (backend_) backend_->seek(ms); }
void Core::setVolume(int v) { if (backend_) backend_->setVolume(v); }

int64_t Core::position() const { return backend_ ? backend_->position() : 0; }
int64_t Core::duration() const { return backend_ ? backend_->duration() : 0; }
int Core::state() const { return backend_ ? backend_->state() : kStopped; }

std::string Core::backendId() const { return resolved_; }

std::string Core::lastError() const {
    std::lock_guard<std::mutex> lk(mutex_);
    return lastError_;
}

void Core::setCallback(Callback cb) {
    std::lock_guard<std::mutex> lk(mutex_);
    cb_ = cb;
}

void Core::dispatch(int ev, int64_t a, int64_t b, const std::string& msg) {
    Callback cb;
    {
        std::lock_guard<std::mutex> lk(mutex_);
        if (ev == kEvError) lastError_ = msg;
        cb = cb_;
    }
    if (cb) cb(ev, a, b, msg);
}

} /* namespace cx */
