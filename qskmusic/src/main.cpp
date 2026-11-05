#include <QGuiApplication>
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

#include <memory>

int main(int argc, char* argv[])
{
    QGuiApplication app(argc, argv);

    QCoreApplication::setOrganizationName("fedlet");
    QCoreApplication::setApplicationName("qskmusic");

    // ── i18n：按保存/系统语言安装翻译，重译分发由 Lang 单例负责 ──
    Lang::instance();

    app.setWindowIcon(QIcon(QCoreApplication::applicationDirPath()
                            + "/../app_icon.png"));

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
        });

    const int ret = app.exec();

    pageManager->saveAllStates();
    return ret;
}
