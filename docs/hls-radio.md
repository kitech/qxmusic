# HLS 电台播放设计（P3.1）

> 2026-11-14 定稿并实施。数据源：`musiccom/radiolist.cpp`（41 条 hls=true，占清单 ~45%）。

## 1. 网搜核对结论

| 事实 | 依据 |
|---|---|
| mpv / ffmpeg 原生支持 HLS（lavf 内置 hls demux，nested network 开启） | mpv `demux/demux_lavf.c`：`{"hls", .nested_network = true, ...}`；mpv 直播 BBC m3u8 实例 |
| Qt6 QMediaPlayer（ffmpeg 后端，Qt≥6.5）可播 HLS | QtMultimedia ffmpeg 后端基于 libavformat |
| Android 系统播放器原生支持 HLS live（MPEG-TS/ADTS/MP3 段；AES-128 加密段也支持） | developer.android.com HLS 兼容表 |
| 中文主流台共性：AAC-LC in MPEG-TS，剥 TS 壳即得 ADTS 连续流 | dsh-music-player 调研（CN top200 hls 占 41.5%）；HLS-Radio-Converter `passthrough: True (adts)` |
| HLS 音频直播固有延迟 ~10–30s（协议缓冲），非 bug | Radio Mast / RSAS 文档 |

结论：ql 与 qsk 现有播放内核均具备原生 HLS 能力，**主方案 = 直连 m3u8 喂现有内核，零内核改动**。

## 2. 分端通路

### qlmusic（cxaudio：auto/ffmpeg/faad/mpv）
- `audioEngine->open(*.m3u8)` → ffmpeg/mpv 后端由 lavf 走 HLS demux。
- faad 后端不支持 → `onNetRadioStream` 前置检查：固定 faad 时状态栏+Toast 提示切 auto/ffmpeg/mpv（key `radio.faad_hls`）。
- `m_liveStream` 标记：`onAudioFinished`/`onAudioError` 中 live 直接复位 UI、**不 advanceNext/skipOnError**（直播流不连播）。
- PlayerBar `setLive(true)`：隐藏 seek 与时长，画 `● LIVE` 徽标（key `radio.live`）。

### qskmusic（AudioPlayer → QMediaPlayer）
- x64：Qt 6.7 ffmpeg 后端直接 `playUrl(id, m3u8)`。
- Android：QMediaPlayer → 系统 MediaPlayer（HLS live 支持）。
- `AudioPlayer::finished(id)`：`netradio-*` id 收到即断流/切台 → 页内 ▶ 标记复位。
- qskcomp AudioPlayer 无 error 信号（既有设计），失败表现为 finished，行为一致。
  **失败反馈**：qskcomp 接 `QMediaPlayer::errorOccurred` → 新信号 `playError(id)`（底层 msg 只进日志）；
  netradiopage 收到 `netradio-*` 的 playError → Toast「该电台流不可用，请稍后重试」（▶ 复位由 finished 兜底）。
- ql live 流错误：Toast `radio.stream_error`（「电台流不可用或已中断」）+ 状态栏 `player.error: msg` 双提示。


## 3. 数据/状态
- `radiolist` 的 `hls` 字段语义不变（true=m3u8），不再代表"不可播"；行尾 `· HLS` 为信息性标签（ql 放 artist 字段、qsk 放名字后缀），无灰态。
- 移除 `radio.hls_no`（ql 三份 JSON 已删；qsk ts 该源串随 lupdate 置 obsolete）。
- 新增 `radio.live`（LIVE 徽标）、`radio.faad_hls`（ql 后端提示）。

## 4. 兜底预案（P3.5，暂缓）
若实测某端某机型拒播 m3u8：`musiccom/hlsdemux` —— m3u8 解析（master 选最高带宽变体、相对 URL 对齐 RFC 8216）→ live playlist 周期刷新（TARGETDURATION/2）→ MPEG-TS PAT/PMT 找 audio PID → PES 重组出 ADTS 连续流（MP3-in-TS 提取裸帧）→ qsk 走 `play(bytes, mime="audio/aac")` / ql 喂流源。约 300 行，默认不实现。

## 5. 验证清单
- [x] ql Qt3 / Qt4 构建绿
- [x] qsk x64 / Android 构建绿（APK 13:11）
- [x] qsk offscreen 冒烟
- [ ] 真机/真播抽测：HLS 台起播延迟 3–12s 属正常；直连流、站点电台、收藏回归
