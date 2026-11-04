#include "core.h"
#include "pcm_backend.h"
#include "pcm_decoder.h"
#include "cxaudio_log.h"

#include <algorithm>
#include <cctype>
#include <cstdio>

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

    const bool remote = (uri.rfind("http://", 0) == 0 ||
                         uri.rfind("https://", 0) == 0);
    const bool explicitSel = (!requested_.empty() && requested_ != "auto");

    /* 远程流（http/https）自动切换：faad 后端（miniaudio/faad2 链路）
     * 只支持本地文件（ma_decoder_init_file/fopen），须走 ffmpeg ——
     * FfmpegDecoder 的 avformat_open_input 内建 http/mp3 支持。
     * 显式选定 ffmpeg/mpv 时不介入（mpv 自带远程流能力同理）。 */
    if (target == "faad" && remote) target = "ffmpeg";

    backend_.reset();
    resolved_ = target;

    /* 候选序回退：auto 与“faad 被远程流强制切换”路径逐后端尝试，
     * 首个可用者胜出；用户显式指定单后端时保持不回退，失败如实上报。
     * 探测过程全景打印到 stderr，供排查后端可用性。 */
    static const char* const kOrder[3] = { "faad", "ffmpeg", "mpv" };
    int start = 0, end = 1;
    if (remote) {
        start = (target == "mpv") ? 2 : 1;  /* 显式 mpv 单候选；否则 ffmpeg 起 */
        end = explicitSel ? start + 1 : 3;
    } else if (explicitSel) {
        start = (target == "faad") ? 0 : (target == "ffmpeg") ? 1 : 2;
        end = start + 1;
    } else {
        end = 3;                            /* auto：全序 */
    }

    cbs_.onState = [this](int st) { dispatch(kEvState, st, 0, ""); };
    cbs_.onPosition = [this](int64_t pos, int64_t dur) {
        dispatch(kEvPosition, pos, dur, "");
    };
    cbs_.onEof = [this]() { dispatch(kEvEof, 0, 0, ""); };
    cbs_.onError = [this](const std::string& msg) {
        dispatch(kEvError, 0, 0, msg);
    };

    std::string lastErr;
    for (int i = start; i < end; ++i) {
        const std::string id = kOrder[i];
        if (id == "faad")
            backend_.reset(new PcmBackend(faadProvider()));
        else if (id == "ffmpeg")
            backend_.reset(new PcmBackend(ffmpegProvider()));
        else
            backend_.reset(createMpvBackend());

        if (!backend_) {
            lastErr = "backend not compiled: " + id;
            CXA_LOG("backend try=%s fail err=%s", id.c_str(), lastErr.c_str());
            continue;
        }
        backend_->setCallbacks(&cbs_);
        if (!pendingOutputDeviceName_.empty())
            backend_->setOutputDeviceName(pendingOutputDeviceName_);

        std::string err;
        if (backend_->available(&err) && backend_->open(uri, &err)) {
            resolved_ = id;
            CXA_LOG("backend try=%s ok", id.c_str());
            return true;
        }
        lastErr = err;
        CXA_LOG("backend try=%s fail err=%s", id.c_str(), err.c_str());
        backend_.reset();
    }

    std::lock_guard<std::mutex> lk(mutex_);
    lastError_ = lastErr.empty() ? std::string("no backend available") : lastErr;
    return false;
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
