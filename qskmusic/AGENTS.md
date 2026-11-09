# qskmusic 页面切换约定（给后续 agent 的工作规约）

> 依据：pagemanager.cpp 全量实现 + anystik 主页同架构在真机稳定运行的实证 + qskmusic
> 数版改版在安卓真机的崩溃/无响应实录。加页面/改首页前先读本文。

## 1. 启动构造顺序（main.cpp 的契约，勿动）

```
installLogHandler → setOrganization/ApplicationName → (mac QSettings IniFormat 补丁)
→ Lang::instance() → (Android 皮肤插件提取 + addLibraryPath)
→ QskSkinManager setPluginPaths/setSkin("Fusion") → QSettings 恢复 skin/dark
→ rootBox(QskLinearBox vertical, panel)
→ stackBox(Animator4) → PageManager(stackBox)
→ 全局 PlayerBar + queuePopup/lyricPopup + Playback 连线   ← 常驻，不属于任何页面
→ registerPage × N → window.addItem(rootBox) → MyTabFocus/BackButtonFilter
→ show() → open("home")
→ applicationStateChanged → Android KeepAlive/NetworkMonitor → extras 线程
```

要点：
- 页面注册全部在 `window.show()/open(...)` 之前；`open("home")` 之后再无结构装配
- `Playback::instance()` 在主页创建前就绪（页面 onCreate 里直接可用）

## 2. 页面注册规约（registerPage）

- id 用小写英文（"home"/"playlists"/"tracks"/"logs"/"settings"/"about"）
- home：`{CachePolicy::Permanent, LaunchMode::SingleInstance}`
- 二级 push 页：`{CachePolicy::Transient, LaunchMode::Standard}`（返回即销毁，数据新鲜）
- launchArgs 即页面契约（在页面头注释里写清楚 key：`cat`(空=热门)/`playlistId`/`title`）
- **每加一页必须同时写**：factory + 注册策略 + launchArgs 契约注释，三件套缺一不可

## 3. open/back 语义（代码里固定，页面侧不能假设更多）

- `open()`/`back()` 有 `m_busy` 防抖——同一 tick 内的重复点击被静默忽略（有 qWarning 日志）
- 「页面在栈但被移出 stackbox」恢复：`onNewIntent + addItem` 自动完成，页面侧无感
- Transient 页 back 时**延迟 250ms 才 destroy/remove**（等 Animator4 转场跑完，防止
  animator 引用已被摘除的 item）——所以页面里不能对自己 deepcopy 的 Item
  deleteLater 后立刻假设它已不在场景
- 返回键（BackButtonFilter）：栈深>1 回栈；栈底 2s 内双击才退出
- 页面只允许通过 `pageManager()`（onCreate/信号 lambda 里）调 open/back；
  不要在 onDestroy/析构里发起导航

## 4. 页面实现硬规约（安卓真机踩坑汇总——每条都有血案）

### 4.1 页面内禁止第二个 QskStackBox / tab 壳
- 页内内嵌 stackBox（哪怕无动画）会让转场时"旧页的隐藏 clip 子树"同步进渲染批
  → 安卓 QSGBatchRenderer 空指针（真机 SIGSEGV 两抓，updateClipState→prepareRenderMergedBatch）
- 需要多 section：**全挂一棵 QskLinearBox 线性树**（home 现行结构），或 push 新页

### 4.2 滚动内容里的"行点击"必须走 QQuickTapHandler（anystik stickerlist.cpp:229）
- **QskPushButton::clicked 在 Android 触摸 + 滚动区（flickable 拦 touch）下不可靠**
  ——真机实测：分类树/歌单卡/曲目行全"点了没响应"
- 正确姿势（anystik stickerlist 同款）：
  ```cpp
  auto* tap = new QQuickTapHandler(m_contentBox);
  tap->setGesturePolicy(QQuickTapHandler::DragThreshold);
  connect(tap, &QQuickTapHandler::singleTapped, this, [this](const QPointF&) { ...按 y 算行号... });
  ```
- 滚动区**外**的按钮（顶栏 ☰/加载更多）用 QskPushButton::clicked 正常
- 分界线记忆：滚动内容里的可点行 = TapHandler；滚动区外控件 = 按钮 clicked

### 4.3 禁止 windowChanged/widthChanged 自适应重排
- Android 启动期窗口宽度多次跳变，任何 orientation/visible 切换都在首帧重排 clip 拓扑
  → 与 4.1 同源（首帧渲染窗口重排），NavBar/双形态设计因此被永久排除
- 尺寸选择按最窄屏（~393dp）一次定死

### 4.4 文本永不画出 item 边界
- 长文本行一律代码级截断（elide，anystik stickerlist.cpp:108 范式），
  禁止对含长文本的控件用可收缩策略（Preferred/Expanding 收缩后字形越界
  → 与 4.1 的 clip 批处理同源问题）

### 4.5 数据加载
- 全部走异步信号（ncmapi/ncmcatalog 已有工作线程+跨线程信号样板）
- 列表重建 = 清子项 deleteLater；页面信号回路用 m_loading / 请求 id 校验防串台
- 长列表数据来了再填控件；**onCreate 只建控件与接线**

## 5. 验证规约（每加一页或改首页必走）

1. `sh build-x64.sh` 必须绿 + offscreen 冒烟（`[PageManager] created/activated` 锚点）
2. Android `sh build-android.sh` 出 APK
3. 真机四点：启动 → 点新页入口 → 新页内一级操作 → 返回键回 home；
   全程无闪退、无"点了没响应"
4. 若真机闪退：先抓 `adb logcat -b crash -d` backtrace，勿猜测

## 6. 现存页面清单（基线）

| id | 类 | 策略 | 内容 |
|---|---|---|---|
| home | HomePage | Permanent | 分类树 + ☰ |
| playlists | PlaylistListPage | Transient | 歌单列表（cat/title） |
| tracks | PlaylistTracksPage | Transient | 曲目 → Playback |
| search | SearchPage | Transient | 搜索 → Playback |
| logs/settings/about | LogPage/SettingsPage/AboutPage | 见 main.cpp | 系统页 |
