#include "ffmpegapi.h"
#include "dynload.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <vector>

#ifdef CXAUDIO_FFMPEGAPI_HEADERS

namespace cx {
namespace {

std::string dirName(const std::string& path) {
    size_t p = path.find_last_of("/\\");
    return p == std::string::npos ? std::string(".") : path.substr(0, p);
}

std::string baseName(const std::string& path) {
    size_t p = path.find_last_of("/\\");
    return p == std::string::npos ? path : path.substr(p + 1);
}

std::vector<std::string> namesFor(const std::string& base, int major) {
    std::vector<std::string> v;
#if defined(_WIN32)
    v.push_back(base + "-" + std::to_string(major) + ".dll");
    v.push_back(base + ".dll");
#elif defined(__APPLE__)
    v.push_back("lib" + base + "." + std::to_string(major) + ".dylib");
    v.push_back("lib" + base + ".dylib");
#else
    v.push_back("lib" + base + ".so." + std::to_string(major));
    v.push_back("lib" + base + ".so");
#endif
    return v;
}

int trailingVersion(const std::string& name) {
    size_t p = name.find_last_of(".");
    if (p == std::string::npos) return -1;
    std::string tail = name.substr(p + 1);
    if (tail.empty() || !std::isdigit((unsigned char)tail[0])) return -1;
    return std::atoi(tail.c_str());
}

/* Find <base> shared lib in `dir`: versioned (preferred major) -> unversioned ->
 * highest numbered match. */
std::string findLib(const std::string& dir, const std::string& base, int major) {
    std::vector<std::string> names = namesFor(base, major);
    for (size_t i = 0; i < names.size(); ++i) {
        std::string full = dir + "/" + names[i];
        if (fileExists(full)) return full;
    }
    /* scan */
    std::vector<std::string> files = listDir(dir, "");
    std::string best;
    int bestVer = -1;
    for (size_t i = 0; i < files.size(); ++i) {
        std::string b = baseName(files[i]);
        std::string stem = b;
        size_t dot = b.find_first_of(".");
        if (dot != std::string::npos) stem = b.substr(0, dot);
#if defined(_WIN32)
        std::string want = base;
#else
        std::string want = "lib" + base;
#endif
        if (stem != want) continue;
        int ver = trailingVersion(b);
        if (ver > bestVer) {
            bestVer = ver;
            best = files[i];
        }
    }
    return best;
}

} /* namespace */

bool loadFfmpegApi(FfmpegApi* api, std::string* err) {
    std::memset(api, 0, sizeof(*api));

#if defined(__APPLE__)
    const int kMajor = LIBAVFORMAT_VERSION_MAJOR;
#else
    const int kMajor = LIBAVFORMAT_VERSION_MAJOR;
#endif

    std::string fmtPath;
    const char* explicitLib = std::getenv("CXAUDIO_FFMPEG_LIB");
    if (explicitLib && *explicitLib && fileExists(explicitLib)) {
        fmtPath = explicitLib;
    } else {
        std::vector<std::string> dirs;
        const char* envDir = std::getenv("CXAUDIO_FFMPEG_DIR");
        if (envDir && *envDir) dirs.push_back(envDir);
        {
            std::vector<std::string> more = librarySearchDirs("");
            dirs.insert(dirs.end(), more.begin(), more.end());
        }
        for (size_t d = 0; d < dirs.size() && fmtPath.empty(); ++d) {
            fmtPath = findLib(dirs[d], "avformat", kMajor);
        }
    }
    if (fmtPath.empty()) {
        if (err) *err = "libavformat not found";
        return false;
    }

    std::string dir = dirName(fmtPath);

    static std::vector<Library*> libs;
    for (size_t i = 0; i < libs.size(); ++i) delete libs[i];
    libs.clear();

    std::string libs0[] = {fmtPath, findLib(dir, "avcodec", kMajor),
                           findLib(dir, "avutil", kMajor),
                           findLib(dir, "swresample", kMajor)};
    for (size_t i = 0; i < 4; ++i) {
        if (libs0[i].empty()) continue;
        Library* lib = new Library();
        if (lib->open(libs0[i])) {
            libs.push_back(lib);
        } else {
            delete lib;
        }
    }
    if (libs.empty()) {
        if (err) *err = "failed to open libav* libraries";
        return false;
    }

    auto sym = [&](const char* n) -> void* {
        for (size_t i = 0; i < libs.size(); ++i) {
            void* p = libs[i]->sym(n);
            if (p) return p;
        }
        return 0;
    };

    api->avformat_version = (unsigned (*)())sym("avformat_version");
    api->avcodec_version = (unsigned (*)())sym("avcodec_version");
    api->avutil_version = (unsigned (*)())sym("avutil_version");
    api->swresample_version = (unsigned (*)())sym("swresample_version");
    api->avformat_open_input = (int (*)(AVFormatContext**, const char*, const AVInputFormat*,
                                        AVDictionary**))sym("avformat_open_input");
    api->avformat_find_stream_info =
        (int (*)(AVFormatContext*, AVDictionary**))sym("avformat_find_stream_info");
    api->avformat_close_input = (void (*)(AVFormatContext**))sym("avformat_close_input");
    api->av_find_best_stream =
        (int (*)(AVFormatContext*, enum AVMediaType, int, int, const AVCodec**, int))
            sym("av_find_best_stream");
    api->av_read_frame = (int (*)(AVFormatContext*, AVPacket*))sym("av_read_frame");
    api->avformat_seek_file =
        (int (*)(AVFormatContext*, int, int64_t, int64_t, int64_t, int))sym("avformat_seek_file");

    api->avcodec_find_decoder = (const AVCodec* (*)(enum AVCodecID))sym("avcodec_find_decoder");
    api->avcodec_alloc_context3 =
        (AVCodecContext * (*)(const AVCodec*)) sym("avcodec_alloc_context3");
    api->avcodec_free_context = (void (*)(AVCodecContext**))sym("avcodec_free_context");
    api->avcodec_parameters_to_context =
        (int (*)(AVCodecContext*, const AVCodecParameters*))sym("avcodec_parameters_to_context");
    api->avcodec_open2 = (int (*)(AVCodecContext*, const AVCodec*, AVDictionary**))sym(
        "avcodec_open2");
    api->avcodec_send_packet = (int (*)(AVCodecContext*, const AVPacket*))sym(
        "avcodec_send_packet");
    api->avcodec_receive_frame = (int (*)(AVCodecContext*, AVFrame*))sym(
        "avcodec_receive_frame");
    api->avcodec_flush_buffers = (void (*)(AVCodecContext*))sym("avcodec_flush_buffers");

    api->av_packet_alloc = (AVPacket * (*)()) sym("av_packet_alloc");
    api->av_packet_free = (void (*)(AVPacket**))sym("av_packet_free");
    api->av_packet_unref = (void (*)(AVPacket*))sym("av_packet_unref");
    api->av_frame_alloc = (AVFrame * (*)()) sym("av_frame_alloc");
    api->av_frame_free = (void (*)(AVFrame**))sym("av_frame_free");
    api->av_frame_unref = (void (*)(AVFrame*))sym("av_frame_unref");
    api->av_strerror = (int (*)(int, char*, size_t))sym("av_strerror");
    api->av_rescale_q = (int64_t (*)(int64_t, AVRational, AVRational))sym("av_rescale_q");
    api->av_channel_layout_default =
        (void (*)(AVChannelLayout*, int))sym("av_channel_layout_default");

    api->swr_alloc = (SwrContext * (*)()) sym("swr_alloc");
    api->swr_init = (int (*)(SwrContext*))sym("swr_init");
    api->swr_convert = (int (*)(SwrContext*, uint8_t**, int, const uint8_t**, int))sym(
        "swr_convert");
    api->swr_free = (void (*)(SwrContext**))sym("swr_free");
#if LIBSWRESAMPLE_VERSION_MAJOR >= 4
    api->swr_alloc_set_opts2 =
        (int (*)(SwrContext**, const AVChannelLayout*, enum AVSampleFormat, int,
                 const AVChannelLayout*, enum AVSampleFormat, int, int, void*))
            sym("swr_alloc_set_opts2");
#endif

    if (!api->valid()) {
        if (err) *err = "libav* symbol resolution failed";
        return false;
    }

    /* The compile-time struct layouts come from the headers; a library whose
     * major versions differ has no ABI guarantee and would be dereferenced with
     * the wrong layout. Reject it unless the caller explicitly opts in. */
    const char* allowMismatch = std::getenv("CXAUDIO_FFMPEG_ALLOW_MAJOR_MISMATCH");
    if (!(allowMismatch && *allowMismatch)) {
        const unsigned fmtMajor = api->avformat_version() >> 16;
        const unsigned codecMajor = api->avcodec_version() >> 16;
        const unsigned utilMajor = api->avutil_version() >> 16;
        const unsigned swrMajor = api->swresample_version() >> 16;
        if (fmtMajor != (unsigned)LIBAVFORMAT_VERSION_MAJOR ||
            codecMajor != (unsigned)LIBAVCODEC_VERSION_MAJOR ||
            utilMajor != (unsigned)LIBAVUTIL_VERSION_MAJOR ||
            swrMajor != (unsigned)LIBSWRESAMPLE_VERSION_MAJOR) {
            if (err) {
                *err = "libav* major version mismatch (runtime fmt=" +
                       std::to_string(fmtMajor) + " codec=" + std::to_string(codecMajor) +
                       " util=" + std::to_string(utilMajor) + " swr=" + std::to_string(swrMajor) +
                       " != headers fmt=" + std::to_string((unsigned)LIBAVFORMAT_VERSION_MAJOR) +
                       "; set CXAUDIO_FFMPEG_ALLOW_MAJOR_MISMATCH=1 to override)";
            }
            return false;
        }
    }
    return true;
}

} /* namespace cx */

#endif /* CXAUDIO_FFMPEGAPI_HEADERS */
