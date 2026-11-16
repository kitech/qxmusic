#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "compat34.h"
#include "appearance.h"
#include "musicmodel.h"
#include "contentview.h"
#include "ncm_service.h"
#include "ConfigDialog.h"   /* SettingsChangedMap 类型随 Q_OBJECT 槽声明完整 */
#include <qmainwindow.h>
#include <qdatetime.h>
#include <qwidget.h>
#include <qlabel.h>
#include <qpushbutton.h>
#include <vector>

class FramelessHelper;
class CustomTitleBar;
class SystemTrayIcon;
class SideNav;
class ContentView;
class QueuePanel;
class PlayerBar;
class AudioEngineAdapter;
class QSplitter;
class QTimer;
namespace musiccom { class FavoriteStore; }
namespace musiccom { class QueueStore; }

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow(QWidget* parent = 0);
    virtual ~MainWindow();

protected:
    virtual void closeEvent(QCloseEvent* event);
    /* 首次显示：按屏幕可用区夹取窗口尺寸（mac 布局最小宽撑出屏幕的兜底） */
    virtual void showEvent(QShowEvent* event);
    /* 空格=全局播放/暂停（app 级 eventFilter；输入控件/外来顶窗豁免） */
    bool eventFilter(QObject* obj, QEvent* ev);
    virtual void customEvent(CustomEventBase* event);

private slots:
    void onTitleUilang(int index);
    void onTitleStyle(int index);
    void onTitleDark(bool on);
    void onDarkMenuItem();
    void retranslateUi();
    void trayActivated(int reason);
    void trayShowMainWindow();
    void quitApp();
    void onMenu1Stub();
    void onDemoStatusInfo();
    void onDemoStatusWarning();
    void onDemoStatusError();
    void onDemoStatusClear();
    void onDemoTrayBubble();
    void onDemoToggleStatusWidgets();
    void onDemoApiRequest();
    void onAboutApp();
    void onTrackActivated(int playlistId, int trackIndex);
    void onBackendSelected(int index);
    void openSettings();
    void onOpenCacheDir();
    void onSettingsSaved(const SettingsChangedMap& changed);
    void onAudioTestPlay();
    void onUserLoginPlaceholder();
    void onPlayPause();
    void onPrev();
    void onNext();
    void onRepeatClicked();
    void onShuffleClicked();
    void onAutoSkipClicked();
    void onSeek(int ms);
    /* 音量（播放条喇叭 + popup 滑条 + wheel；apply/save 双 debounce） */
    void onVolumeChanged(int v);
    void onVolSaveTimeout();      /* 500ms Config 写盘 debounce（全文 hjson 重写卡顿根修；日志随写盘拍） */
    void onQueueSaveTimeout();    /* 队列保存 debounce 到期 → 立即落盘 */
    void onNcmDownloadProgress(qint64 songId, int pct);
    void onAudioPosition(int ms);
    void onAudioDuration(int ms);
    void onAudioState(int state);
    void onAudioFinished();
    void onAudioError(const QString& msg);

    /* NCM（libncm 网易云源）*/
    void onNcmCategorySelected(const QString& catKey);
    void onNavAction(int action);
    void onNcmPlaylistActivated(qint64 ncmPlaylistId);
    void onContentViewRefresh();
    void onNcmLoadMore();
    void onNcmBack();
    void onNcmCategories(int rc, const QString& json, int sel);
    void onNcmTopPlaylists(int rc, const QString& json, int sel);
    void onNcmPlaylistTracks(int rc, const QString& json, int localId);
    void onNcmSource(int rc, const QString& json, qint64 songId);
    void onNcmFetched(int rc, const QString& json, qint64 songId);
    void onNcmLyric(qint64 songId, const QString& json);
    void onNcmArtist(qint64 artistId, int rc, const QString& json);
    void onNcmAuthRequired(const QString& op);
    /* 收藏：本地（favorites.json）+ 账号双向 */
    void onFavToggled(int playlistId, int trackIndex);
    void onNcmLikeList(int rc, const QString& json);
    void onNcmSongDetail(int rc, const QString& json, int sel);
    void onNcmLikeDone(qint64 songId, bool on, int rc);
    /* 站点电台 */
    void onNcmDjHot(int rc, const QString& json);
    void onNcmDjPrograms(int rc, const QString& json, qint64 rid);
    void onRadioSelected(qint64 rid);
    /* 网络电台（内置清单） */
    void onNetRadioRegion(const QString& regionKey);
    void onNetRadioStream(int index);
    void onRadioWatchdog();   /* 异步 open 60s 无结果 → 提示并复位 */
    /* YouTube Music（yt-dlp；桌面端） */
    void onSiteChanged(const QString& site);
    void onSearchTriggered(const QString& text);
    void onSearchLoaded(int rc, const QString& json);
    void onYtmSearchDone(int rc, const QString& json);
    void onYtmStreamDone(int rc, const QString& json);

