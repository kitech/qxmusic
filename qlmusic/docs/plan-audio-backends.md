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

## 5b. NCM 线上播放缓存层（sidecar 主导 · cache-first）

业界模式（网搜核对）：InnerTune/Spotify/Qobuz 一致采用
**本地命中即播先行**（player 先查 download cache 和 cache，都无才 internet
拉取），元数据独立记录（Linthra/Spotify SQL：cache metadata 只存非密
track id、id-derived 文件名、byte size、timestamps；**token URL 不持久、
fetch 时才生成**）。本项目按 sidecar 路径实施：

### 缓存布局
```
~/.cache/qlmusic/ncm/
  <song_id>.<ext>        实体音频（ext ∈ mp3/flac/m4a/aac）
  <song_id>.meta         sidecar {"level":"exhigh","size":N}
  <song_id>.<ext>.part   下载中临时（完成后原子 rename）
  songs/<净化歌名>.<ext>  软链 → ../<song_id>.<ext>（人名可浏览）
```
文件名**不带**任何比特率/音质/tag 后缀（与业界网易云客户端
`id-比特率-后缀` 命名分流；音质切换由 sidecar 比对判定）。

### 播放路径
```
点歌 → MainWindow::findNcmCache (零网络)
       解析 <id>.meta level ↔ m_ncmQuality 比对 + <id>.<ext> size 校验
       ├─ 命中 → 直接 playNcmResolved（跳过 resolveSource/fetchRemote，秒开）
       └─ 未命中/音质不匹配 → resolveSource → onNcmSource → fetchRemote
```

### worker（fetchRemoteTask）职责收窄：纯下载
curl → `<id>.<ext>.part` → ID3v2.4 前置（源自带 "ID3" 魔数 → 保留原 tag
跳写）→ rename → 写 `<id>.meta{level,size}` → ensureTitleSymlink。
**命中/音质判定逻辑全部上移 UI 层，双层检测不再冗余。**

### ID3 元信息（TIT2/题名, TPE1/歌者, TALB/专辑）
**系统 libid3tag**（0.16.x，`pkg-config id3tag` 探测可选链接
`HAVE_LIBID3TAG`，本机 0.16.4）：不 vendored；无库环境（mac 默认）
tag 段整体剔除、仅下载不写 tag（log 明示）。
工作流程（fetch 后 **.part 上原位操作**，再 rename）：
```
① 提取 + 日志：TIT2/TPE1/TALB → id3_field_getstrings →
   id3_ucs4_utf8duplicate；全量输出现有值（可为空）
② 补缺（仅缺失字段，已有保留；编码按 tag 版本分支：
   v2.4→UTF_8、v2.3→UTF_16 库 render 自动转换、v2.2 老 tag 仅日志）
   → id3_file_update；全齐跳写（log "tag complete"）
③ 无既有 tag（源净流，id3_file_tag==NULL）→ id3_tag_new 全量三帧
   （UTF_16，v2.3/v2.4 双合法）+ id3_tag_render + tag+原字节手拼前缀
```
**sidecar 顺序**：`stat(rename 后最终文件)` 取**含 tag 最终 size** 写
`<id>.meta`——pre-tag part 尺寸不入表（旧实现两 bug 修复：
手写帧头 size 错位 → 弃手写改库；sidecar 记 pre-tag size →
顺序改为 rename 后 stat）。
验证：`tools/id3smoke` CLI（读/补双场景）+ `ffprobe -show_format`
反读 + `xxd -l 12`（"ID3"+version+syncsafe）。
M4A/FLAC 属另一元数据体系（Phase 2，fetch 时 log skip）。

### 其他
- 软链：inode 比对，同名不同歌尾缀 `_<id>`；失败静默；每次 fetch 刷新指向
- `<id>.<ext>.part` 半断下载残留下次重下覆盖（原子落位，不污染缓存）
- 工作菜单 `menu.open_cache`：mkdir 确保目录 → `qOpenUrl(file://…ncm)`
- 旧无 sidecar 的 `<id>.<ext>` 文件不迁移；首次播放重下一次即入新体系

