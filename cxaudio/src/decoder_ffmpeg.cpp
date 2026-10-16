#include "pcm_decoder.h"
#include "audioutil.h"

#if defined(__has_include)
#  if __has_include(<libavformat/avformat.h>) && __has_include(<libavcodec/avcodec.h>) && \
      __has_include(<libswresample/swresample.h>)
#    include "ffmpegapi.h"
#    if LIBAVCODEC_VERSION_MAJOR >= 59 && LIBAVUTIL_VERSION_MAJOR >= 57 && \
        LIBSWRESAMPLE_VERSION_MAJOR >= 4
#      define CXAUDIO_FFMPEG_OK 1
#    endif
#  endif
#endif

#if CXAUDIO_FFMPEG_OK

#include <cstring>
#include <vector>

namespace cx {
namespace {

FfmpegApi* sharedApi(std::string* err = 0);

class FfmpegDecoder : public Decoder {
public:
    static Decoder* openFile(const std::string& path, std::string* err) {
        FfmpegDecoder* d = new FfmpegDecoder();
        if (!d->init(path, err)) {
            delete d;
            return 0;
        }
        return d;
    }

    ~FfmpegDecoder() override { cleanup(); }

    int sampleRate() const override { return outRate_; }
    int channels() const override { return outCh_; }
    int64_t durationMs() const override { return durationMs_; }

    size_t readFrames(int16_t* dst, size_t frames) override {
        size_t want = frames * (size_t)outCh_;
        size_t produced = 0;
        while (produced < want) {
            if (stagePos_ >= stage_.size()) {
                stage_.clear();
                stagePos_ = 0;
                if (!decodeNextFrame()) break;
            }
            size_t avail = stage_.size() - stagePos_;
            size_t take = want - produced;
            if (take > avail) take = avail;
            std::memcpy(dst + produced, stage_.data() + stagePos_, take * sizeof(int16_t));
            stagePos_ += take;
            produced += take;
        }
        return produced / (size_t)outCh_;
    }

    bool seek(int64_t ms) override {
        if (!fmt_ || streamIndex_ < 0) return false;
        AVRational msTb = {1, 1000};
        AVRational stTb = fmt_->streams[streamIndex_]->time_base;
        int64_t ts = api_->av_rescale_q(ms, msTb, stTb);
        int r = api_->avformat_seek_file(fmt_, streamIndex_, INT64_MIN, ts, INT64_MAX, 0);
        if (r < 0) return false;
        api_->avcodec_flush_buffers(codec_);
        stage_.clear();
        stagePos_ = 0;
        draining_ = false;
        needSwr_ = true;
        return true;
    }

    const char* name() const override { return "ffmpeg"; }

private:
    FfmpegDecoder() {}
    FfmpegDecoder(const FfmpegDecoder&);
    FfmpegDecoder& operator=(const FfmpegDecoder&);

    void cleanup() {
        if (frame_) api_->av_frame_free(&frame_);
        if (pkt_) api_->av_packet_free(&pkt_);
        if (swr_) api_->swr_free(&swr_);
        if (codec_) api_->avcodec_free_context(&codec_);
        if (fmt_) api_->avformat_close_input(&fmt_);
    }

    std::string errStr(int r) {
        char buf[256];
        buf[0] = 0;
        if (api_ && api_->av_strerror) api_->av_strerror(r, buf, sizeof(buf));
        return std::string(buf);
    }

    bool init(const std::string& path, std::string* err) {
        api_ = sharedApi();
        if (!api_ || !api_->valid()) {
            if (err) *err = "libav* unavailable";
            return false;
        }
        int r = api_->avformat_open_input(&fmt_, path.c_str(), 0, 0);
        if (r < 0) {
            if (err) *err = "open_input: " + errStr(r);
            return false;
        }
        r = api_->avformat_find_stream_info(fmt_, 0);
        if (r < 0) {
            if (err) *err = "find_stream_info: " + errStr(r);
            return false;
        }
        const AVCodec* dec = 0;
        streamIndex_ = api_->av_find_best_stream(fmt_, AVMEDIA_TYPE_AUDIO, -1, -1, &dec, 0);
        if (streamIndex_ < 0) {
            if (err) *err = "no audio stream";
            return false;
        }
        AVStream* st = fmt_->streams[streamIndex_];
        codec_ = api_->avcodec_alloc_context3(dec);
        if (!codec_) {
            if (err) *err = "alloc codec failed";
            return false;
        }
        api_->avcodec_parameters_to_context(codec_, st->codecpar);
        r = api_->avcodec_open2(codec_, dec, 0);
        if (r < 0) {
            if (err) *err = "codec_open2: " + errStr(r);
            return false;
        }

        outRate_ = codec_->sample_rate > 0 ? codec_->sample_rate : 44100;
        outCh_ = codec_->ch_layout.nb_channels;
        if (outCh_ < 1) outCh_ = 1;
        if (outCh_ > 2) outCh_ = 2;

        if (st->duration > 0) {
            durationMs_ = api_->av_rescale_q(st->duration, st->time_base, AVRational{1, 1000});
        } else if (fmt_->duration > 0) {
            durationMs_ = fmt_->duration / 1000;
        }

        pkt_ = api_->av_packet_alloc();
        frame_ = api_->av_frame_alloc();
        if (!pkt_ || !frame_) {
            if (err) *err = "alloc packet/frame failed";
            return false;
        }
        return true;
    }

