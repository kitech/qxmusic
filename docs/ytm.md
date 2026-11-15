# YouTube Music 接入（yt-dlp）

## 路线核对（2026-11 实测）

| 方案 | 结果 |
|---|---|
| Piped 官方实例（15 个） | `pipedapi.kavin.rocks` 502；其余不可达/API 已下线 |
| Invidious 公共实例 | `inv.nadeko.net` / `invidious.nerdvpn.de` / `iv.duti.dev` 全部 **403 Endpoint disabled** |
| InnerTube 直调 | `WEB_REMIX` 搜索 200 可用；`player` 需 PO token，WEB/IOS/ANDROID 全 400/UNPLAYABLE |
| **yt-dlp 2026.10.26** | ✅ `ytsearch` 搜索、`-g` 直链、代理下取流 206 全通 |

结论：Go core 通过 **exec yt-dlp** 提供搜索与直链；Piped 仅保留为后续可配置备选。

## 架构

```
ql/qsk UI ──(搜索结果/直链 JSON)──> libncm (Go)
                                    └─ internal/ytm → exec yt-dlp
```

- `internal/ytm/ytm.go`
  - `Search(query, limit)`：`yt-dlp --flat-playlist --dump-json ytsearchN:<q>`
  - `Stream(videoId)`：`yt-dlp -f bestaudio[ext=m4a]/bestaudio -g <id>`
  - 均可执行文件 `yt-dlp`（PATH 查找，`ytm.SetBin` 可改）
- ABI（`ncm.h` / `ncm.hpp`）：
  - `ncm_ytm_available()`：0 = yt-dlp 可用
  - `ncm_ytm_set_proxy(char*)` / 环境变量 `YTM_PROXY`
  - `ncm_ytm_search(client, kw, limit, &out)` → `{"songs":[{id,name,artists,album,duration_ms,playable,publish_ms}]}`
    （`id` 为 **videoId 字符串**，与网易云数字 id 形状不同）
  - `ncm_ytm_stream(client, videoId, &out)` → `{"video_id":..,"url":..}`

## 代理（必须）

默认 `http://127.0.0.1:10809`（`YTM_PROXY` 覆盖）。两个环节都要走代理：

1. yt-dlp 自身：`--proxy <p>`（Go 层注入）；
2. **googlevideo 直链回源**：实测直连超时、走代理 206。ffmpeg（ql 引擎）与
   GStreamer soup（qsk 桌面）读 `http_proxy` 环境变量 —— 播放前设置、起播后清除，
   避免影响网易云直连。

## 平台

| 端 | 状态 |
|---|---|
| ql（Qt3/Qt4/Qt5 桌面） | 站点下拉选「youtube音乐」→ 搜索框回车 → 结果列表 → 点歌取流播放 |
| qsk（桌面） | 首页「Music Site」→ YouTube Music → 搜索页（`site=ytm`）→ 点歌 |
| Android | 入口隐藏（无 yt-dlp） |

## 依赖

- `yt-dlp` 需在 PATH（或后续提供配置项）。缺失时：
  - qsk：Toast「yt-dlp not found: YouTube Music unavailable」
  - ql：Toast `site.ytm_unavailable`

## 验证命令

```sh
# ABI 冒烟（含 available/search/stream）
gcc /tmp/opencode/ytm_smoke.c -o /tmp/opencode/ytm_smoke \
    -I go163-music-core/out/include -L go163-music-core/out -lncm \
    -Wl,-rpath,$PWD/go163-music-core/out
/tmp/opencode/ytm_smoke
```
