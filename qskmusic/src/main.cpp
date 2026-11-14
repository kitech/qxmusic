#include <QGuiApplication>
#include <QDebug>
#include <QIcon>
#include <QSettings>
#include <QDateTime>
#include <QCoreApplication>
#include <QQuickItem>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QskDialog.h>
#include <QskSkinManager.h>
#include <QskSkin.h>
#include <QskWindow.h>
#include <QskLinearBox.h>
#include <QskStackBox.h>
#include <QskStackBoxAnimator.h>
#include "myi18n.h"
#include "pagemanager.h"
#include "mainpage.h"
#include "playlistlistpage.h"
#include "playlisttrackspage.h"
#include "favoritespage.h"
#include "radiopage.h"
#include "favorites.h"
#include "searchpage.h"
#include "logpage.h"
#include "settingspage.h"
#include "aboutpage.h"
#include "playerbar.h"
#include "progressline.h"
#include <QskControl.h>
#include "queuepopup.h"
#include "lyricpopup.h"
#include "playback.h"
#include "ncmapi.h"
#include "downloadservice.h"
#include "toastpopup.h"
#include "tabfocus.h"
#include "keepalive.h"
#include "networkmonitor.h"
#include "androidutils.h"
#include "envprobe.h"
#include "iconloader.h"
#include "iconmem.h"
#include <QKeyEvent>
#include <QElapsedTimer>
#include <QTimer>
#include <QQuickWindow>
#include <QMouseEvent>
#include <QTouchEvent>
#include <QskPopup.h>
#include <QskTextInput.h>
#include "playback.h"

#include <memory>
#include <thread>

extern "C" void gosoMainLoop();
extern "C" void csoMainLoop();

namespace {

// 全局键过滤器（规则见 docs/空格键播控规则.md）：
//   返回键 —— 栈深>1 回退页面栈；栈底 2s 内双击才退出（防误退）
//   空格   —— 播放/暂停（文本输入且内容非空时放行输入；空框=播控）
//   媒体键 —— 永远播控（输入中亦可，无歧义通道）
//   点击非输入区域 —— 清除输入焦点（Qt Quick 默认不清 activeFocusItem）
class GlobalKeyFilter : public QObject
{
public:
    GlobalKeyFilter(PageManager* pm, QObject* parent = nullptr)
        : QObject(parent), m_pageManager(pm) {}

protected:
    bool eventFilter(QObject* obj, QEvent* event) override
    {
        auto* win = qobject_cast<QQuickWindow*>(obj);

        // DIAG: 指针链探针（定位"行点击无响应"根因，结论后撤）
        if (event->type() == QEvent::MouseButtonPress
            || event->type() == QEvent::TouchBegin) {
            QPointF p;
            if (event->type() == QEvent::MouseButtonPress)
                p = static_cast<QMouseEvent*>(event)->position();
            else if (!static_cast<QTouchEvent*>(event)->points().isEmpty())
                p = static_cast<QTouchEvent*>(event)->points().first().position();
            QQuickItem* hit = win ? win->contentItem()->childAt(p.x(), p.y()) : nullptr;
            QString chain;
            for (QQuickItem* it = hit; it; it = it->parentItem())
                chain += QString::fromUtf8(it->metaObject()->className())
                    + QStringLiteral(" < ");
            qDebug() << "[DIAG] press pos=" << p << "chain=" << chain
                     << "focus=" << (win && win->activeFocusItem()
                            ? win->activeFocusItem()->metaObject()->className() : "none");
        }

        if (event->type() == QEvent::KeyPress) {
            auto* ke = static_cast<QKeyEvent*>(event);
            if (ke->key() == Qt::Key_Back) {
                event->accept();

                if (m_pageManager->depth() > 1) {
                    m_pageManager->back();
                } else if (m_backTimer.isValid() && m_backTimer.elapsed() < 2000) {
                    QCoreApplication::quit();
                } else {
                    m_backTimer.start();
                    showAndroidToast(QStringLiteral("Press again to exit"));
                }
                return true;
            }

            const bool space = ke->key() == Qt::Key_Space
                && ke->modifiers() == Qt::NoModifier;
            const bool media = ke->key() == Qt::Key_MediaTogglePlayPause;
            if (space || media) {
                if (hasOpenModalPopup(win))
                    return false;                       // 弹层内交给弹层
                QQuickItem* f = win ? win->activeFocusItem() : nullptr;
                if (space && isTextInputFocus(f)
                    && !focusText(f).trimmed().isEmpty())
                    return false;                       // 输入内容非空：打字
                Playback::instance().togglePause();     // 空框/其他焦点：播控
                return true;                            // 吞：防按钮二次响应
            }
        } else if (event->type() == QEvent::MouseButtonPress
                   || event->type() == QEvent::TouchBegin) {
            // 焦点释放：点非输入区域 → 清输入焦点
            if (win) {
                QQuickItem* f = win->activeFocusItem();
                if (isTextInputFocus(f) && !hitIsInside(win, event, f))
                    f->setFocus(false);
            }
        }
        return false;
    }

private:
    // 文本输入判定（含 QskTextField 内嵌 QQuickTextInput 编辑态焦点）
    static bool isTextInputFocus(QQuickItem* item)
    {
        for (QQuickItem* it = item; it; it = it->parentItem()) {
            if (qobject_cast<QskTextInput*>(it)) return true;
            if (it->inherits("QQuickTextInput")
                || it->inherits("QQuickTextEdit")) return true;
        }
        return false;
    }

