#include <QGuiApplication>
#include <QDebug>
#include <QIcon>
#include <QSettings>
#include <QCoreApplication>
#include <QQuickItem>
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
#include "logpage.h"
#include "settingspage.h"
#include "aboutpage.h"
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

    // ── i18n：按保存/系统语言安装翻译，重译分发由 Lang 单例负责 ──
    Lang::instance();

#ifdef Q_OS_MACOS
    // bundle 里 Contents/ 顶层是 code 位置（TN2206），数据文件必须放 Resources/
    app.setWindowIcon(QIcon(QCoreApplication::applicationDirPath()
                            + "/../Resources/app_icon.png"));
#else
    app.setWindowIcon(QIcon(QCoreApplication::applicationDirPath()
                            + "/../app_icon.png"));
#endif

    // ── Skin ──
    auto* skinManager = QskSkinManager::instance();
    {
        QStringList paths = QCoreApplication::libraryPaths();
        paths.prepend("/opt/qt/qskinny/lib/qskinny/plugins");
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

    // ── Root layout ──
    auto* rootBox = new QskLinearBox(Qt::Vertical);
    rootBox->setPanel(true);
    auto* stackBox = new QskStackBox(rootBox);
    stackBox->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Expanding);
    auto* defaultAnimator = new QskStackBoxAnimator4(stackBox);
    stackBox->setAnimator(defaultAnimator);

    // ── PageManager ──
    auto* pageManager = new PageManager(stackBox);

    // ── Register pages ──
    pageManager->registerPage("main", []() -> Page* {
        return new MainPage();
    }, {CachePolicy::Permanent, LaunchMode::SingleInstance});

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

    // ── Start with main page ── (window 已就绪，控件可正常解析 skin hint)
    pageManager->open("main");

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