private:
    struct MenuItemRef {
        const char* key;
        bool darkToggle;
        void* owner;
#ifdef QT3_BUILD
        int id;
#else
        void* action;
#endif
        MenuItemRef() : key(0), darkToggle(false), owner(0)
#ifdef QT3_BUILD
            , id(-1)
#else
            , action(0)
#endif
        {}
    };

    void buildStatusBar();
    void buildCentralWidget();
    void buildAppearancePanel();
    void buildMenus();
    void buildAppearanceMenus();
    void buildBackendMenu(void* toolMenu);
    void buildTray();
    /* 收藏：从 FavoriteStore 重建"我的收藏"歌单并刷新两端视图 */
    void refreshFavPlaylist();
public:
    void saveQueueState();            /* 队列快照落盘（current<0 时不写，保留旧快照；立即同步写） */
    void markQueueDirty();            /* 内部变更点：置脏 + 300ms debounce 合并落盘 */
    void restoreQueueSnapshot();      /* 启动恢复：临时歌单摆队列，不自动播 */

    void ensureEngine();
    void playTrack(int playlistId, int trackIndex);
    void updateBackendChecks();
    void loadLyricsForTrack(const Track& t);   /* 本地 .lrc 优先 + NCM 兜底注入 */

    /* NCM 视图流转（UI 线程，禁止直接调 ncm_* 阻塞调用） */
    void requestTopPlaylists(const QString& catKey, int offset);
    void showNcmError(int rc, const QString& op);
    /* 引擎错误可见化：Toast + 状态栏（同因 2s 限频；原样展示运行时文本） */
    void showPlaybackErrorOnce(const QString& msg);
    /* 播放类错误统一出口：上下文（电台/节目/歌曲）+ 后端运行时原文，不改写 */
    void reportPlaybackError(const QString& detail);
    QString playbackContext() const;
    /* rc + 上游 message 的真实错误串 */
    QString ncmErrorDetail(int rc, const QString& json) const;
    QString m_liveTitle;      /* 网络电台正在播的台名 */
    QTimer* m_radioWatchdog = 0;
    void playNcmResolved(const QString& url, const QString& level);
    /* cache-first：点歌入口零网查缓存（sidecar level 匹配 + size 校验）；
     * 命中回 file:// 路径并带出 level 串（业界 InnerTune/Spotify 一致模式） */
    QString findNcmCache(qint64 ncmId, int quality, QString* outLevel);
    void advanceNext(bool fromAuto);   /* fromAuto=false 手动 next 环绕；true 自然/失败推进 */
    bool skipOnError(const QString& why);  /* autoskip 核心：止损+提示+advanceNext；true=已跳播 */
    void updateAutoSkipCheck();        /* Tool 菜单 check 反向同步（updateBackendChecks 范式） */
    void showNcmAuthToast(const QString& op);
    void appendNcmPlaylist(int playlistId /* local */);

    void addMenuItem(void* menu, const QObject* receiver, const char* slot,
                     const char* key, bool darkToggle = false);
    void* addTopMenu(const char* key);
    void* addSubMenu(void* parentMenu, const QString& title, const char* key);
    void addMenuSeparator(void* menu);
    void refreshMenuTexts();
    void setTopMenuText(int index, const QString& text);
    void refreshAppearanceMenuTexts();
    void updateAppearanceTooltips();
    void updateAppearanceMenuChecks();

    void addDemoWidgets();
    void removeDemoWidgets();
    bool hasDemoWidgets() const;

    FramelessHelper* framelessHelper;
    CustomTitleBar* titleBar;
    SystemTrayIcon* tray;
    SideNav* sideNav;
    ContentView* contentView;
    QueuePanel* queuePanel;
    PlayerBar* playerBar;
    QSplitter* bodySplitter;
    std::vector<Playlist> m_library;
    musiccom::FavoriteStore* m_favs;
    musiccom::QueueStore* m_queueStore;   /* 播放队列持久化（queue.json，重启恢复） */    QStringList m_favPendingDetailIds;   /* 账号同步缺快照的 id（批量 song_detail） */
    AudioEngineAdapter* audioEngine;
    int m_curPlaylistId;
    int m_curTrackIndex;
    int m_backendMenuFirst;
    int m_ncmQuality;   /* 当前选定播放音质（NCM_QUALITY_*，设置窗改） */

    /* NCM 状态（全部只属于 UI 线程；队列工作走 NcmService worker） */
    NcmService* m_ncm;
    bool m_ncmAvailable;
    bool m_ncmBusy;
    QString m_ncmCatKey;     /* 当前分类（kHotViewKey=热门歌单） */
    QString m_ncmListTitle;  /* 列表标题 */
    qint64 m_ncmPendingPlaylistId = 0; /* 正在加载的远端歌单 id（曲目回调回填 p.ncmId） */
    bool m_queueRestored = false;      /* 本次启动已恢复队列快照（启动热门请求让位） */
    std::vector<RemotePlaylistCard> m_siteRadioCards;  /* 站点电台列表缓存（节目页返回用） */
    QString m_currentSite;   /* 音乐站点："ncm"(默认) / "ytm"（YouTube Music） */
    QString m_ytmProxy;      /* YTM 代理（默认 http://127.0.0.1:10809；YTM_PROXY 覆盖） */
    bool m_ytmStreaming;     /* 当前播放的是 YTM 直链（http_proxy 已置，收尾清除） */
    QStringList m_ncmAllCats;   /* 暂未使用：备用扁平分类 */
    int m_ncmOffset;
    std::vector<RemotePlaylistCard> m_ncmPendingCards;
    bool m_ncmHasMore;
    qint64 m_ncmSrcPlaylist;    /* 等待 resolve 的本地歌单 */
    int m_ncmSrcTrack;
    /* 播放条行为（Config 持久：player.repeat / player.autoskip） */
    int m_repeatMode;           /* 0=off 1=all 2=one */
    int m_shuffle;              /* 随机乱序 0/1（切曲随机、prev 保持顺序） */
    bool m_autoSkip;            /* 播放失败自动跳下一首（默认开） */
    bool m_liveStream = false;  /* 网络电台直播流：不自动连播/不出进度 */
    int m_errSkipRun;           /* 连续失败计数，成功播放清零 */
    QString m_lastPlaybackError;      /* 同因限频（Toast 降噪） */
    QTime m_lastPlaybackErrorTimer;   /* Qt3 无 QElapsedTimer */
    /* 同一曲失败去重：engine State=4 与 errorOccurred 可能对同一次失败双发 */
    int m_skipGuardPlaylist = -1;
    int m_skipGuardTrack = -1;
    int m_autoSkipMenuIndex;    /* Tool 菜单 check 项（updateBackendChecks 范式） */
    enum { kErrSkipStopAt = 99 };   /* 连续失败止损阈值（foo_skip 惯例量级） */
    /* 音量 + NCM 下载指示（播放条） */
    int m_volume;               /* 0-100，Config player.volume 缺省 100 */
    qint64 m_dlSongId;          /* 当前跟踪下载的 songId，>0 显示 pie */
    int m_dlPct;                /* 0-100；-1=未定（无 Content-Length）；-2=失败（由 bar 存） */
    QTimer* m_volSaveTimer;     /* 500ms Config 写盘 debounce（全文 hjson 重写卡顿根修；日志随写盘拍） */
    QTimer* m_queueSaveTimer;   /* 队列保存 300ms 合并 debounce（切曲多点几次只落一次盘） */
    bool m_queueDirty;          /* 自上次落盘以来队列/当前曲有变 */
    qint64 m_ncmLastToastMs;    /* auth toast 节流 */

    // 演示用状态栏控件（演示菜单可反复增删）
    QLabel* demoLeftLabel;
    QPushButton* demoLeftBtn;
    QLabel* demoRightLabel;
    QPushButton* demoRightBtn;
    QLineEdit* demoEdit;

    QPushButton* m_langBtn;
    QPushButton* m_styleBtn;
    QPushButton* m_darkBtn;
    void* m_langMenu;
    void* m_styleMenu;
    void* m_darkMenu;

    std::vector<AppearanceMenuItemRef> m_appearanceItems;
    std::vector<MenuItemRef> menuItemRefs;
    std::vector<MenuItemRef> topMenuRefs;
    bool forceQuit;
};
#endif // MAINWINDOW_H