    static QString focusText(QQuickItem* item)
    {
        for (QQuickItem* it = item; it; it = it->parentItem())
            if (auto* ti = qobject_cast<QskTextInput*>(it))
                return ti->text();
        return QString();
    }

    static bool hasOpenModalPopup(QQuickWindow* win)
    {
        if (!win) return false;
        for (auto* p : win->findChildren<QskPopup*>())
            if (p->isOpen() && p->isModal()) return true;
        return false;
    }

    static bool hitIsInside(QQuickWindow* win, QEvent* event, QQuickItem* item)
    {
        QPointF pos;
        if (event->type() == QEvent::MouseButtonPress)
            pos = static_cast<QMouseEvent*>(event)->position();
        else {
            const auto pts = static_cast<QTouchEvent*>(event)->points();
            if (pts.isEmpty()) return true;   // 无坐标：保守认为在控件内
            pos = pts.first().position();
        }
        QQuickItem* hit = win->contentItem()->childAt(pos.x(), pos.y());
        for (QQuickItem* it = hit; it; it = it->parentItem())
            if (it == item) return true;
        return false;
    }

    PageManager* m_pageManager;
    QElapsedTimer m_backTimer;
};

} // namespace

int main(int argc, char* argv[])
{
    QGuiApplication app(argc, argv);

    installLogHandler();

    QCoreApplication::setOrganizationName("fedlet");
    QCoreApplication::setApplicationName("qskmusic");

#ifdef Q_OS_MACOS
    // mac 上 QSettings(NativeFormat) 走 CFPreferences 异步 daemon 缓存，落盘无保证
    //（析构虽自动 sync 仍丢失）；强制 IniFormat 走文件原子写 = 与 Linux 行为一致，
    // 确保保存及时可靠。必须早于任何 QSettings 构造。
    QSettings::setDefaultFormat(QSettings::IniFormat);
    // 定向 mac 落盘到 ~/Library/Application Support（层次浅、可写），
    // 规避 $HOME/.config 在 mac 上不可写的问题。必须早于任何 QSettings 构造。
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
        QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation));
    // mac 诊断探针回环：确认 QSettings 是否真的能写盘（写→sync→查→删）
    {
        QSettings probe;
        probe.setValue("__diagProbe", QDateTime::currentDateTime().toString(Qt::ISODate));
        probe.sync();
        qDebug() << "[qskmusic] settings probe writable:"
                 << probe.contains("__diagProbe");
        probe.remove("__diagProbe");
        probe.sync();
    }
#endif

    // ── i18n：按保存/系统语言安装翻译，重译分发由 Lang 单例负责 ──
    Lang::instance();

#ifndef Q_OS_ANDROID
#  ifdef Q_OS_MACOS
    // bundle 里 Contents/ 顶层是 code 位置（TN2206），数据文件必须放 Resources/
    app.setWindowIcon(QIcon(QCoreApplication::applicationDirPath()
                            + "/../Resources/app_icon.png"));
#  else
    app.setWindowIcon(QIcon(QCoreApplication::applicationDirPath()
                            + "/../app_icon.png"));
