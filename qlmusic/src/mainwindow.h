#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "compat34.h"
#include "appearance.h"
#include "musicmodel.h"
#include "contentview.h"
#include "ncm_service.h"
#include "ConfigDialog.h"   /* SettingsChangedMap 类型随 Q_OBJECT 槽声明完整 */
#include <qmainwindow.h>
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

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow(QWidget* parent = 0);
    virtual ~MainWindow();

protected:
    virtual void closeEvent(QCloseEvent* event);
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
    void onSeek(int ms);
    void onAudioPosition(int ms);
    void onAudioDuration(int ms);
    void onAudioState(int state);
    void onAudioFinished();
    void onAudioError(const QString& msg);

    /* NCM（libncm 网易云源）*/
    void onNcmCategorySelected(const QString& catKey);
    void onNavAction(int action);
    void onNcmPlaylistActivated(qint64 ncmPlaylistId);
    void onNcmLoadMore();
    void onNcmBack();
    void onNcmCategories(int rc, const QString& json, int sel);
    void onNcmTopPlaylists(int rc, const QString& json, int sel);
    void onNcmPlaylistTracks(int rc, const QString& json, int localId);
    void onNcmSource(int rc, const QString& json, qint64 songId);
    void onNcmFetched(int rc, const QString& json, qint64 songId);
    void onNcmAuthRequired(const QString& op);

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

    void ensureEngine();
    void playTrack(int playlistId, int trackIndex);
    void updateBackendChecks();

    /* NCM 视图流转（UI 线程，禁止直接调 ncm_* 阻塞调用） */
    void requestTopPlaylists(const QString& catKey, int offset);
    void showNcmError(int rc, const QString& op);
    void playNcmResolved(const QString& url, const QString& level);
    /* cache-first：点歌入口零网查缓存（sidecar level 匹配 + size 校验）；
     * 命中回 file:// 路径并带出 level 串（业界 InnerTune/Spotify 一致模式） */
    QString findNcmCache(qint64 ncmId, int quality, QString* outLevel);
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
    QStringList m_ncmAllCats;   /* 暂未使用：备用扁平分类 */
    int m_ncmOffset;
    std::vector<RemotePlaylistCard> m_ncmPendingCards;
    bool m_ncmHasMore;
    qint64 m_ncmSrcPlaylist;    /* 等待 resolve 的本地歌单 */
    int m_ncmSrcTrack;
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
