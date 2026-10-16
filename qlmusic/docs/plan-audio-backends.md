# qlmusic 音频核心规划 v5（多平台可编 · NDK 可用 · 3 后端并行）

## 0. 已锁定决策
1. 核心**去 Qt**、纯 C++17/C；同时提供 **C ABI（前缀 `cxaudio_`）+ C++ API**。
2. 构建用 **CMake**，工程宿主目录 **`qskmusic/`**；**Android 产出 `libcxaudio.so`**。
3. ABI 全出：`arm64-v8a` + `armeabi-v7a` + `x86_64`。
4. FFmpeg/libmpv 的 Android `.so`：**只预留目录 + 运行时查找**，不预编二进制。
5. **桌面（qlmusic, Qt3/4/6 qmake）直接把核心源编入，不链接/不生成 `.so`**。
6. FFmpeg/mpv 桌面走**热加载**、不链接；FFmpeg 版本**运行期检测、可用即用**（不钉死、不因版本不符直接放弃）。
7. `auto` 默认后端 = **B（FAAD 全内建）**；三后端全部编译进程序。
8. 本轮**不做 URL 获取**；仅实现「给定 URI 能播」；macOS/Windows 本地可编。

## 1. 源文件去向
```
qxmusic/                                   # 仓库根
  third_party/                             # 第三方库放**顶级**（多工程共享）
    miniaudio/miniaudio.h
    stb/stb_vorbis.c
    minimp4/minimp4.h
    faad2/{include,libfaad,CMakeLists.txt} # 2.11.3，无 dr_libs（并入 miniaudio 内建）
    mpv/include/mpv/client.h
    fetch.sh  LICENSES/
  cxaudio/                                 # 音频核心（零 Qt）放**顶级**独立工程
    CMakeLists.txt                         # Android: libcxaudio.so（3 ABI）；桌面可选静态目标
    cxaudio.cmake                          # 源清单/包含目录（仿 qskcomp.cmake）
    include/cxaudio.h                      # C ABI（extern "C"，cxaudio_*）
    include/cxaudio.hpp                    # C++ API（cxaudio::Player）
    src/                                   # 零 Qt
      core.{h,cpp} dynload.{h,cpp} audioutil.{h,cpp} cxcommon.h
      pcm_decoder.h pcm_backend.{h,cpp}
      decoder_faad.{h,cpp} decoder_ffmpeg.{h,cpp} backend_mpv.{h,cpp}
      ffmpegapi.{h,cpp} mpvapi.{h,cpp} cxaudio_c.cpp
      miniaudio_impl.c
    android/lib/{arm64-v8a,armeabi-v7a,x86_64}/.gitkeep   # 预留 FFmpeg/mpv
  qskmusic/                                # 预留：将来 Qt Quick 前端（暂空）
  qlmusic/
    src/audio/AudioEngineAdapter.{h,cpp}   # : QObject（仅 QtCore），包 cxaudio::Player
    audio.pri                              # 显式源清单：编入 ../cxaudio/src + ../third_party
    tools/audio_smoke.{pro,cpp}            # 桌面 CLI，直接编译核心源（无 .so）
    docs/plan-audio-backends.md            # 本规划落盘处
```
> 第三方库位于**顶级 `third_party/`**，音频核心位于**顶级 `cxaudio/`**（被 qlmusic 等共享）。`audio.pri` 用**显式文件列表**（Qt3 qmake 无 `$$files()` 通配），需与 `cxaudio.cmake` 同步维护。

## 1b. 第三方库清单（钉 tag + 拷贝进仓）
| 用途 | 仓库 | 钉定 ref | 拷入文件 | 许可 |
|---|---|---|---|---|
| 输出设备 + WAV/FLAC/MP3 解码 | `mackron/miniaudio` | tag `0.11.25`（`9634bed`） | `miniaudio.h` | PD / MIT-0 |
| OGG/Vorbis 解码 | 同上 `extras/` | `0.11.25` | `extras/stb_vorbis.c` | PD |
| M4A 容器解析 | `lieff/minimp4` | commit `2a3014a` | `minimp4.h` | CC0-1.0 |
| AAC 解码 | `FreewareAdvancedAudio/faad2` | tag `2.11.3` | `include/{faad.h,neaacdec.h}` + `libfaad/**` | GPL-2.0+ |
| mpv 后端头 | `mpv-player/mpv` | tag `v0.41.0`（`41f6a64`） | `include/mpv/client.h` | ISC（头） |
| FFmpeg | 系统/`pkg-config` | 运行期检测 | 不 vendor | LGPL/GPL |
| ~~dr_libs~~ | — | — | **弃用**（WAV/FLAC/MP3 走 miniaudio 内建） | — |

raw 地址：
```
miniaudio.h    https://raw.githubusercontent.com/mackron/miniaudio/0.11.25/miniaudio.h
stb_vorbis.c   https://raw.githubusercontent.com/mackron/miniaudio/0.11.25/extras/stb_vorbis.c
minimp4.h      https://raw.githubusercontent.com/lieff/minimp4/2a3014a/minimp4.h
faad2          https://github.com/FreewareAdvancedAudio/faad2/archive/refs/tags/2.11.3.tar.gz
mpv client.h   https://raw.githubusercontent.com/mpv-player/mpv/v0.41.0/include/mpv/client.h
```
无外网时用 `ghpxy` 代理；文件直接拷进仓，附 `third_party/fetch.sh` 记录上述 pin。