#  endif
#endif

    // ── Android：皮肤插件提取 ──
    // JNI lib 目录是平铺的（无 skins/ 子目录），QskSkinManager 按
    // <pluginPath>/skins/ 约定找皮肤插件，直接从 lib 目录找不到。
    // 把皮肤插件 .so 拷到 AppLocalDataLocation/skins/ 并 addLibraryPath，
    // 否则 setSkin 加载不到插件，后续 skin() 崩溃。
#ifdef Q_OS_ANDROID
    const auto dataDir = QStandardPaths::writableLocation(
        QStandardPaths::AppLocalDataLocation);
    qDebug() << "[qskmusic] AppLocalDataLocation:" << dataDir;

    const auto libPaths = QCoreApplication::libraryPaths();
    qDebug() << "[qskmusic] libraryPaths:" << libPaths;

    for (const auto& libPath : libPaths) {
        QDir srcDir(libPath);
        qDebug() << "[qskmusic] checking path:" << libPath
                 << "exists=" << srcDir.exists()
                 << "has_qskinny=" << srcDir.exists("libqskinny_arm64-v8a.so");

        if (!srcDir.exists("libqskinny_arm64-v8a.so"))
            continue;

        QDir().mkpath(dataDir + "/skins");
        for (const auto& name : {"libfusionskin", "libfluent2skin",
                                  "libmaterial3skin"}) {
            const QString fileName = name + QString("_arm64-v8a.so");
            const QString dst = dataDir + "/skins/" + fileName;

            if (QFile::exists(dst)) {
                qDebug() << "[qskmusic] already exists, skip:" << dst;
                continue;
            }

            const QString src = srcDir.absoluteFilePath(fileName);
            qDebug() << "[qskmusic] copying" << src << "->" << dst;

            if (QFile::copy(src, dst)) {
                QFile(dst).setPermissions(
                    QFile::ReadOwner | QFile::ExeOwner |
                    QFile::ReadUser  | QFile::ExeUser);
                qDebug() << "[qskmusic] copy OK";
            } else {
                qWarning() << "[qskmusic] copy FAILED:" << src;
            }
        }
        break;
    }
    QCoreApplication::addLibraryPath(dataDir);
    qDebug() << "[qskmusic] added libraryPath:" << dataDir;
