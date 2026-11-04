#include "pcm_backend.h"
#include "audioutil.h"
#include "cxaudio_log.h"

#include <miniaudio.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace cx {

static const size_t kChunkFrames = 2048;
static const int kRingSeconds = 8;

/* 观测计数（open() 时重置；decodeLoop/音频回调线程写，无并发打点冲突） */
static bool gLoggedEmpty = false;
static bool gLoggedData = false;
static int gRead0Streak = 0;
static bool gPullLogged = false;

/* NULL_DEVICE 环境变量 + ALSA→Pulse→默认 后端序（open 与设备枚举共用；
 * 两处必须同一选择逻辑，避免"枚举列表"与"实际打开"错位）。
 * 优先 ALSA：实测 PulseAudio 后端 ma_device_start 返回 0 但数据回调
 * 不运转（sink SUSPENDED、pullAudio 无 take）；均失败回退默认序 */
static bool initMaContext(ma_context* ctx, ma_context_config* ccfg,
                          std::string* err) {
    ma_result cr;
    if (std::getenv("CXAUDIO_NULL_DEVICE")) {
        ma_backend backends[1] = { ma_backend_null };
        cr = ma_context_init(backends, 1, ccfg, ctx);
    } else {
        ma_backend backends[2] = { ma_backend_alsa, ma_backend_pulseaudio };
        cr = ma_context_init(backends, 2, ccfg, ctx);
        if (cr != MA_SUCCESS)
            cr = ma_context_init(NULL, 0, ccfg, ctx);
    }
    if (cr != MA_SUCCESS) {
        if (err)
            *err = std::string("ma_context_init failed: ") + ma_result_description(cr);
        return false;
    }
    return true;
}

PcmBackend::PcmBackend(DecoderProvider* provider, const std::string& providerTag)
    : provider_(provider), m_providerTag(providerTag) {}
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

    gLoggedEmpty = gLoggedData = gPullLogged = false;
    gRead0Streak = 0;

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
    if (!initMaContext(context_, &ccfg, err)) {
        delete context_;
        context_ = nullptr;
        delete decoder_;
        decoder_ = nullptr;
        return false;
    }

    /* 选定输出设备（设置窗记忆；空=系统默认）；名字精确匹配 */
    ma_device_id preferredDeviceId_;
    if (!outputDeviceName_.empty()) {
        ma_device_info* infos = nullptr;
        ma_uint32 cnt = 0;
        if (ma_context_get_devices(context_, &infos, &cnt, 0, 0) == MA_SUCCESS) {
            for (ma_uint32 i = 0; i < cnt; ++i) {
                if (std::strcmp(infos[i].name, outputDeviceName_.c_str()) == 0) {
                    preferredDeviceId_ = infos[i].id;
                    cfg.playback.pDeviceID = &preferredDeviceId_; /* init 期间有效 */
                    CXA_LOG("device select: %s", outputDeviceName_.c_str());
                    break;
                }
            }
        }
        if (!cfg.playback.pDeviceID)
            CXA_LOG("device not found, fallback default: %s", outputDeviceName_.c_str());
    }

    device_ = new ma_device;
    ma_result r = ma_device_init(context_, &cfg, device_);
    if (r != MA_SUCCESS) {
        if (err) *err = std::string("ma_device_init failed: ") + ma_result_description(r);
        CXA_LOG("device init failed: r=%d %s (ctx=%s)", (int)r,
                ma_result_description(r), ma_get_backend_name(context_->backend));
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
    r = ma_device_start(device_);
    CXA_LOG("device backend=%s provider=%s sr=%d ch=%d start=%d (%s)",
            ma_get_backend_name(context_->backend), m_providerTag.c_str(),
            sampleRate_, channels_, (int)r, ma_result_description(r));

    state_ = kStopped;
    quit_ = false;
    thread_ = std::thread(&PcmBackend::decodeLoop, this);
    return true;
}

