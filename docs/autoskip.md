# 连播自动跳播（autoskip）与版权提示

## 问题

连播时遇到**版权/无可播资源提示**（`NCM_ERR_NO_RESOURCE` / `NCM_ERR_GEO_BLOCK`）后，
播放偶发停住，需要手动点一次才继续。

## 根因（双版本）

| 端 | 缺陷 |
|---|---|
| ql | ① `m_errSkipRun` 成功起播时**从不归零**，版权曲零散失败跨曲累计到 99 → 止损停播；② 引擎 `State=4 (Error)`（解码失败/无效流且无 error 文案）只置暂停、不跳播；③ 队列失效/到尾时静默停 |
| qsk | ① `AudioPlayer::playError` **完全没接线** → 直链播放失败（版权 403/格式拒/断网）静默停住；② 带 Referer 的 B站直链无法被 `QMediaPlayer` 播放 → 必然 403 → 落入① |

## 修复

### ql（`mainwindow.cpp`）
- `onAudioState`：`state==1` → `m_errSkipRun=0`；`state==4 && !m_liveStream && m_autoSkip` → `skipOnError("engine error state")`；
- `playNcmResolved`：起播成功 → `m_errSkipRun=0`（覆盖 cache-first 路径）；
- `skipOnError`：队列失效/已到尾且不循环 → 明确状态栏提示并停播（不再静默）；
- `onNcmSource` JSON 解析失败 → 与其它失败分支一致走 `skipOnError`。

### qsk（`playback.{h,cpp}`）
- 新增 `onStreamFailure(why)`：同曲去重（`m_failedSongId`，防 `playError` + resolve 失败双跳）→ 计数 → 跳下一首 → 99 止损；
- `AudioPlayer::playError` 接入：id 匹配当前曲时走 `onStreamFailure`；
- `startPlayback(url)`：申请音频焦点 + 起播 + 成功归零计数 + 歌词预取；
- `resolveDone`：`headers` 非空（B站）→ `DownloadService` 落盘后播 `file://`（缓存命中直接播）；下载失败（-2）→ 跳播。

## 语义

- 提示（Toast）非模态，与跳播并行，不阻塞连播；
- 连续失败止损阈值 99（ql/qsk 一致），成功一次即归零，正常连播不会误触发；
- 队尾且非循环时停止（不无限循环）。
