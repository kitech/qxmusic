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
#include "searchpage.h"
#include "logpage.h"
#include "settingspage.h"
#include "aboutpage.h"
#include "playerbar.h"
#include "queuepopup.h"
#include "lyricpopup.h"
#include "playback.h"
#include "toastpopup.h"
#include "tabfocus.h"
#include "keepalive.h"
#include "networkmonitor.h"
#include "androidutils.h"
#include <QKeyEvent>
#include <QElapsedTimer>

#include <memory>
#include <thread>

extern "C" void gosoMainLoop();
extern "C" void csoMainLoop();

namespace {

// Android 返回键：栈深>1 回退页面栈；栈底 2s 内双击才退出（防误退）
class BackButtonFilter : public QObject
{
public:
    BackButtonFilter(PageManager* pm, QObject* parent = nullptr)
        : QObject(parent), m_pageManager(pm) {}

protected:
    bool eventFilter(QObject*, QEvent* event) override
    {
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
        }
        return false;
    }

private:
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

    // ── PageManager ──
    auto* pageManager = new PageManager(stackBox);

    // ── 全局唯一播放条 + 队列/歌词弹层（qlmusic 主窗体底部播放条正統）──
    auto* playerBar = new PlayerBar(rootBox);
    auto* queuePopup = new QueuePopup(rootBox);
    auto* lyricPopup = new LyricPopup(rootBox);
    auto& pb = Playback::instance();
    QObject::connect(&pb, &Playback::playingChanged, playerBar, &PlayerBar::setPlaying);
    QObject::connect(&pb, &Playback::progressMsChanged, playerBar, &PlayerBar::setProgressMs);
    QObject::connect(&pb, &Playback::durationMsChanged, playerBar, &PlayerBar::setDurationMs);
    QObject::connect(&pb, &Playback::playError, playerBar, [playerBar](const QString& msg) {
        ToastPopup::show(playerBar, msg);
    });
    QObject::connect(playerBar, &PlayerBar::playPauseRequested, &pb, &Playback::togglePause);
    QObject::connect(playerBar, &PlayerBar::prevRequested, &pb, &Playback::prev);
    QObject::connect(playerBar, &PlayerBar::nextRequested, &pb, &Playback::next);
    QObject::connect(playerBar, &PlayerBar::volumeClicked, playerBar, [playerBar]() {
        ToastPopup::show(playerBar, QString::fromUtf8("音量弹层：下轮实装"));
    });
    QObject::connect(playerBar, &PlayerBar::queueRequested, playerBar, [&pb, queuePopup]() {
        QStringList names;
        for (const auto& s : pb.queue())
            names << (s.artists.isEmpty() ? s.name
                                          : QString("%1 - %2").arg(s.name, s.artists));
        queuePopup->setTracks(names);
        queuePopup->setCurrentIndex(pb.currentIndex());
        if (!queuePopup->isOpen())
            queuePopup->open();
    });
    QObject::connect(queuePopup, &QueuePopup::trackSelected, &pb, &Playback::playIndex);
    QObject::connect(playerBar, &PlayerBar::lyricRequested, playerBar, [&pb, lyricPopup]() {
        if (pb.currentSongId() == 0) {
            ToastPopup::show(lyricPopup, QString::fromUtf8("先播放一首歌"));
            return;
        }
        if (pb.lyricCached(pb.currentSongId()))
            lyricPopup->setLyric(pb.currentTitle(), pb.lrcText());
        else
            lyricPopup->setLyric(pb.currentTitle(), QString::fromUtf8("歌词加载中…"));
        if (!lyricPopup->isOpen())
            lyricPopup->open();
    });
    QObject::connect(&pb, &Playback::songChanged, lyricPopup, [&pb, lyricPopup]() {
        if (lyricPopup->isOpen()) {
            lyricPopup->setLyric(pb.currentTitle(),
                pb.lyricCached(pb.currentSongId()) ? pb.lrcText()
                    : QString::fromUtf8("歌词加载中…"));
        }
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

    // Android 返回键接管
    app.installEventFilter(new BackButtonFilter(pageManager));

    window.setPreferredSize({1024, 680});
    window.show();

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
