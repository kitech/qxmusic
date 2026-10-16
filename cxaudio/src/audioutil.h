#ifndef CXAUDIO_AUDIOUTIL_H
#define CXAUDIO_AUDIOUTIL_H

#include <cstdint>
#include <string>
#include <vector>

namespace cx {

enum FormatKind {
    kFmtUnknown = 0,
    kFmtWav,
    kFmtFlac,
    kFmtMp3,
    kFmtOgg,
    kFmtAacAdts,
    kFmtM4a
};

/* Strip file:// scheme (keeps things simple this round; no network). */
std::string normalizeUri(const std::string& uri);

bool readHead(const std::string& path, uint8_t* buf, size_t cap, size_t* got);
bool readFile(const std::string& path, std::vector<uint8_t>* out);

FormatKind sniffFormat(const std::string& path);
const char* formatName(FormatKind k);

} /* namespace cx */

#endif /* CXAUDIO_AUDIOUTIL_H */
