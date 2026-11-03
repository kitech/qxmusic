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

    /* 远程流（http/https）自动切换：faad 后端（miniaudio/faad2 链路）
     * 只支持本地文件（ma_decoder_init_file/fopen），须走 ffmpeg ——
     * FfmpegDecoder 的 avformat_open_input 内建 http/mp3 支持。
     * 显式选定 ffmpeg/mpv 时不介入。 */
    if (target == "faad" &&
        (uri.rfind("http://", 0) == 0 || uri.rfind("https://", 0) == 0))
        target = "ffmpeg";

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
    if (!pendingOutputDeviceName_.empty())
        backend_->setOutputDeviceName(pendingOutputDeviceName_);

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

std::vector<AudioDeviceInfo> Core::listOutputDevices() {
    if (backend_) return backend_->listOutputDevices();
    /* 未 open 枚举：临时 PcmBackend（枚举自建临时 context，与 backend 选择无关） */
    PcmBackend probe(faadProvider());
    return probe.listOutputDevices();
}

void Core::setOutputDeviceName(const std::string& name) {
    pendingOutputDeviceName_ = name;
    if (backend_) backend_->setOutputDeviceName(name);
}

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
