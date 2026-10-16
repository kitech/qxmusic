#include "core.h"
#include "mpvapi.h"

#include <mpv/client.h>

#include <atomic>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>

namespace cx {

class MpvBackend : public IBackend {
public:
    MpvBackend() {}
    ~MpvBackend() override {
        quit_ = true;
        if (thread_.joinable()) thread_.join();
        if (handle_ && api_.terminate_destroy) api_.terminate_destroy(handle_);
        handle_ = 0;
    }

    const char* id() const override { return "mpv"; }

    bool available(std::string* err) override { return ensureInit(err); }

    bool open(const std::string& uri, std::string* err) override {
        if (!ensureInit(err)) return false;
        state_ = kBuffering;
        if (cb_ && cb_->onState) cb_->onState(kBuffering);
        const char* cmd[] = {"loadfile", uri.c_str(), 0};
        std::lock_guard<std::mutex> lk(cmdMutex_);
        int r = api_.command(handle_, cmd);
        if (r < 0) {
            if (err) *err = api_.error_string ? api_.error_string(r) : "loadfile failed";
            return false;
        }
        loadedFile_ = true;
        return true;
    }

    void play() override {
        if (!handle_) return;
        int no = 0;
        std::lock_guard<std::mutex> lk(cmdMutex_);
        api_.set_property(handle_, "pause", MPV_FORMAT_FLAG, &no);
        state_ = kPlaying;
        if (cb_ && cb_->onState) cb_->onState(kPlaying);
    }

    void pause() override {
        if (!handle_) return;
        int yes = 1;
        std::lock_guard<std::mutex> lk(cmdMutex_);
        api_.set_property(handle_, "pause", MPV_FORMAT_FLAG, &yes);
        state_ = kPaused;
        if (cb_ && cb_->onState) cb_->onState(kPaused);
    }

    void stop() override {
        if (!handle_) return;
        const char* cmd[] = {"stop", 0};
        std::lock_guard<std::mutex> lk(cmdMutex_);
        api_.command(handle_, cmd);
        posMs_ = 0;
        loadedFile_ = false;
        state_ = kStopped;
        if (cb_ && cb_->onState) cb_->onState(kStopped);
    }

    void seek(int64_t ms) override {
        if (!handle_) return;
        double s = ms / 1000.0;
        std::lock_guard<std::mutex> lk(cmdMutex_);
        api_.set_property(handle_, "time-pos", MPV_FORMAT_DOUBLE, &s);
    }

    void setVolume(int volume) override {
        if (volume < 0) volume = 0;
        if (volume > 100) volume = 100;
        volume_ = volume;
        if (!handle_) return;
        double v = volume;
        std::lock_guard<std::mutex> lk(cmdMutex_);
        api_.set_property(handle_, "volume", MPV_FORMAT_DOUBLE, &v);
    }

    int64_t position() const override { return posMs_.load(); }
    int64_t duration() const override { return durMs_.load(); }
    int state() const override { return state_.load(); }

private:
    MpvBackend(const MpvBackend&);
    MpvBackend& operator=(const MpvBackend&);

    bool ensureInit(std::string* err) {
        if (loaded_) return true;
        std::lock_guard<std::mutex> lk(cmdMutex_);
        if (loaded_) return true;
        std::string path;
        if (!loadMpvApi(&api_, &path, err)) return false;
        handle_ = api_.create();
        if (!handle_) {
            if (err) *err = "mpv_create failed";
            return false;
        }
        api_.set_option_string(handle_, "vo", "null");
        api_.set_option_string(handle_, "vid", "no");
        api_.set_option_string(handle_, "audio-display", "no");
        api_.set_option_string(handle_, "idle", "yes");
        api_.set_option_string(handle_, "terminal", "no");
        api_.set_option_string(handle_, "keep-open", "no");
        int r = api_.initialize(handle_);
        if (r < 0) {
            if (err) *err = api_.error_string ? api_.error_string(r) : "mpv_initialize failed";
            api_.terminate_destroy(handle_);
            handle_ = 0;
            return false;
        }
        api_.observe_property(handle_, 1, "time-pos", MPV_FORMAT_DOUBLE);
        api_.observe_property(handle_, 2, "duration", MPV_FORMAT_DOUBLE);
        api_.observe_property(handle_, 3, "pause", MPV_FORMAT_FLAG);
        api_.observe_property(handle_, 4, "eof-reached", MPV_FORMAT_FLAG);
        {
            double v = volume_.load();
            api_.set_property(handle_, "volume", MPV_FORMAT_DOUBLE, &v);
        }
        loaded_ = true;
        quit_ = false;
        thread_ = std::thread(&MpvBackend::eventLoop, this);
        return true;
    }

    void eventLoop() {
        while (!quit_) {
            mpv_event* ev = api_.wait_event(handle_, 0.2);
            if (!ev) continue;
            switch (ev->event_id) {
            case MPV_EVENT_PROPERTY_CHANGE: {
                mpv_event_property* p = (mpv_event_property*)ev->data;
                if (!p || !p->data) break;
                if (!std::strcmp(p->name, "time-pos") && p->format == MPV_FORMAT_DOUBLE) {
                    posMs_ = (int64_t)(*(double*)p->data * 1000.0);
                    if (cb_ && cb_->onPosition) cb_->onPosition(posMs_.load(), durMs_.load());
                } else if (!std::strcmp(p->name, "duration") &&
                           p->format == MPV_FORMAT_DOUBLE) {
                    durMs_ = (int64_t)(*(double*)p->data * 1000.0);
                    if (cb_ && cb_->onPosition) cb_->onPosition(posMs_.load(), durMs_.load());
                } else if (!std::strcmp(p->name, "pause") && p->format == MPV_FORMAT_FLAG) {
                    bool paused = (*(int*)p->data) != 0;
                    if (loadedFile_) {
                        state_ = paused ? kPaused : kPlaying;
                        if (cb_ && cb_->onState) cb_->onState(state_.load());
                    }
                } else if (!std::strcmp(p->name, "eof-reached") &&
                           p->format == MPV_FORMAT_FLAG) {
                    if (*(int*)p->data) {
                        state_ = kStopped;
                        if (cb_ && cb_->onEof) cb_->onEof();
                        if (cb_ && cb_->onState) cb_->onState(kStopped);
                    }
                }
                break;
            }
            case MPV_EVENT_FILE_LOADED: {
                loadedFile_ = true;
                double d = 0;
                if (api_.get_property(handle_, "duration", MPV_FORMAT_DOUBLE, &d) == 0)
                    durMs_ = (int64_t)(d * 1000.0);
                break;
            }
            case MPV_EVENT_END_FILE: {
                loadedFile_ = false;
                state_ = kStopped;
                if (cb_ && cb_->onState) cb_->onState(kStopped);
                break;
            }
            case MPV_EVENT_SHUTDOWN: {
                quit_ = true;
                break;
            }
            default:
                break;
            }
        }
    }

    MpvApi api_;
    bool loaded_ = false;
    mpv_handle* handle_ = 0;
    std::thread thread_;
    std::atomic<bool> quit_{false};
    std::atomic<int64_t> posMs_{0};
    std::atomic<int64_t> durMs_{0};
    std::atomic<int> state_{kStopped};
    std::atomic<int> volume_{100};
    bool loadedFile_ = false;
    std::mutex cmdMutex_;
};

IBackend* createMpvBackend() { return new MpvBackend(); }

} /* namespace cx */
