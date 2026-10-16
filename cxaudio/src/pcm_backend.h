#ifndef CXAUDIO_PCM_BACKEND_H
#define CXAUDIO_PCM_BACKEND_H

#include "cxcommon.h"
#include "pcm_decoder.h"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

struct ma_device;
struct ma_context;

namespace cx {

/* Shared backend for A (ffmpeg) and B (faad): decode thread -> ring -> ma_device. */
class PcmBackend : public IBackend {
public:
    explicit PcmBackend(DecoderProvider* provider);
    ~PcmBackend() override;

    const char* id() const override;
    bool available(std::string* err) override;
    bool open(const std::string& uri, std::string* err) override;

    void play() override;
    void pause() override;
    void stop() override;
    void seek(int64_t ms) override;
    void setVolume(int volume) override;

    int64_t position() const override;
    int64_t duration() const override;
    int state() const override;

private:
    PcmBackend(const PcmBackend&);
    PcmBackend& operator=(const PcmBackend&);

    void decodeLoop();
    void teardown();
    size_t pullAudio(int16_t* out, size_t frames);
    static void dataCallback(ma_device* dev, void* out, const void* in,
                             unsigned int frameCount);

    DecoderProvider* provider_;
    Decoder* decoder_ = nullptr;
    ma_device* device_ = nullptr;
    ma_context* context_ = nullptr;

    int sampleRate_ = 0;
    int channels_ = 0;

    std::thread thread_;
    std::atomic<bool> quit_{false};
    std::atomic<int> state_{kStopped};
    std::atomic<int64_t> framesPlayed_{0};
    std::atomic<int64_t> durationMs_{0};
    std::atomic<bool> eof_{false};

    std::mutex ringMutex_;
    std::vector<int16_t> ring_;
    size_t ringRead_ = 0;
    size_t ringWrite_ = 0;
    size_t ringCount_ = 0;
    std::mutex seekMutex_;
};

} /* namespace cx */

#endif /* CXAUDIO_PCM_BACKEND_H */
