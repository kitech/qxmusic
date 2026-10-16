/* 64-bit file offsets: on 32-bit targets (e.g. Android armeabi-v7a) `long` is
 * 32 bits and std::fseek/ftell silently truncate offsets past 2 GiB. Must be
 * defined before any system header is included. */
#ifndef _FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 64
#endif

#include "pcm_decoder.h"
#include "audioutil.h"
#include "stb_vorbis_iface.h"

#include <miniaudio.h>
extern "C" {
#include <minimp4.h>
}
#include <neaacdec.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#if !defined(_WIN32)
#include <sys/types.h>
#endif

namespace cx {
namespace {

#if defined(_WIN32)
static int fcSeek(FILE* f, int64_t off, int whence) { return _fseeki64(f, off, whence); }
static int64_t fcTell(FILE* f) { return _ftelli64(f); }
#else
static int fcSeek(FILE* f, int64_t off, int whence) { return fseeko(f, (off_t)off, whence); }
static int64_t fcTell(FILE* f) { return (int64_t)ftello(f); }
#endif

/* Parse the raw AudioSpecificConfig (ASC/DecoderSpecificInfo): validate it and
 * extract the AAC object type so SBR/PS can be recognized and unsupported
 * profiles rejected early. */
static bool parseAsc(const unsigned char* dsi, unsigned bytes, int* aot, int* sfIndex) {
    if (!dsi || bytes < 2) return false;
    int a = dsi[0] >> 3;                                   /* 5-bit audioObjectType */
    int sf = ((dsi[0] & 0x07) << 1) | (dsi[1] >> 7);       /* 4-bit samplingFrequencyIndex */
    if (a == 31) {                                         /* escape: extended AOT */
        if (bytes < 3) return false;
        a = 32 + (((dsi[0] & 0x07) << 3) | (dsi[1] >> 5));
    }
    if (aot) *aot = a;
    if (sfIndex) *sfIndex = sf;
    return true;
}

/* ---------------- WAV / FLAC / MP3 via miniaudio ---------------- */
class MaDecoder : public Decoder {
public:
    static Decoder* openFile(const std::string& path, std::string* err) {
        MaDecoder* d = new MaDecoder();
        ma_decoder_config cfg = ma_decoder_config_init(ma_format_s16, 0, 0);
        ma_result r = ma_decoder_init_file(path.c_str(), &cfg, &d->dec_);
        if (r != MA_SUCCESS) {
            if (err) *err = std::string("ma_decoder_init_file: ") + ma_result_description(r);
            delete d;
            return 0;
        }
        d->init_ = true;
        return d;
    }
    ~MaDecoder() override {
        if (init_) ma_decoder_uninit(&dec_);
    }

    int sampleRate() const override { return (int)dec_.outputSampleRate; }
    int channels() const override { return (int)dec_.outputChannels; }
    int64_t durationMs() const override {
        ma_uint64 len = 0;
        ma_decoder_get_length_in_pcm_frames((ma_decoder*)&dec_, &len);
        if (len == 0 || dec_.outputSampleRate == 0) return 0;
        return (int64_t)(len * 1000 / dec_.outputSampleRate);
    }
    size_t readFrames(int16_t* dst, size_t frames) override {
        ma_uint64 got = 0;
        if (ma_decoder_read_pcm_frames(&dec_, dst, (ma_uint64)frames, &got) != MA_SUCCESS)
            return 0;
        return (size_t)got;
    }
    bool seek(int64_t ms) override {
        if (dec_.outputSampleRate == 0) return false;
        ma_uint64 f = (ma_uint64)(ms * dec_.outputSampleRate / 1000);
        return ma_decoder_seek_to_pcm_frame(&dec_, f) == MA_SUCCESS;
    }
    const char* name() const override { return "miniaudio"; }

private:
    MaDecoder() { std::memset(&dec_, 0, sizeof(dec_)); }
    ma_decoder dec_;
    bool init_ = false;
};

/* ---------------- OGG/Vorbis via stb_vorbis ---------------- */
class VorbisDecoder : public Decoder {
public:
    static Decoder* openFile(const std::string& path, std::string* err) {
        VorbisDecoder* d = new VorbisDecoder();
        if (!readFile(path, &d->data_)) {
            if (err) *err = "read failed";
            delete d;
            return 0;
        }
        int e = 0;
        d->v_ = stb_vorbis_open_memory(d->data_.data(), (int)d->data_.size(), &e, 0);
        if (!d->v_) {
            if (err) *err = "stb_vorbis_open_memory failed";
            delete d;
            return 0;
        }
        stb_vorbis_info info = stb_vorbis_get_info(d->v_);
        d->ch_ = info.channels;
        d->rate_ = info.sample_rate;
        d->total_ = stb_vorbis_stream_length_in_samples(d->v_);
        return d;
    }
    ~VorbisDecoder() override {
        if (v_) stb_vorbis_close(v_);
    }