#endif

    // ── Skin ──
    auto* skinManager = QskSkinManager::instance();
    {
        QStringList paths = QCoreApplication::libraryPaths();
#ifndef Q_OS_ANDROID
        paths.prepend("/opt/qt/qskinny/lib/qskinny/plugins");
#endif
        skinManager->setPluginPaths(paths);
    }
    skinManager->setSkin("Fusion");

    {
        // 恢复持久化外观设置（与 SettingsPage 的 QSettings 键对齐）
        QSettings s;
        int skinIdx = s.value("skin", 0).toInt();
        if (skinIdx > 0 && skinIdx < 3) {
            static const char* names[] = { "Fusion", "Fluent2", "Material3" };
            skinManager->setSkin(names[skinIdx]);
        }
        const bool dark = s.value("darkMode", true).toBool();
        if (dark) {
            auto* skin = skinManager->skin();
            if (skin) skin->setColorScheme(QskSkin::DarkScheme);
        }
    }

    qDebug() << "[qskmusic] skin OK:" << skinManager->skinName();
    qDebug() << "[qskmusic] DPI:" << QGuiApplication::primaryScreen()->logicalDotsPerInch()
             << "dpr:" << QGuiApplication::primaryScreen()->devicePixelRatio();

    // ── Root layout（anystik 同款）──
    auto* rootBox = new QskLinearBox(Qt::Vertical);
    rootBox->setPanel(true);
    auto* stackBox = new QskStackBox(rootBox);
    stackBox->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Expanding);
    auto* defaultAnimator = new QskStackBoxAnimator4(stackBox);
    stackBox->setAnimator(defaultAnimator);
    /* 恢复转场选择（anystik 同口径：0=Slide 1=2D 2=3D 3=Perspective） */
    {
        const int t = QSettings().value("transition", 3).toInt();
        QskStackBoxAnimator* a = nullptr;
        if (t == 0) a = new QskStackBoxAnimator1(stackBox);
        else if (t == 1) a = new QskStackBoxAnimator2(stackBox);
        else if (t == 2) a = new QskStackBoxAnimator3(stackBox);
        if (a) stackBox->setAnimator(a);
    }

    // ── PageManager ──
    auto* pageManager = new PageManager(stackBox);

    // 本地收藏（favorites.json）+ 登录态双向同步（失败静默）
    Favorites::instance().init(QStandardPaths::writableLocation(
        QStandardPaths::AppLocalDataLocation));

    // ── 全局唯一播放条 + 队列/歌词弹层（qlmusic 主窗体底部播放条正統）──
    auto* playerBar = new PlayerBar(rootBox);
    // 全宽细线进度（贴播放条上沿，业界 mini-player 惯例）：
    // QskControl 容器参与 Qsk 布局（Expanding/Fixed 3dp），自绘项作为其子项铺满
    auto* lineBox = new QskControl(rootBox);
    lineBox->setPreferredHeight(3);
    lineBox->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Fixed);
    lineBox->setAutoLayoutChildren(true);
    auto* progressLine = new ProgressLineItem(lineBox);
    progressLine->setColors(QColor(0x4a, 0x4a, 0x4a),   // 底槽（空进度可见）
                            QColor(0x3d, 0x8b, 0xfd));  // 播放：蓝
    playerBar->attachProgressLine(progressLine);
    {   // 插到播放条之前（rootBox 顺序：stackBox | lineBox | playerBar）
        const int idx = rootBox->indexOf(playerBar);
        if (idx >= 0) {
            rootBox->removeItem(lineBox);
            rootBox->insertItem(idx, lineBox);
        }
    }

    // 下载进度线（新增）：播放条下方，常显；无进度=底槽色，有下载=绿色填充
    auto* dlLineBox = new QskControl(rootBox);       // 创建在 playerBar 之后 = 下方
    dlLineBox->setPreferredHeight(3);
    dlLineBox->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Fixed);
    dlLineBox->setAutoLayoutChildren(true);
    auto* dlLine = new ProgressLineItem(dlLineBox);
    dlLine->setColors(QColor(0x4a, 0x4a, 0x4a),      // 底槽
                      QColor(0x4c, 0xaf, 0x50));     // 下载：绿
    playerBar->attachDownloadLine(dlLine);
    auto* queuePopup = new QueuePopup(rootBox);
    auto* lyricPopup = new LyricPopup(rootBox);
    auto& pb = Playback::instance();
    QObject::connect(&pb, &Playback::playingChanged, playerBar, &PlayerBar::setPlaying);
    // 播放状态 → Android 前台服务（WakeLock 熄屏不断播 / 通知文案）
    QObject::connect(&pb, &Playback::playingChanged, playerBar,
        [](bool playing) { KeepAlive::setPlaybackActive(playing); });

    // ── 阶段2b：锁屏/通知元数据 + 播放状态（进度 1s 节流）──
    QObject::connect(&pb, &Playback::songChanged, &pb, []() {
        const Song* s = Playback::instance().currentSong();
        KeepAlive::setMetadata(s ? s->name : QString(),
                               s ? s->artists : QString(),
                               s ? s->durationMs : 0);
        KeepAlive::setPlaybackState(KeepAlive::StateBuffering, 0, 1.0f);
    });
    QObject::connect(&pb, &Playback::playingChanged, &pb, [](bool playing) {
        KeepAlive::setPlaybackState(
            playing ? KeepAlive::StatePlaying : KeepAlive::StatePaused,
            Playback::instance().positionMs(), 1.0f);
    });
    auto* mediaTick = new QTimer(&pb);
    mediaTick->setInterval(1000);
    QObject::connect(mediaTick, &QTimer::timeout, &pb, []() {
        Playback& p = Playback::instance();
        if (p.isPlaying())
            KeepAlive::setPlaybackState(KeepAlive::StatePlaying, p.positionMs(), 1.0f);
    });
    QObject::connect(&pb, &Playback::playingChanged, mediaTick,
        [mediaTick](bool playing) {
            if (playing)
                mediaTick->start(1000);
            else
                mediaTick->stop();
        });
    QObject::connect(&pb, &Playback::progressMsChanged, playerBar, &PlayerBar::setProgressMs);
    QObject::connect(&pb, &Playback::durationMsChanged, playerBar, &PlayerBar::setDurationMs);
    QObject::connect(&pb, &Playback::playError, playerBar, [playerBar](const QString& msg) {
        ToastPopup::show(playerBar, msg);
    });
    QObject::connect(playerBar, &PlayerBar::playPauseRequested, &pb, &Playback::togglePause);
    QObject::connect(playerBar, &PlayerBar::prevRequested, &pb, &Playback::prev);
    QObject::connect(playerBar, &PlayerBar::nextRequested, &pb, &Playback::next);
    QObject::connect(playerBar, &PlayerBar::volumeClicked, playerBar, [playerBar]() {
        ToastPopup::show(playerBar,
            QCoreApplication::translate("main", "Volume panel: coming soon"));
    });
    QObject::connect(playerBar, &PlayerBar::queueRequested, playerBar, [&pb, queuePopup]() {
        QStringList names;
        QStringList covers;
        QStringList seeds;
        for (const auto& s : pb.queue()) {
            names << (s.artists.isEmpty() ? s.name
                                          : QString("%1 - %2").arg(s.name, s.artists));
            covers << s.cover;
            seeds << QString::number(s.id);
        }
        queuePopup->setTracks(names);
        queuePopup->setTrackCovers(covers, seeds);
        queuePopup->setCurrentIndex(pb.currentIndex());
        if (!queuePopup->isOpen())
            queuePopup->open();
    });
    QObject::connect(queuePopup, &QueuePopup::trackSelected, &pb, &Playback::playIndex);
    // 队列开着时切歌 → 刷新 ▶ 位置
    QObject::connect(&pb, &Playback::songChanged, queuePopup, [&pb, queuePopup]() {
        if (queuePopup->isOpen())
            queuePopup->setCurrentIndex(pb.currentIndex());
    });
    QObject::connect(playerBar, &PlayerBar::lyricRequested, playerBar, [&pb, lyricPopup]() {
        qDebug() << "[DIAG] lyricRequested songId=" << pb.currentSongId()
                 << "cached=" << pb.lyricCached(pb.currentSongId())
                 << "open=" << lyricPopup->isOpen();
        if (pb.currentSongId() == 0) {
            ToastPopup::show(lyricPopup,
                QCoreApplication::translate("main", "Play a song first"));
            return;
        }
        if (pb.lyricCached(pb.currentSongId())) {
            lyricPopup->setLyric(pb.currentTitle(), pb.lrcText());
        } else {
            lyricPopup->setLyric(pb.currentTitle(),
                QCoreApplication::translate("main", "Loading…"));
            // 预取未到位/失败的兜底：立即再拉一次
            NcmApi::instance().lyricAsync(pb.currentSongId());
        }
        lyricPopup->relayout();      // 先定几何（首帧即正确位置）
        if (!lyricPopup->isOpen())
            lyricPopup->open();
        // 打开后再补一次（窗口尺寸此时才最终可用）
        QTimer::singleShot(0, lyricPopup, [lyricPopup]() { lyricPopup->relayout(); });
    });
    QObject::connect(&pb, &Playback::songChanged, lyricPopup, [&pb, lyricPopup]() {
        if (lyricPopup->isOpen()) {
            lyricPopup->setLyric(pb.currentTitle(),
                pb.lyricCached(pb.currentSongId()) ? pb.lrcText()
                    : QCoreApplication::translate("main", "Loading…"));
        }
    });

    // 歌词到达 → 弹层实时刷新（修复"永远加载中"）
    QObject::connect(&pb, &Playback::lyricReady, lyricPopup,
        [&pb, lyricPopup](qint64 songId, const QString& text) {
            if (!lyricPopup->isOpen() || songId != pb.currentSongId())
                return;
            lyricPopup->setLyric(pb.currentTitle(), text);
        });

    // 播放进度 → 当前行高亮 + 自动滚动
    QObject::connect(&pb, &Playback::progressMsChanged, lyricPopup,
        [lyricPopup](int ms) { lyricPopup->setProgressMs(ms); });

    // 当前歌名/歌手（qlmusic 播放条同款）+ 下载进度转发
    QObject::connect(&pb, &Playback::songChanged, playerBar, [&pb, playerBar]() {
        const Song* s = pb.currentSong();
        if (s) {
            playerBar->setSongInfo(s->name, s->artists);
            playerBar->setCover(s->cover, QString::number(s->id));
            // 歌手头像：先显示首字母兜底，再异步取真实头像
            playerBar->setArtistAvatar(QString(), s->artists);
            if (!s->artistIds.isEmpty())
                NcmApi::instance().artistAsync(s->artistIds.first());
        }
    });
    QObject::connect(&NcmApi::instance(), &NcmApi::artistDone, playerBar,
        [playerBar](qint64, const QString& picUrl, const QString&) {
            const Song* s = Playback::instance().currentSong();
            playerBar->setArtistAvatar(picUrl, s ? s->artists : QString());
        });
    QObject::connect(&DownloadService::instance(), &DownloadService::progress,
        playerBar, [playerBar](qint64, int pct) {
            playerBar->setDownloadProgress(pct);
        });

    // ── Register pages ──
    pageManager->registerPage("home", []() -> Page* {
        return new HomePage();
    }, {CachePolicy::Permanent, LaunchMode::SingleInstance});

    pageManager->registerPage("playlists", []() -> Page* {
        return new PlaylistListPage();
    }, {CachePolicy::Transient, LaunchMode::Standard});

    pageManager->registerPage("tracks", []() -> Page* {
        return new PlaylistTracksPage();
    }, {CachePolicy::Transient, LaunchMode::Standard});

    pageManager->registerPage("favorites", []() -> Page* {
        return new FavoritesPage();
    }, {CachePolicy::Transient, LaunchMode::Standard});

    pageManager->registerPage("radioPrograms", []() -> Page* {
        return new RadioPage();
    }, {CachePolicy::Transient, LaunchMode::Standard});

    pageManager->registerPage("search", []() -> Page* {
        return new SearchPage();
    }, {CachePolicy::Transient, LaunchMode::Standard});

    pageManager->registerPage("settings", []() -> Page* {
        return new SettingsPage();
    }, {CachePolicy::Permanent, LaunchMode::Standard});

    pageManager->registerPage("about", []() -> Page* {
        return new AboutPage();
    }, {CachePolicy::Transient, LaunchMode::Standard});

    pageManager->registerPage("logs", []() -> Page* {
        return new LogPage();
    }, {CachePolicy::Transient, LaunchMode::Standard});

    // ── Window ──
    QskWindow window;
    window.setTitle("qskmusic");
    window.addItem(rootBox);

    // Tab/Shift+Tab 手动焦点管理（QSkinny 默认关闭 tab focus 链）
    window.installEventFilter(new MyTabFocus(&window));

    // 全局键过滤（返回键 + 空格播控 + 焦点释放）
    app.installEventFilter(new GlobalKeyFilter(pageManager));

    window.setPreferredSize({1024, 680});
    window.show();

    // ── 运行环境探测：[ENV] 快照日志 + LogModel（App 内 Logs 页可见）──
    EnvProbe::instance().detect();
    IconLoader::instance().trimCache();   // 启动后异步修剪封面缓存