    bool initSwr(const AVFrame* f) {
        AVChannelLayout outLayout;
        api_->av_channel_layout_default(&outLayout, outCh_);
#if LIBAVUTIL_VERSION_MAJOR >= 57
        const AVChannelLayout* inLayout = &f->ch_layout;
#else
        AVChannelLayout inLegacy;
        api_->av_channel_layout_default(&inLegacy, f->channels);
        const AVChannelLayout* inLayout = &inLegacy;
#endif
        int r = api_->swr_alloc_set_opts2(&swr_, &outLayout, AV_SAMPLE_FMT_S16, outRate_,
                                          inLayout, (enum AVSampleFormat)f->format,
                                          f->sample_rate, 0, 0);
        if (r < 0 || api_->swr_init(swr_) < 0) return false;
        inRate_ = f->sample_rate > 0 ? f->sample_rate : outRate_;
        return true;
    }

    bool convertFrame(const AVFrame* f) {
        if (needSwr_ || !swr_) {
            if (swr_) api_->swr_free(&swr_);
            if (!initSwr(f)) return false;
            needSwr_ = false;
        }
        int outMax = (int)((int64_t)f->nb_samples * outRate_ / (inRate_ > 0 ? inRate_ : outRate_)) +
                     256;
        if (outMax < 1) outMax = 1;
        stage_.resize((size_t)outMax * (size_t)outCh_);
        uint8_t* outPtr = (uint8_t*)stage_.data();
        int got = api_->swr_convert(swr_, &outPtr, outMax, (const uint8_t**)f->data,
                                    f->nb_samples);
        if (got < 0) {
            stage_.clear();
            return false;
        }
        stage_.resize((size_t)got * (size_t)outCh_);
        stagePos_ = 0;
        return got > 0;
    }

    bool decodeNextFrame() {
        for (;;) {
            int r = api_->avcodec_receive_frame(codec_, frame_);
            if (r == 0) {
                bool ok = convertFrame(frame_);
                api_->av_frame_unref(frame_);
                if (ok) return true;
                continue;
            }
            if (r != AVERROR(EAGAIN) && r != AVERROR_EOF) return false;

            if (draining_) return false;

            int pr = api_->av_read_frame(fmt_, pkt_);
            if (pr < 0) {
                api_->avcodec_send_packet(codec_, 0);
                draining_ = true;
                continue;
            }
            if (pkt_->stream_index == streamIndex_)
                api_->avcodec_send_packet(codec_, pkt_);
            api_->av_packet_unref(pkt_);
        }
    }

    FfmpegApi* api_ = 0;
    AVFormatContext* fmt_ = 0;
    AVCodecContext* codec_ = 0;
    SwrContext* swr_ = 0;
    AVPacket* pkt_ = 0;
    AVFrame* frame_ = 0;
    int streamIndex_ = -1;
    int outRate_ = 0;
    int outCh_ = 0;
    int inRate_ = 0;
    int64_t durationMs_ = 0;
    bool needSwr_ = true;
    bool draining_ = false;
    std::vector<int16_t> stage_;
    size_t stagePos_ = 0;
};

FfmpegApi* sharedApi(std::string* err) {
    static FfmpegApi api;
    static bool tried = false;
    static bool ok = false;
    static std::string loadErr;
    if (!tried) {
        tried = true;
        ok = loadFfmpegApi(&api, &loadErr);
    }
    if (!ok && err) *err = loadErr.empty() ? "libavformat not found" : loadErr;
    return ok ? &api : 0;
}

class FfmpegProvider : public DecoderProvider {
public:
    const char* id() const override { return "ffmpeg"; }
    bool available(std::string* err) override {
        return sharedApi(err) != 0;
    }
    Decoder* create(const std::string& path, std::string* err) override {
        return FfmpegDecoder::openFile(path, err);
    }
};

} /* namespace */

DecoderProvider* ffmpegProvider() {
    static FfmpegProvider provider;
    return &provider;
}

} /* namespace cx */

#else /* no FFmpeg headers / too old */

namespace cx {
namespace {
class FfmpegProvider : public DecoderProvider {
public:
    const char* id() const override { return "ffmpeg"; }
    bool available(std::string* err) override {
        if (err) *err = "ffmpeg backend not built (headers missing/too old)";
        return false;
    }
    Decoder* create(const std::string&, std::string* err) override {
        if (err) *err = "ffmpeg backend not built (headers missing/too old)";
        return 0;
    }
};
} /* namespace */
DecoderProvider* ffmpegProvider() {
    static FfmpegProvider provider;
    return &provider;
}
} /* namespace cx */

#endif
