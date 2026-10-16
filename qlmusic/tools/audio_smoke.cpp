/* qlmusic audio CLI smoke test (no GUI).
 *
 *   audio_smoke [-b backend] [-t seconds] <uri...>
 *
 * backend: auto | ffmpeg | faad | mpv
 *
 * Compiles the cxaudio core sources directly (via ../audio.pri); no .so.
 */
#include "cxaudio.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

int main(int argc, char** argv) {
    std::string backend = "auto";
    int seconds = 5;
    std::vector<std::string> files;

    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "-b") && i + 1 < argc) {
            backend = argv[++i];
        } else if (!std::strcmp(argv[i], "-t") && i + 1 < argc) {
            seconds = std::atoi(argv[++i]);
        } else if (!std::strcmp(argv[i], "-h") || !std::strcmp(argv[i], "--help")) {
            std::printf("usage: audio_smoke [-b backend] [-t seconds] <uri...>\n");
            return 0;
        } else {
            files.push_back(argv[i]);
        }
    }
    if (files.empty()) {
        std::printf("usage: audio_smoke [-b backend] [-t seconds] <uri...>\n");
        return 2;
    }

    cxaudio::Player player;
    player.setCallback([](int ev, int64_t a, int64_t b, const std::string& msg) {
        const char* names[] = {"state", "position", "duration", "eof", "error"};
        const char* n = (ev >= 0 && ev <= 4) ? names[ev] : "?";
        if (ev == cxaudio::EvState)
            std::printf("  [ev] %s: state=%lld\n", n, (long long)a);
        else if (ev == cxaudio::EvPosition)
            std::printf("  [ev] %s: %lld / %lld ms\n", n, (long long)a, (long long)b);
        else if (ev == cxaudio::EvDuration)
            std::printf("  [ev] %s: %lld ms\n", n, (long long)a);
        else
            std::printf("  [ev] %s: %s\n", n, msg.c_str());
        std::fflush(stdout);
    });

    int rc = 0;
    for (size_t i = 0; i < files.size(); ++i) {
        const std::string& f = files[i];
        std::printf("== %s (backend=%s)\n", f.c_str(), backend.c_str());
        if (!player.setBackend(backend)) {
            std::printf("  setBackend failed: %s\n", player.lastError().c_str());
            rc = 1;
            continue;
        }
        if (!player.open(f)) {
            std::printf("  open failed [%s]: %s\n", player.backendId().c_str(),
                        player.lastError().c_str());
            rc = 1;
            continue;
        }
        std::printf("  opened; backend=%s duration=%lld ms\n", player.backendId().c_str(),
                    (long long)player.duration());
        player.setVolume(80);
        player.play();

        int waitMs = 0;
        int pollTarget = (player.duration() > 0 && player.duration() < (int64_t)seconds * 1000)
                             ? 1500
                             : seconds * 1000;
        while (waitMs < pollTarget) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            waitMs += 100;
            if (player.state() == cxaudio::Stopped && player.duration() > 0 &&
                player.position() >= player.duration() - 200)
                break;
        }
        std::printf("  stopped at %lld / %lld ms\n", (long long)player.position(),
                    (long long)player.duration());
        player.stop();
    }
    return rc;
}