std::vector<AudioDeviceInfo> PcmBackend::listOutputDevices() {
    std::vector<AudioDeviceInfo> out;
    ma_context ctx;
    ma_context_config ccfg = ma_context_config_init();
    if (!initMaContext(&ctx, &ccfg, nullptr)) return out;
    ma_device_info* infos = nullptr;
    ma_uint32 cnt = 0;
    if (ma_context_get_devices(&ctx, &infos, &cnt, 0, 0) == MA_SUCCESS) {
        for (ma_uint32 i = 0; i < cnt; ++i) {
            AudioDeviceInfo d;
            d.name = infos[i].name;
            d.isDefault = (i == 0); /* ma 默认序：首个为系统默认 */
            out.push_back(d);
        }
    }
    ma_context_uninit(&ctx);
    return out;
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
        {
            /* 每实例观测一次 + 累计空读连续计数（displayed at open()） */
            if (got == 0) {
                ++gRead0Streak;
                if (gRead0Streak == 1 || gRead0Streak == 300 /*~1s*/) {
                    CXA_LOG("decode read-0 streak=%d eof=%d",
                            gRead0Streak, (int)eof_.load());
                }
                if (!gLoggedEmpty && !eof_.load()) {
                    gLoggedEmpty = true;
                    CXA_LOG("decode first-empty");
                }
            } else {
                gRead0Streak = 0;
                if (!gLoggedData) {
                    gLoggedData = true;
                    CXA_LOG("decode flowing: got=%zu chunk frames", got);
                }
            }
        }
        if (got == 0) {
            std::string de = decoder_->lastError();
            if (!de.empty()) {
                CXA_LOG("decode error: %s", de.c_str());
                if (cb_ && cb_->onError) cb_->onError(de);
                state_ = kStopped;
            }
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
        /* 每秒快照一次 ring 水位（与 pullAudio live 行对照） */
        {
            static auto lastRing = std::chrono::steady_clock::now();
            auto now = std::chrono::steady_clock::now();
            if (now - lastRing > std::chrono::seconds(1)) {
                lastRing = now;
                CXA_LOG("decode ring: count=%zu write=%zu got=%zu",
                        ringCount_, ringWrite_, got);
            }
        }
    }
}

size_t PcmBackend::pullAudio(int16_t* out, size_t frames) {
    size_t want = frames * (size_t)channels_;
    int st = state_.load();
    if (st != kPlaying) {
        std::memset(out, 0, want * sizeof(int16_t));
        /* 每秒打一次"非播放态截流"明细：设备回调在跑但被 state 挡住 */
        {
            static auto lastBlocked = std::chrono::steady_clock::now();
            auto now = std::chrono::steady_clock::now();
            if (now - lastBlocked > std::chrono::seconds(1)) {
                lastBlocked = now;
                CXA_LOG("pullAudio blocked: st=%d (not playing) "
                        "ringCount=%zu framesPlayed=%lld",
                        st, ringCount_, (long long)framesPlayed_.load());
            }
        }
        return frames;
    }
    std::lock_guard<std::mutex> lk(ringMutex_);
    size_t take = std::min(want, ringCount_);
    /* 播放态下每秒打一次明细（增强一次性的 pullAudio flowing） */
    {
        static auto lastLive = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        if (now - lastLive > std::chrono::seconds(1)) {
            lastLive = now;
            CXA_LOG("pullAudio live: st=%d take=%zu want=%zu "
                    "ringCount=%zu framesPlayed=%lld",
                    st, take, want, ringCount_, (long long)framesPlayed_.load());
        }
    }
    for (size_t i = 0; i < take; ++i) {
        out[i] = ring_[ringRead_];
        ringRead_ = (ringRead_ + 1) % ring_.size();
    }
    ringCount_ -= take;
    if (take < want)
        std::memset(out + take, 0, (want - take) * sizeof(int16_t));
    framesPlayed_ += (int64_t)(take / (size_t)channels_);
    if (take > 0 && !gPullLogged) {
        gPullLogged = true;
        CXA_LOG("pullAudio flowing: take=%zu of want=%zu", take, want);
    }
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