### 5c. 播放条 repeat 与失败连播开关
业界核对（Spotify/YTM/Apple 三态一致；mpv 默认失败跳下；foobar foo_skip 止损惯例）：
- 播放条两侧对称小图标：左 = 失败跳过开关（圆勾），右 = repeat（off ⟳ / all 双弧 / one 中央 1）
- repeat 三态存 `player.repeat`（默认 off）；autoskip 存 `player.autoskip`（默认开，NCM 灰歌场景）
- 自然播完：one→重播当前；all→环回首；off→停。手动 next：始终环绕（Spotify 式）
- 播放失败（errorOccurred）：autoskip 开→状态栏提示 + advanceNext（优先级高于 repeat one，
  不复播同一首防死循环）；连续 99 首失败（kErrSkipStopAt）→停播+StatusError 提示；
  成功播放（onAudioState Playing）清零计数。autoskip 关→原“停播+错误提示”
- Tool 菜单 `menu.autoskip` check 项与播放条图标双入口同步（updateBackendChecks 范式）
- 两图标动态 tooltip：qt3 走 `QToolTip` 子类 maybeTip（区段级现读文案）；qt5 走
  `event(QEvent::ToolTip)` + `QToolTip::showText`；命中计算唯一点 `tipForPos/tipRectFor`
- 灰歌真治本：resolve（onNcmSource rc≠0/url 空）与下载（onNcmFetched 三失败）三阶段
  统一走 `skipOnError(why)`（带控制台 trace 日志；只此一处止损/提示/advanceNext 真源）

### 5d. 播放条音量 / NCM 下载指示 / 空格暂停（本轮六项新需求）

**音量/布局终版（popup+自绘 panel+静音 checkbox+实时调音；右区歌词域）**
- 布局（拍板：控制按钮不占最右，歌词区才占最右）：
  - 中央：prev/play/next（cx∓40/cx）、autoSkip(cx-95)/repeat(cx+75) icon 行 y19..39；progress cx±130 y58..64；时间文本 progress 两外侧槽（cx-174/cx-134、cx+134/cx+178，y50..70）；
  - **pie + 喇叭横排于歌词区左邻**（pie `lyricLeft-52`、speaker `lyricLeft-24`，y19..39，与时间文本分行无撞）；
  - **三行滚动歌词区贴最右**（`lyricLeft= qMax(cx+190, W-312)`，y10..70 全高；`>=1024px` 窗达 300px 设计宽，窄窗动态缩宽让中央链；`LyricsStrip` 空占位子组件，resizeEvent 同步几何，暂零绘制）。
- 音量交互（Apple Music Win 式 + 静音独立控件拍板）：
  - 喇叭点击 → `WType_Popup`(qt3)/`Qt::Popup`(qt4) 弹层（VolumePopupFrame）+ **自绘 VolumePanelSlider**（零 QSlider——Qt3 QSlider 点击默认 pageStep 步进不跳值怪癖 + 弹层 X11 grab 吞释放，bar 内进度条同款已验证鼠标链替代）；panel click-jump clamp 2..100 防点击直接落 0 误静音；
  - **静音 checkbox 在弹层右侧**（QCheckBox "player.mute"；on=音量 0 记忆静音前值，off=回静音前值/50；setVolume 回环 blockSignals 同步）；
  - 播放条 wheel = ±5（VLC/Spotify desktop 惯例，compat34 `qWheelDeltaY` shim）；
  - 音量 0 = 喇叭画红斜杠 `volume_off` 态（Material 图标对惯例）。