    int sampleRate() const override { return rate_; }
    int channels() const override { return ch_; }
    int64_t durationMs() const override {
        if (rate_ <= 0) return 0;
        return (int64_t)((uint64_t)total_ * 1000 / rate_);
    }
    size_t readFrames(int16_t* dst, size_t frames) override {
        int n = stb_vorbis_get_samples_short_interleaved(
            v_, ch_, dst, (int)(frames * (size_t)ch_));
        return n < 0 ? 0 : (size_t)n;
    }
    bool seek(int64_t ms) override {
        if (rate_ <= 0) return false;
        return stb_vorbis_seek(v_, (unsigned)(ms * rate_ / 1000)) != 0;
    }
    const char* name() const override { return "stb_vorbis"; }

private:
    VorbisDecoder() {}
    std::vector<uint8_t> data_;
    stb_vorbis* v_ = 0;
    int ch_ = 0;
    int rate_ = 0;
    unsigned total_ = 0;
};

/* ---------------- AAC (raw ADTS) / M4A via FAAD2 + minimp4 ---------------- */
class FaadDecoder : public Decoder {
public:
    static Decoder* openFile(const std::string& path, FormatKind kind, std::string* err) {
        FaadDecoder* d = new FaadDecoder();
        d->kind_ = kind;
        d->h_ = NeAACDecOpen();
        if (!d->h_) {
            if (err) *err = "NeAACDecOpen failed";
            delete d;
            return 0;
        }
        NeAACDecConfigurationPtr c = NeAACDecGetCurrentConfiguration(d->h_);
        if (c) {
            c->outputFormat = FAAD_FMT_16BIT;
            c->downMatrix = 1;
            NeAACDecSetConfiguration(d->h_, c);
        }
        bool ok = (kind == kFmtM4a) ? d->initMp4(path, err) : d->initAdts(path, err);
        if (!ok) {
            delete d;
            return 0;
        }
        /* Prime: decode the first frame to learn the true output rate/channels
         * (matters for HE-AAC/SBR where rate doubles). */
        if (!d->fillPending() && d->pending_.empty()) {
            if (err) *err = "no decodable AAC frame";
            delete d;
            return 0;
        }
        return d;
    }
    ~FaadDecoder() override {
        if (f_) std::fclose(f_);
        if (mp4Open_) MP4D_close(&mp4_);
        if (h_) NeAACDecClose(h_);
    }

    int sampleRate() const override { return rate_; }
    int channels() const override { return ch_; }
    int64_t durationMs() const override { return durationMs_; }

    size_t readFrames(int16_t* dst, size_t frames) override {
        size_t produced = 0;
        size_t want = frames * (size_t)(ch_ > 0 ? ch_ : 2);
        while (produced < want) {
            if (pendPos_ >= pending_.size()) {
                pending_.clear();
                pendPos_ = 0;
                if (!fillPending()) break;
            }
            size_t avail = pending_.size() - pendPos_;
            size_t take = want - produced;
            if (take > avail) take = avail;
            std::memcpy(dst + produced, pending_.data() + pendPos_, take * sizeof(int16_t));
            pendPos_ += take;
            produced += take;
        }
        if (ch_ <= 0) return 0;
        return produced / (size_t)ch_;
    }

    bool seek(int64_t ms) override {
        pending_.clear();
        pendPos_ = 0;
        if (kind_ == kFmtM4a && mp4Open_ && timescale_ > 0 && timestampCount_ > 0) {
            uint64_t target = (uint64_t)ms * timescale_ / 1000;
            unsigned lo = 0, hi = timestampCount_;
            while (lo < hi) {
                unsigned mid = (lo + hi) / 2;
                if (timestamp_[mid] < target) lo = mid + 1;
                else hi = mid;
            }
            curSample_ = (lo > 0) ? lo - 1 : 0;
            fillPending();
            return true;
        }
        if (kind_ == kFmtAacAdts) {
            pos_ = 0;
            if (initAdtsSeek()) return true;
        }
        return false;
    }
    const char* name() const override { return "faad2"; }

private:
    FaadDecoder() {}
    FaadDecoder(const FaadDecoder&);
    FaadDecoder& operator=(const FaadDecoder&);