#ifdef Q_OS_ANDROID
    qximage::PixmapCache::instance().setMaxBytes(12LL * 1024 * 1024);
#else
    qximage::PixmapCache::instance().setMaxBytes(16LL * 1024 * 1024);
#endif

    // ── Start with home page ── (window 已就绪，控件可正常解析 skin hint)
    pageManager->open("home");

    // ── Application lifecycle → sync QSettings ──
    QObject::connect(&app, &QGuiApplication::applicationStateChanged,
        [](Qt::ApplicationState state) {
            if (state == Qt::ApplicationInactive
             || state == Qt::ApplicationSuspended) {
                QSettings().sync();
            }
#ifdef Q_OS_ANDROID
            if (state == Qt::ApplicationActive) {
                NetworkMonitor::setForeground(true);
                NetworkMonitor::checkNetwork();
            } else {
                NetworkMonitor::setForeground(false);
            }
            qDebug() << "[ENV] app-state ->" << state;
#endif
        });

    // ── Android 保活 + 网络监听 ──
#ifdef Q_OS_ANDROID
    KeepAlive::start();
    NetworkMonitor::start();
    qDebug() << "[qskmusic] KeepAlive service started";
#endif

    std::thread(gosoMainLoop).detach();
    std::thread(csoMainLoop).detach();
    qDebug() << "[qskmusic] extras threads started";

    const int ret = app.exec();

    pageManager->saveAllStates();
    return ret;
}
