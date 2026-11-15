# 播放条布局（qskmusic · QSkinny）

状态：生效　更新：2026-11-11

## 整体结构（根布局自上而下）

```
stackBox（页面栈）
ProgressLine（播放进度 · 3dp · 全宽 · 常显，蓝 #3d8bfd 填充 / 灰 #4a4a4a 底槽）
PlayerBar（56dp 两行高 · 单层横排五格 + 词）
ProgressLine（下载进度 · 3dp · 全宽 · 常显，绿 #4caf50 填充 / 灰 #4a4a4a 底槽）
```

## PlayerBar 内部（QskLinearBox Horizontal，高 56dp，margins 6，spacing 8）

```
┌──────────────────────────────────────────────────────────────┐
│ [♪ 40×40] 歌曲名 elide10                1:23  [词30] [☰36] │
│           歌手  elide14（无歌→左右横划提示） 4:56            │
└──────────────────────────────────────────────────────────────┘
              [▶/⏸ 44×44 圆形，唯一]
```

| 区 | 控件 | 规格 | 交互 |
|---|---|---|---|
| icon | QskTextLabel "♪" | 40×40，面板 + 圆角 8 | 占位封面（后续接 cover URL） |
| 文本列 | QskLinearBox 纵向两行 | 歌名 10 字 elide / 歌手 14 字 elide | **左右横划**（±36dp）→ prev/next |
| 播放键 | QskPushButton | 44×44 圆形（QskBoxShapeMetrics 22） | togglePause |
| 时间列 | QskLinearBox 纵向两行 | 上=当前（亮）/ 下=总时长（Caption 灰），右对齐 | — |
| 词 | QskPushButton "词" | 30×30 | lyricRequested（歌词弹层） |
| 队列 | QskPushButton "☰" | 36×36 | 点击=队列弹层；**长按=歌词**（额外通道） |
| 音量 | QskPushButton "🔊" | 30×30 | **仅桌面创建；Android 不创建** |

## 进度线（ProgressLineItem：QQuickItem + QskPaintedNode）

| 线 | 位置 | 填充 | 数据源 |
|---|---|---|---|
| 播放 | 播放条上方 | 蓝 #3d8bfd | Playback progress/duration |
| 下载 | 播放条下方 | 绿 #4caf50 | DownloadService progress |

- 两条均**常显**；无进度时画底槽色 `#4a4a4a`（不与面板同色，始终可辨）
- 下载完成（100）/ 失败（-2）→ 下载线归零（回底槽色，常显）
- 刷新机制：`setRatio()` 脏标记 → `updatePaintNode` → `node->update(window, rect, size, nullptr)`；
  `hash()` 未变不重建纹理（本机 QSkinny 头文件核对：无 triggerUpdate，anystik 的为自定义包装）

## 信号接线（main.cpp）

| 信号 | 目标 |
|---|---|
| playPauseRequested | Playback::togglePause |
| prevRequested / nextRequested | Playback::prev / next（横划触发） |
| queueRequested | QueuePopup |
| lyricRequested | LyricPopup |
| volumeClicked | 桌面 toast 占位 |

## 约束

- 不引入宽度自适应（AGENTS.md 4.3）；固定 56dp 两行高
- 文本一律 elide（AGENTS.md 4.4）
- 任何改动不得删除既有入口（词 / 队列 / 音量 / 滑动切歌 / 进度线）