    static int readCb(int64_t offset, void* buffer, size_t size, void* token) {
        /* minimp4 convention: return 0 on success, non-zero on failure. */
        FILE* f = (FILE*)token;
        if (fcSeek(f, offset, SEEK_SET) != 0) return -1;
        size_t got = std::fread(buffer, 1, size, f);
        return got == size ? 0 : -1;
    }

    bool initAdts(const std::string& path, std::string* err) {
        if (!readFile(path, &buf_)) { if (err) *err = "read failed"; return false; }
        unsigned long sr = 0;
        unsigned char ch = 0;
        long r = NeAACDecInit(h_, buf_.data(), (unsigned long)buf_.size(), &sr, &ch);
        if (r < 0) { if (err) *err = faadErr(); return false; }
        pos_ = (size_t)r;
        rate_ = (int)sr;
        ch_ = ch > 2 ? 2 : (ch ? ch : 2);
        return true;
    }

    bool initAdtsSeek() {
        NeAACDecClose(h_);
        h_ = NeAACDecOpen();
        if (!h_) return false;
        NeAACDecConfigurationPtr c = NeAACDecGetCurrentConfiguration(h_);
        if (c) { c->outputFormat = FAAD_FMT_16BIT; c->downMatrix = 1; NeAACDecSetConfiguration(h_, c); }
        unsigned long sr = 0; unsigned char ch = 0;
        long r = NeAACDecInit(h_, buf_.data(), (unsigned long)buf_.size(), &sr, &ch);
        if (r < 0) return false;
        pos_ = (size_t)r;
        rate_ = (int)sr;
        ch_ = ch > 2 ? 2 : (ch ? ch : 2);
        return true;
    }

    bool initMp4(const std::string& path, std::string* err) {
        f_ = std::fopen(path.c_str(), "rb");
        if (!f_) { if (err) *err = "open failed"; return false; }
        fcSeek(f_, 0, SEEK_END);
        int64_t sz = fcTell(f_);
        fcSeek(f_, 0, SEEK_SET);
        if (sz <= 0) { if (err) *err = "empty file"; return false; }
        if (!MP4D_open(&mp4_, readCb, f_, (int64_t)sz)) {
            if (err) *err = "MP4D_open failed";
            return false;
        }
        mp4Open_ = true;
        int tr = -1;
        for (unsigned i = 0; i < mp4_.track_count; ++i) {
            if (mp4_.track[i].SampleDescription.audio.samplerate_hz > 0 ||
                mp4_.track[i].dsi_bytes > 0) {
                tr = (int)i;
                break;
            }
        }
        if (tr < 0) { if (err) *err = "no audio track"; return false; }
        track_ = tr;
        MP4D_track_t& t = mp4_.track[tr];
        if (t.dsi && t.dsi_bytes > 0) {
            int aot = 0, sf = 0;
            if (!parseAsc(t.dsi, t.dsi_bytes, &aot, &sf)) {
                if (err) *err = "invalid AudioSpecificConfig (esds)";
                return false;
            }
            /* FAAD2 handles these AAC object types (incl. SBR=5 / PS=29); reject
             * anything else up front instead of failing deep inside NeAACDec. */
            switch (aot) {
            case 1: case 2: case 3: case 4: case 5: case 6: case 7:
            case 17: case 19: case 20: case 21: case 22: case 23:
            case 29: case 39: case 42: case 43:
                break;
            default:
                if (err) *err = "unsupported AAC object type: " + std::to_string(aot);
                return false;
            }
            unsigned long sr = 0; unsigned char ch = 0;
            char r = NeAACDecInit2(h_, t.dsi, t.dsi_bytes, &sr, &ch);
            if (r != 0) { if (err) *err = "NeAACDecInit2 failed"; return false; }
            rate_ = (int)sr;
            ch_ = ch > 2 ? 2 : (ch ? ch : 2);
        } else {
            if (err) *err = "missing AudioSpecificConfig (esds)";
            return false;
        }
        timescale_ = t.timescale;
        sampleCount_ = t.sample_count;
#if MP4D_TIMESTAMPS_SUPPORTED
        timestamp_ = t.timestamp;
        timestampCount_ = t.timestamp_count;
#endif
        if (timescale_ > 0) {
            uint64_t dur = ((uint64_t)t.duration_hi << 32) | t.duration_lo;
            if (dur == 0 && sampleCount_ > 0) {
                /* mdhd duration missing: derive the end time of the last frame
                 * from the stts (via MP4D_frame_offset) instead of assuming a
                 * fixed 1024 samples/frame, which is wrong for SBR. */
                unsigned fb = 0, ts = 0, fd = 0;
                MP4D_frame_offset(&mp4_, (unsigned)track_, sampleCount_ - 1, &fb, &ts, &fd);
                dur = (uint64_t)ts + fd;
            }
            if (dur > 0) durationMs_ = (int64_t)(dur * 1000 / timescale_);
        }
        curSample_ = 0;
        return true;
    }

