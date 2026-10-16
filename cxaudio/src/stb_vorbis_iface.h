#ifndef CXAUDIO_STB_VORBIS_IFACE_H
#define CXAUDIO_STB_VORBIS_IFACE_H

/* Pull in only the public declarations of stb_vorbis (the implementation is
 * compiled separately from third_party/stb/stb_vorbis.c). */
#ifndef STB_VORBIS_HEADER_ONLY
#define STB_VORBIS_HEADER_ONLY
#endif
#include "stb_vorbis.c"

#endif /* CXAUDIO_STB_VORBIS_IFACE_H */
