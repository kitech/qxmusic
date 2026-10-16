#ifndef CXAUDIO_PCM_DECODER_H
#define CXAUDIO_PCM_DECODER_H

#include <cstddef>
#include <cstdint>
#include <string>

namespace cx {

/* Produces interleaved signed 16-bit PCM at a fixed rate/channels. */
class Decoder {
public:
    virtual ~Decoder() {}

    virtual int sampleRate() const = 0;
    virtual int channels() const = 0;
    virtual int64_t durationMs() const = 0;

    /* Fill up to `frames` frames; returns frames actually written (0 = EOF). */
    virtual size_t readFrames(int16_t* dst, size_t frames) = 0;

    /* Best-effort seek; returns true on success. */
    virtual bool seek(int64_t ms) = 0;

    virtual const char* name() const = 0;
};

/* A decoder family. Providers are process-wide singletons. */
class DecoderProvider {
public:
    virtual ~DecoderProvider() {}
    virtual const char* id() const = 0;
    virtual bool available(std::string* err) = 0;
    virtual Decoder* create(const std::string& path, std::string* err) = 0;
};

DecoderProvider* faadProvider();
DecoderProvider* ffmpegProvider();

} /* namespace cx */

#endif /* CXAUDIO_PCM_DECODER_H */