## 2. 核心接口
- **C ABI** `include/cxaudio.h`：
```c
typedef struct cxaudio_player cxaudio_player;
typedef void (*cxaudio_cb)(int ev, int64_t a, int64_t b, const char* msg, void* user);
cxaudio_player* cxaudio_create(void);
void cxaudio_destroy(cxaudio_player*);
int  cxaudio_set_backend(cxaudio_player*, const char* id);   /* ffmpeg|faad|mpv|auto */
int  cxaudio_open(cxaudio_player*, const char* uri);
int  cxaudio_play(cxaudio_player*); int cxaudio_pause(cxaudio_player*); int cxaudio_stop(cxaudio_player*);
int  cxaudio_seek(cxaudio_player*, int64_t ms); int cxaudio_set_volume(cxaudio_player*, int v);
int64_t cxaudio_position(cxaudio_player*); int64_t cxaudio_duration(cxaudio_player*);
int  cxaudio_state(cxaudio_player*); void cxaudio_set_callback(cxaudio_player*, cxaudio_cb, void*);
const char* cxaudio_backend_id(cxaudio_player*); const char* cxaudio_last_error(cxaudio_player*);
```
- **C++** `include/cxaudio.hpp`：`cxaudio::Player`（`std::function` 回调）；C ABI 与 C++ 共用**同一实现**（其一做薄封装）。
- 线程：`std::thread/mutex/atomic`；PCM 环形缓冲 → `ma_device`（A/B）；mpv 自身线程。UI 线程 marshal 由前端负责。

## 3. 后端
- **A｜miniaudio + FFmpeg（热加载）**：运行期枚举所有 `libavformat.so.*` / `libavformat*.dylib` / `avformat-*.dll`，逐个 dlopen 读 `avformat_version()`；优先与编译头 major 一致，否则取最近版本并进入能力探测（存在 `swr_alloc_set_opts2`(≥5.0)/`av_channel_layout_*`(≥5.1) 则用，否则旧 API）；结构差异用 `LIBAVCODEC_VERSION_MAJOR` 编译分支；**不因版本不同判死**。
- **B｜miniaudio + vendored 解码（始终可用）**：`sniffFormat()` 分派——`ID3/0xFFEx`→miniaudio 内建 MP3、`fLaC`→miniaudio 内建 FLAC、`RIFF/WAVE`→miniaudio 内建 WAV、`OggS`→stb_vorbis、`ftyp/M4A`→minimp4 取 `esds` 的 AudioSpecificConfig→FAAD2 `NeAACDecInit2`、`0xFFFx`→FAAD2 `NeAACDecInit`。
- **C｜libmpv（热加载）**：`vo=null`；观察 `time-pos/duration/pause/eof-reached`。

## 4. 平台/构建矩阵
| | Linux | macOS | Windows | Android |
|---|---|---|---|---|
| 核心编译 | ✅ 直编 | ✅ 直编 | ✅ 直编 | ✅ CMake→`libcxaudio.so` |
| 输出 | 无 .so | 无 .so | 无 .so | `libcxaudio.so`×3 ABI |
| 平台库 | `-ldl -lpthread -lm` | CoreAudio/CoreFoundation/AudioToolbox | `-lole32 -lwinmm -luser32` | `-llog -lOpenSLES`；`-Wl,-z,max-page-size=16384` |
| FFmpeg/mpv | 热加载 | 热加载 | 热加载 | 预留目录+查找（无二进制） |

CMake 验证：
```
cmake -S cxaudio -B build-ndk \
  -DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-26
cmake --build build-ndk     # → libcxaudio.so
```

## 5. 桌面接线（qlmusic）
- `audio.pri`：`INCLUDEPATH += ../cxaudio/include ../third_party/...`；`SOURCES +=` 核心源 + `AudioEngineAdapter`；**无 `LIBS -lcxaudio`**。
- `PlayerBar`：加 `playPauseRequested/prevRequested/nextRequested/seekRequested(int)` + `setProgress/setDuration`；进度由引擎驱动。
- `MainWindow`：持 `AudioEngineAdapter*`；`onTrackActivated`→`ensureEngine()`→`open(t.url)`（`Track` 增可选 `url`）→`play()`；`工具(&T)` 三后端单选（下次播放生效）；`aboutToFinish` 自动下一首。
- `Config` 增默认 `audioengine=auto`；env `QLMUSIC_AUDIO` 覆盖。
- `lang/{zh-CN,zh-TW,en-US}.json` 增键。

## 6. 验证
- 桌面 `buildqt3/4/6.sh` 串行（大 timeout）；`tools/audio_smoke -b <id> <uri...>` 无 GUI 逐后端；`xvfb-run` GUI；mp3/flac/ogg/wav/m4a/aac 素材。
- Android（有条件时）：NDK 三 ABI 出 `libcxaudio.so`。

## 7. 风险
- FFmpeg 头/运行库 major 一致性（运行期自适应 + 能力探测缓解，非硬拒绝）；`swr`/channel-layout 跨版本。
- FAAD2 M4A(`esds`/HE-AAC) 与 seek 受限；FAAD2 由 CMake/qmake 传 `HAVE_*`/`PACKAGE_VERSION` 宏（`HAVE_CONFIG_H` 不定义，无需 `config.h`）。
- 桌面 qmake 与 Android CMake 两份源清单需同步维护。
- macOS bundle dylib rpath、Windows DLL 搜索路径、Android `nativeLibraryDir` 查找。
