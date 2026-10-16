#ifndef CXAUDIO_FFMPEGAPI_H
#define CXAUDIO_FFMPEGAPI_H

/* FFmpeg is hot-loaded and never linked; its headers are only present on
 * desktop toolchains (Android ships no FFmpeg headers). When they are absent
 * this header declares nothing and ffmpegapi.cpp compiles to an empty unit. */
#if defined(__has_include)
#  if __has_include(<libavformat/avformat.h>) && __has_include(<libavcodec/avcodec.h>) && \
      __has_include(<libswresample/swresample.h>)
#    define CXAUDIO_FFMPEGAPI_HEADERS 1
#  endif
#endif

#ifdef CXAUDIO_FFMPEGAPI_HEADERS

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>

#include <cstdint>
#include <string>

namespace cx {

/* Dynamically resolved subset of the FFmpeg libav* API. Structural access uses
 * the compile-time headers; at runtime we prefer a library whose major version
 * matches those headers (falling back to the closest available). */
struct FfmpegApi {
    unsigned (*avformat_version)();
    unsigned (*avcodec_version)();
    unsigned (*avutil_version)();
    unsigned (*swresample_version)();
    int (*avformat_open_input)(AVFormatContext**, const char*, const AVInputFormat*,
                               AVDictionary**);
    int (*avformat_find_stream_info)(AVFormatContext*, AVDictionary**);
    void (*avformat_close_input)(AVFormatContext**);
    int (*av_find_best_stream)(AVFormatContext*, enum AVMediaType, int, int,
                               const AVCodec**, int);
    int (*av_read_frame)(AVFormatContext*, AVPacket*);
    int (*avformat_seek_file)(AVFormatContext*, int, int64_t, int64_t, int64_t, int);

    const AVCodec* (*avcodec_find_decoder)(enum AVCodecID);
    AVCodecContext* (*avcodec_alloc_context3)(const AVCodec*);
    void (*avcodec_free_context)(AVCodecContext**);
    int (*avcodec_parameters_to_context)(AVCodecContext*, const AVCodecParameters*);
    int (*avcodec_open2)(AVCodecContext*, const AVCodec*, AVDictionary**);
    int (*avcodec_send_packet)(AVCodecContext*, const AVPacket*);
    int (*avcodec_receive_frame)(AVCodecContext*, AVFrame*);
    void (*avcodec_flush_buffers)(AVCodecContext*);

    AVPacket* (*av_packet_alloc)();
    void (*av_packet_free)(AVPacket**);
    void (*av_packet_unref)(AVPacket*);
    AVFrame* (*av_frame_alloc)();
    void (*av_frame_free)(AVFrame**);
    void (*av_frame_unref)(AVFrame*);
    int (*av_strerror)(int, char*, size_t);
    int64_t (*av_rescale_q)(int64_t, AVRational, AVRational);
    void (*av_channel_layout_default)(AVChannelLayout*, int);

    SwrContext* (*swr_alloc)();
    int (*swr_init)(SwrContext*);
    int (*swr_convert)(SwrContext*, uint8_t**, int, const uint8_t**, int);
    void (*swr_free)(SwrContext**);
#if LIBSWRESAMPLE_VERSION_MAJOR >= 4
    int (*swr_alloc_set_opts2)(SwrContext**, const AVChannelLayout*, enum AVSampleFormat, int,
                               const AVChannelLayout*, enum AVSampleFormat, int, int, void*);
#endif

    bool valid() const {
        return avformat_version && avformat_open_input && avformat_find_stream_info &&
               avformat_close_input &&
               av_find_best_stream && av_read_frame && avformat_seek_file &&
               avcodec_find_decoder && avcodec_alloc_context3 && avcodec_free_context &&
               avcodec_parameters_to_context && avcodec_open2 && avcodec_send_packet &&
               avcodec_receive_frame && avcodec_flush_buffers && av_packet_alloc &&
               av_packet_free && av_packet_unref && av_frame_alloc && av_frame_free &&
               av_frame_unref && av_strerror && av_rescale_q && swr_alloc && swr_init &&
               swr_convert && swr_free;
    }
};

bool loadFfmpegApi(FfmpegApi* api, std::string* err);

} /* namespace cx */

#endif /* CXAUDIO_FFMPEGAPI_HEADERS */

#endif /* CXAUDIO_FFMPEGAPI_H */