    bool fillPending() {
        if (kind_ == kFmtM4a) return fillPendingMp4();
        return fillPendingAdts();
    }

    bool fillPendingMp4() {
        while (mp4Open_ && curSample_ < sampleCount_) {
            unsigned frameBytes = 0, ts = 0, dur = 0;
            MP4D_file_offset_t off = MP4D_frame_offset(&mp4_, (unsigned)track_, curSample_,
                                                       &frameBytes, &ts, &dur);
            ++curSample_;
            if (frameBytes == 0) continue;
            std::vector<uint8_t> tmp(frameBytes);
            if (readCb((int64_t)off, tmp.data(), frameBytes, f_) != 0) continue;
            decodeBytes(tmp.data(), frameBytes);
            if (!pending_.empty()) return true;
        }
        return false;
    }

    bool fillPendingAdts() {
        while (pos_ + 2 < buf_.size()) {
            if (!(buf_[pos_] == 0xFF && (buf_[pos_ + 1] & 0xF0) == 0xF0)) {
                ++pos_;
                continue;
            }
            size_t before = pos_;
            bool consumed = decodeBytes(buf_.data() + pos_,
                                        (unsigned long)(buf_.size() - pos_));
            if (!pending_.empty()) return true;
            if (!consumed || pos_ == before) ++pos_;
        }
        return false;
    }

    bool decodeBytes(unsigned char* data, unsigned long len) {
        NeAACDecFrameInfo fi;
        std::memset(&fi, 0, sizeof(fi));
        void* out = NeAACDecDecode(h_, &fi, data, len);
        if (kind_ == kFmtAacAdts && fi.bytesconsumed > 0)
            pos_ += fi.bytesconsumed;
        if (fi.error || !out) return false;
        if (fi.samplerate > 0 && (int)fi.samplerate != rate_) rate_ = (int)fi.samplerate;
        if (fi.channels > 0) {
            int cc = fi.channels > 2 ? 2 : (int)fi.channels; /* downMatrix caps at stereo */
            if (cc != ch_) ch_ = cc;
        }
        if (fi.samples > 0) {
            pending_.assign((const int16_t*)out, (const int16_t*)out + fi.samples);
            pendPos_ = 0;
        } else {
            /* valid frame with no output yet (e.g. priming) */
            pending_.clear();
            pendPos_ = 0;
        }
        return fi.bytesconsumed > 0 || fi.samples > 0;
    }

    std::string faadErr() const {
        return "FAAD2 init error";
    }

    NeAACDecHandle h_ = 0;
    FormatKind kind_ = kFmtUnknown;

    std::vector<uint8_t> buf_;
    size_t pos_ = 0;

    FILE* f_ = 0;
    MP4D_demux_t mp4_;
    bool mp4Open_ = false;
    int track_ = -1;
    unsigned timescale_ = 0;
    unsigned sampleCount_ = 0;
    unsigned curSample_ = 0;
    unsigned* timestamp_ = 0;
    unsigned timestampCount_ = 0;

    int rate_ = 0;
    int ch_ = 0;
    int64_t durationMs_ = 0;

    std::vector<int16_t> pending_;
    size_t pendPos_ = 0;
};

/* ---------------- provider ---------------- */
class FaadProvider : public DecoderProvider {
public:
    const char* id() const override { return "faad"; }
    bool available(std::string* err) override {
        (void)err;
        return true;
    }
    Decoder* create(const std::string& path, std::string* err) override {
        FormatKind k = sniffFormat(path);
        switch (k) {
        case kFmtWav:
        case kFmtFlac:
        case kFmtMp3:
            return MaDecoder::openFile(path, err);
        case kFmtOgg:
            return VorbisDecoder::openFile(path, err);
        case kFmtAacAdts:
        case kFmtM4a:
            return FaadDecoder::openFile(path, k, err);
        default:
            if (err) *err = "unsupported format";
            return 0;
        }
    }
};

} /* namespace */

DecoderProvider* faadProvider() {
    static FaadProvider provider;
    return &provider;
}

} /* namespace cx */
