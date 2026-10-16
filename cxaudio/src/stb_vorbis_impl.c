/* Compile the stb_vorbis implementation.
 *
 * A wrapper is used instead of listing third_party/stb/stb_vorbis.c directly:
 * qmake silently drops a source file whose own directory is also on
 * INCLUDEPATH (which we need for stb_vorbis_iface.h). */
#include "stb_vorbis.c"
