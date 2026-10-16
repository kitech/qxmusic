#include "pcm_backend.h"
#include "audioutil.h"

#include <miniaudio.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>

namespace cx {

static const size_t kChunkFrames = 2048;
static const int kRingSeconds = 8;

PcmBackend::PcmBackend(DecoderProvider* provider) : provider_(provider) {}
PcmBackend::~PcmBackend() { teardown(); }

const char* PcmBackend::id() const { return provider_->id(); }

bool PcmBackend::available(std::string* err) { return provider_->available(err); }

void PcmBackend::dataCallback(ma_device* dev, void* out, const void* in,
                              unsigned int frameCount) {
    (void)in;
    PcmBackend* self = static_cast<PcmBackend*>(dev->pUserData);
    self->pullAudio(static_cast<int16_t*>(out), frameCount);
}

bool PcmBackend::open(const std::string& uri, std::string* err) {
    teardown();

    std::string path = normalizeUri(uri);
    decoder_ = provider_->create(path, err);
    if (!decoder_) return false;

    sampleRate_ = decoder_->sampleRate();
    channels_ = decoder_->channels();
    if (sampleRate_ <= 0) sampleRate_ = 44100;
    if (channels_ < 1) channels_ = 1;
    if (channels_ > 2) channels_ = 2;
    durationMs_ = decoder_->durationMs();

    ring_.assign((size_t)sampleRate_ * channels_ * kRingSeconds, 0);
    ringRead_ = ringWrite_ = ringCount_ = 0;
    framesPlayed_ = 0;
    eof_ = false;

    ma_device_config cfg = ma_device_config_init(ma_device_type_playback);
    cfg.playback.format = ma_format_s16;
    cfg.playback.channels = (ma_uint32)channels_;
    cfg.sampleRate = (ma_uint32)sampleRate_;
    cfg.dataCallback = &PcmBackend::dataCallback;
    cfg.pUserData = this;

    ma_context_config ccfg = ma_context_config_init();
    context_ = new ma_context;
    ma_result cr;
    if (std::getenv("CXAUDIO_NULL_DEVICE")) {
        ma_backend backends[1] = { ma_backend_null };
        cr = ma_context_init(backends, 1, &ccfg, context_);
    } else {
        cr = ma_context_init(NULL, 0, &ccfg, context_);
    }
    if (cr != MA_SUCCESS) {
        if (err) *err = std::string("ma_context_init failed: ") + ma_result_description(cr);
        delete context_;
        context_ = nullptr;
        delete decoder_;
        decoder_ = nullptr;
        return false;
    }

    device_ = new ma_device;
    ma_result r = ma_device_init(context_, &cfg, device_);
    if (r != MA_SUCCESS) {
        if (err) *err = std::string("ma_device_init failed: ") + ma_result_description(r);
        delete device_;
        device_ = nullptr;
        ma_context_uninit(context_);
        delete context_;
        context_ = nullptr;
        delete decoder_;
        decoder_ = nullptr;
        return false;
    }
    ma_device_set_master_volume(device_, 1.0f);
    ma_device_start(device_);

    state_ = kStopped;
    quit_ = false;
    thread_ = std::thread(&PcmBackend::decodeLoop, this);
    return true;
}

void PcmBackend::teardown() {
    quit_ = true;
    if (thread_.joinable()) thread_.join();
    if (device_) {
        ma_device_stop(device_);
        ma_device_uninit(device_);
        delete device_;
        device_ = nullptr;
    }
    if (context_) {
        ma_context_uninit(context_);
        delete context_;
        context_ = nullptr;
    }
    delete decoder_;
    decoder_ = nullptr;
    ring_.clear();
    ringRead_ = ringWrite_ = ringCount_ = 0;
    framesPlayed_ = 0;
    state_ = kStopped;
    sampleRate_ = channels_ = 0;
    durationMs_ = 0;
    eof_ = false;
}

void PcmBackend::decodeLoop() {
    std::vector<int16_t> tmp((size_t)kChunkFrames * (channels_ > 0 ? channels_ : 2));
    auto lastPos = std::chrono::steady_clock::now();
    while (!quit_) {
        if (state_.load() != kPlaying) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }
        {
            auto now = std::chrono::steady_clock::now();
            if (now - lastPos > std::chrono::milliseconds(200)) {
                lastPos = now;
                if (cb_ && cb_->onPosition) cb_->onPosition(position(), duration());
            }
        }
        if (eof_.load()) {
            bool empty;
            {
                std::lock_guard<std::mutex> lk(ringMutex_);
                empty = (ringCount_ == 0);
            }
            if (empty && !quit_) {
                state_ = kStopped;
                if (cb_ && cb_->onEof) cb_->onEof();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }

        size_t need = (size_t)kChunkFrames * channels_;
        size_t freeSamples;
        {
            std::lock_guard<std::mutex> lk(ringMutex_);
            freeSamples = ring_.size() - ringCount_;
        }
        if (freeSamples < need) {
            std::this_thread::sleep_for(std::chrono::milliseconds(3));
            continue;
        }

        size_t got;
        {
            std::lock_guard<std::mutex> lk(seekMutex_);
            if (quit_) break;
            got = decoder_->readFrames(tmp.data(), kChunkFrames);
        }
        if (got == 0) {
            eof_ = true;
            continue;
        }
        size_t samples = got * channels_;
        std::lock_guard<std::mutex> lk(ringMutex_);
        for (size_t i = 0; i < samples; ++i) {
            ring_[ringWrite_] = tmp[i];
            ringWrite_ = (ringWrite_ + 1) % ring_.size();
            ++ringCount_;
        }
    }
}

size_t PcmBackend::pullAudio(int16_t* out, size_t frames) {
    size_t want = frames * (size_t)channels_;
    int st = state_.load();
    if (st != kPlaying) {
        std::memset(out, 0, want * sizeof(int16_t));
        return frames;
    }
    std::lock_guard<std::mutex> lk(ringMutex_);
    size_t take = std::min(want, ringCount_);
    for (size_t i = 0; i < take; ++i) {
        out[i] = ring_[ringRead_];
        ringRead_ = (ringRead_ + 1) % ring_.size();
    }
    ringCount_ -= take;
    if (take < want)
        std::memset(out + take, 0, (want - take) * sizeof(int16_t));
    framesPlayed_ += (int64_t)(take / (size_t)channels_);
    return frames;
}

void PcmBackend::play() {
    if (!decoder_) return;
    if (eof_.load()) seek(0);
    eof_ = false;
    state_ = kPlaying;
    if (cb_ && cb_->onState) cb_->onState(kPlaying);
}

void PcmBackend::pause() {
    if (state_.load() == kPlaying) {
        state_ = kPaused;
        if (cb_ && cb_->onState) cb_->onState(kPaused);
    }
}

void PcmBackend::stop() {
    if (!decoder_) return;
    {
        std::lock_guard<std::mutex> lk(seekMutex_);
        decoder_->seek(0);
    }
    {
        std::lock_guard<std::mutex> lk(ringMutex_);
        ringRead_ = ringWrite_ = ringCount_ = 0;
    }
    framesPlayed_ = 0;
    eof_ = false;
    state_ = kStopped;
    if (cb_ && cb_->onState) cb_->onState(kStopped);
}

void PcmBackend::seek(int64_t ms) {
    if (!decoder_) return;
    if (ms < 0) ms = 0;
    {
        std::lock_guard<std::mutex> lk(seekMutex_);
        decoder_->seek(ms);
    }
    {
        std::lock_guard<std::mutex> lk(ringMutex_);
        ringRead_ = ringWrite_ = ringCount_ = 0;
    }
    framesPlayed_ = sampleRate_ > 0 ? ms * sampleRate_ / 1000 : 0;
    eof_ = false;
}

void PcmBackend::setVolume(int volume) {
    if (volume < 0) volume = 0;
    if (volume > 100) volume = 100;
    if (device_) ma_device_set_master_volume(device_, volume / 100.0f);
}

int64_t PcmBackend::position() const {
    if (sampleRate_ <= 0) return 0;
    return framesPlayed_.load() * 1000 / sampleRate_;
}

int64_t PcmBackend::duration() const { return durationMs_.load(); }

int PcmBackend::state() const { return state_.load(); }

} /* namespace cx */