- **关弹层 fake release + drag 兜底（Qt3 popup-example.html 官方范式）**：WType_Popup 的 X11 grab 把 move/release 派发给弹层容器 → VolumePopupFrame 三事件（press/move/release）转发 panel（幂等兜底）+ `closeEvent` 先 `panel->endDrag()` 再递假 `MouseButtonRelease(NoButton)` 给 bar——**m_drag 永真会造成悬停划过持续 volumeChanged 刷屏（已实测）**。
- 接线：`panel 自算值 → PlayerBar.onPopupVolume → volumeChanged(int) → MainWindow.onVolumeChanged → audioEngine.setVolume（cxaudio setter µs 级实证零卡顿，实时调音）→ 视觉即时 bar.setVolume；Config 写盘 debounce 500ms（onVolSaveTimeout：setValue+一条日志——日志随写盘拍，拖动不刷屏）；**Config::setValue 为 hjsonLoad+hjsonSave 全文写盘 IO（config.cpp:73），每 tick 双磁盘同步 IO 卡 UI 是初代"响应不流畅"根因。
- 联带修复：全局空格 eventFilter 对外来顶层窗口（popup 滑条）豁免，防在滑条内按空格误触播放暂停。

**NCM 下载 pie 指示器（Material Design 圆环）**
- 状态单图标：进度扇形 0-100 / 未定旋转短弧（-1，无 Content-Length）/ 失败红斜杠（-2，1.2s 后隐）/ 完成 100→1.2s 隐。
- 数据链：`fetchRemoteTask`（ncm_service.cpp）加 `CURLOPT_PROGRESSFUNCTION`（老 API，任意 curl 版本）→ `dlProgressCb`（节流 200ms 或 pct 变化）→ `postProgress(songId,pct)`（复用 `NcmResultEvent` kind=2 + `QApplication::postEvent` worker→主线程管道）→ `downloadProgress(qint64,int)` 信号 → `MainWindow.onNcmDownloadProgress`（只跟当前曲 `m_dlSongId`）→ `playerBar.setDownloadState`。
- 收尾：`onNcmFetched` rc==OK→pie 100 隐；rc!=OK→红斜杠（只有当前曲）。fetchRemote 调用处（resolve 后缓存 miss 路径）启动指示。

**空格暂停（全局 + 输入豁免拍板）**
- 实现于 `MainWindow.eventFilter`（`qApp->installEventFilter`，app 级；Qt3 无 QShortcut）：
  - `Key_Space` 无修饰键 → `onPlayPause()`（`audioEngine->toggle()`）；
  - 豁免：focusWidget 为 QLineEdit/QTextEdit/QComboBox/QSpinBox（输入控件）；
  - 豁免：focusWidget 顶层窗口非主窗（音量 popup 等 popup 窗）。
- 主窗 eventFilter 透传链：其余事件 `QMainWindow::eventFilter` 原路返回。

**B1/B2-B3 修复记录**
- B1（菜单 check 首启不亮）：`buildMenus()` 在 ctor :268，早于 :450-465 playerBar 段的 `m_autoSkip` 初始化 → `buildBackendMenu` 尾 `updateAutoSkipCheck()` 读未初始化内存。修：ctor 初始化列表前置 `m_repeatMode(0)/m_autoSkip(true)/m_autoSkipMenuIndex(-1)`（Config 真值在 playerBar 段覆盖）。
- B2/B3（点击切不动/零反馈）：shot2 已证 click→toggle→Config 落盘链通；根因是 Qt3 hover tooltip popup 偶发吞点击 + 点击后无可视变化。修：`onAutoSkipClicked` 末尾 `playerBar->nudgeAutoSkipTip()`（qt3 经 `PlayerBarToolTip::showTip` 公共转发 protected `QToolTip::tip`；qt5 `QToolTip::showText`）+ SharedStatusBar 文案双通道（带 icon 版本 SharedStatusBar::showMessage 无 3 参重载，故仅 msg+timeout）。

**lang 键**（player 对象）：`tip_volume`、`tip_muted`、`dl_downloading`、`dl_busy`、`dl_failed`（zh-CN/en-US/zh-TW 三份）。

## 6. 验证
- 桌面 `buildqt3/4/6.sh` 串行（大 timeout）；`tools/audio_smoke -b <id> <uri...>` 无 GUI 逐后端；`xvfb-run` GUI；mp3/flac/ogg/wav/m4a/aac 素材。
- Android（有条件时）：NDK 三 ABI 出 `libcxaudio.so`。

## 7. 风险
- FFmpeg 头/运行库 major 一致性（运行期自适应 + 能力探测缓解，非硬拒绝）；`swr`/channel-layout 跨版本。
- FAAD2 M4A(`esds`/HE-AAC) 与 seek 受限；FAAD2 由 CMake/qmake 传 `HAVE_*`/`PACKAGE_VERSION` 宏（`HAVE_CONFIG_H` 不定义，无需 `config.h`）。
- 桌面 qmake 与 Android CMake 两份源清单需同步维护。
- macOS bundle dylib rpath、Windows DLL 搜索路径、Android `nativeLibraryDir` 查找。
