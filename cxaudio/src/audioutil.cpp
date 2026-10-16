#include "audioutil.h"

#include <cstdio>
#include <cstring>

namespace cx {

std::string normalizeUri(const std::string& uri) {
    const std::string p = "file://";
    if (uri.compare(0, p.size(), p) == 0)
        return uri.substr(p.size());
    return uri;
}

bool readHead(const std::string& path, uint8_t* buf, size_t cap, size_t* got) {
    if (got) *got = 0;
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    size_t n = std::fread(buf, 1, cap, f);
    std::fclose(f);
    if (got) *got = n;
    return true;
}

bool readFile(const std::string& path, std::vector<uint8_t>* out) {
    out->clear();
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    std::fseek(f, 0, SEEK_END);
    long sz = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (sz < 0) { std::fclose(f); return false; }
    out->resize((size_t)sz);
    size_t n = sz > 0 ? std::fread(out->data(), 1, (size_t)sz, f) : 0;
    std::fclose(f);
    out->resize(n);
    return true;
}

static bool startsWith(const uint8_t* b, size_t n, const char* sig, size_t m) {
    return n >= m && std::memcmp(b, sig, m) == 0;
}

FormatKind sniffFormat(const std::string& path) {
    uint8_t h[16];
    size_t n = 0;
    if (!readHead(path, h, sizeof(h), &n) || n < 4)
        return kFmtUnknown;

    if (startsWith(h, n, "RIFF", 4)) return kFmtWav;
    if (startsWith(h, n, "fLaC", 4)) return kFmtFlac;
    if (startsWith(h, n, "OggS", 4)) return kFmtOgg;
    if (startsWith(h, n, "ID3", 3)) return kFmtMp3;
    if (n >= 2 && h[0] == 0xFF && (h[1] & 0xE0) == 0xE0) {
        /* 0xFFFx: ADTS AAC vs MPEG audio. Layer bits (bits 1-2) nonzero => MPEG. */
        int layer = (h[1] >> 1) & 0x03;
        if (layer != 0) return kFmtMp3;
        return kFmtAacAdts;
    }
    if (n >= 8 && std::memcmp(h + 4, "ftyp", 4) == 0) return kFmtM4a;
    return kFmtUnknown;
}

const char* formatName(FormatKind k) {
    switch (k) {
    case kFmtWav: return "wav";
    case kFmtFlac: return "flac";
    case kFmtMp3: return "mp3";
    case kFmtOgg: return "ogg";
    case kFmtAacAdts: return "aac(adts)";
    case kFmtM4a: return "m4a";
    default: return "unknown";
    }
}

} /* namespace cx */
