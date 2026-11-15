# Bilibili Music（视频音轨）接入

## 路线核对（2026-11 实测）

| 方案 | 结果 |
|---|---|
| 音频区 `audio.bilibili.com` | 官方歌单已清空（`menu/hit` snum=0、`menu/song` 返回 `data:null`），仅存量 auid 可取流 → 弃用 |
| **视频音轨（采用）** | `ranking/v2?rid=3`（免签名）+ WBI 搜索 + `view`(cid) + `playurl?fnval=4048` → `dash.audio` |
| 音轨 CDN | 带 `Referer: https://www.bilibili.com/` → 206；不带 → **403** |

## 与网易云完全同构的分层

| 层 | 网易云 | B站 |
|---|---|---|
| HTTP/签名/JSON | Go `internal/catalog` | Go `internal/bili` |
| ABI | `ncm_top_playlists` / `ncm_playlist_tracks` / `ncm_resolve_source` / `ncm_search` | `ncm_bili_top_playlists` / `ncm_bili_playlist_tracks` / `ncm_bili_stream` / `ncm_bili_search` |
| 结果 JSON | 歌单/曲目/直链 + `headers` | **同形**，直链同样带 `headers` |
| 下载/缓存 | C++ curl / QNAM（带头） | 同一函数、同一 headers 解析 |
| 播放 | `file://` | 同 |

### 内容模型映射

| 概念 | 映射 |
|---|---|
| 歌单 | 音乐区榜单：总榜 `id=3`；其余为分区 `tid`（tname 聚合：翻唱/演奏/乐评盘点…） |
| 曲目 | 榜单视频（`id=aid`，内部映射 `aid→bvid`） |
| 取流 | `ncm_bili_stream(aid)` → `playurl` 最高码率（30251 FLAC > 30280 192K > 30232 132K > 30216 64K） |
| 搜索 | WBI 签名（`nav` 取键 + `mixinKeyEncTab` + MD5），`search_type=video&tids=3` |

## uni 契约：`headers`

`resolve` 结果统一新增：

```json
"headers": { "Referer": "…", "User-Agent": "…", "Cookie": "…(可选)" }
```

- NCM：`Referer: https://music.163.com/` + 统一 UA；
- B站：`Referer: https://www.bilibili.com/` + Windows UA（实测 Linux 完整 UA 会触发 `-352` 风控）；
- C++ 下载器逐条 `set`：ql `CURLOPT_HTTPHEADER`（`NcmTask.header_lines`）、qsk `QNetworkRequest::setRawHeader`；
- 带 headers 的直链**强制走落盘路径**（不让 ffmpeg/mpv/GStreamer 直连），播放本地文件。

## 可选增强

- `ncm_bili_set_cookie(SESSDATA)`：大会员账号可出 30251 Hi-Res FLAC；
- 排行页级缓存 5 分钟（B站对连续 ranking 请求返回 `-352`，Go 层内置缓存 + 一次退避重试）。

## UI

| 端 | 入口 |
|---|---|
| ql | 侧栏「音乐站点」选 `bili音乐` → 走原「热门歌单」流程（列表页/曲目页/播放全部/计数/⟳ 全复用）；搜索框回车走 `ncm_bili_search` |
| qsk | 首页「Music Site」→ Bilibili Music → 原歌单列表页（数据源=榜单）；搜索结果页同 163 流程 |

## 验证

```sh
cd go163-music-core && bash scripts/build-linux.sh && ./scripts/check-abi.sh   # 42 符号
gcc /tmp/opencode/bili_smoke.c -o /tmp/opencode/bili_smoke \
    -I out/include -L out -lncm -Wl,-rpath,$PWD/out
/tmp/opencode/bili_smoke
# top / tracks / stream(含 headers) / ncm resolve(含 headers) 均应为 st=0
```
