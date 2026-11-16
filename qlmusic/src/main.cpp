#include "compat34.h"
#include "mainwindow.h"
#include "iconloader.h"
#include "translator.h"
#include "appsetup.h"
#include "LimeStyle.h"
#include "ThemeManager.h"
#include "globaluiutil.h"
#include "sharedstatusbar.h"
#include "config.h"
#include "version.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
/* Ctrl-C/SIGTERM 兜底：Qt3/4 不自带 Unix 信号处理，进程直接死会跳过保存。
 * 处理函数只写 socketpair（async-signal-safe），Qt 侧经 QSocketNotifier
 * 触发 quit()，保存统一在 exec 返回后的 main() 里做。 */
#ifndef Q_OS_WIN
#include <signal.h>
#include <sys/socket.h>
#include <qsocketnotifier.h>
#include "lambdaslot.h"

static int g_sigFd[2] = { -1, -1 };
static void qlSigHandler(int)
{
    if (g_sigFd[1] >= 0) {
        char c = 1;
        ssize_t r = write(g_sigFd[1], &c, 1);
        Q_UNUSED(r);
    }
}
#endif
#ifdef __linux__
#include <malloc.h>
#endif

static QString loadSavedLanguage() {
    return Config::value("uilang");
}

void stbarShowStatusMessage(const QString &msg, int timeout)
{
    if (!SharedStatusBar::instanceExists()) { return; }
    SharedStatusBar::instance()->showMessage(msg, timeout);
}

void stbarShowStatusMessage(const QString &msg, SticonIcon type, int timeout)
{
    if (!SharedStatusBar::instanceExists()) { return; }
    SharedStatusBar::instance()->showMessageTyped(msg, (StatusMessageType)type, timeout);
}

#include "app_icon.xpm"

int main(int argc, char* argv[]) {
#ifdef __linux__
    mallopt(M_MMAP_THRESHOLD, 32768);
    mallopt(M_ARENA_MAX, 2);
#endif
#if defined(__APPLE__)
    /* mac 双击 .app 启动的 stdout/stderr 不入终端。
     * isatty 分流：Terminal 直跑（stderr 是 TTY）→ 控制台直出；
     * 双击/open 启动（非 TTY）→ stderr 落盘 ~/Library/Logs/qlmusic.log，
     * [cxaudio]/[ncm]/ALOG/qDebug 全量可见。 */
    if (!isatty(fileno(stderr))) {
        const char* home = std::getenv("HOME");
        if (home && *home) {
            char path[512];
            snprintf(path, sizeof(path), "%s/Library/Logs/qlmusic.log", home);
            if (std::freopen(path, "a", stderr)) setvbuf(stderr, 0, _IOLBF, 0);
        }
    }
#endif
    setenv("QT_IM_MODULE", "xim", true);
    setenv("XMODIFIERS", "@im=fcitx", true);

    bool showVersion = false;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0) {
            showVersion = true;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            printf("Usage: qlmusic [options]\n"
                   "Options:\n"
                   "  -v, --version    Show version and exit\n"
                   "  -h, --help       Show this help\n");
            return 0;
        }
    }

    QApplication app(argc, argv);
#ifndef QT3_BUILD
    app.setApplicationVersion(APP_VERSION);
#endif
#if defined(__APPLE__) && QT_VERSION >= 0x040000
    /* Qt4 的 jpeg/gif 等是运行时 imageformats 插件：bundle 内没有 → 所有 jpg
     * 解码失败（封面 corrupt-on-disk 刷屏）。两处补搜：bundle 内 PlugIns +
     * macports Qt4 插件目录（build 目录直跑时的兜底）。 */
    QCoreApplication::addLibraryPath(
        QCoreApplication::applicationDirPath() + QString::fromUtf8("/../PlugIns"));
    QCoreApplication::addLibraryPath(
        QString::fromUtf8("/opt/local/libexec/qt4/share/qt4/plugins"));
#endif

    if (showVersion) {
        printf("qlmusic %s\n", APP_VERSION);
        return 0;
    }

    QtappSetup::setup(app);
    app.setStyle(new LimeStyle);
    ThemeManager::setStyle("qtFusion", true);

    QString savedLang = loadSavedLanguage();
    if (savedLang.isEmpty()) savedLang = "zh-CN";

    Translator::instance().addTranslationPath(app.applicationDirPath() + "/../lang");
    Translator::instance().addTranslationPath(app.applicationDirPath() + "/lang");
    Translator::instance().loadLanguage(savedLang);
    QtappSetup::installQtTranslations(savedLang);

    QtappSetup::setQuitOnExit(false);

    MainWindow window;
    qSetAppIcon(app_icon);
    qSetWindowTitle(&window, _("app_title"));
    window.show();
    IconLoader::instance().trimCache();
    qSetAppIcon(app_icon);

    QtappSetup::setQuitOnExit(true);

#ifndef Q_OS_WIN
    /* 信号 → quit 管道桥（正常退出/Ctrl-C/SIGTERM 全部汇入 exec 返回后的统一保存） */
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, g_sigFd) == 0) {
        signal(SIGINT, qlSigHandler);
        signal(SIGTERM, qlSigHandler);
        QSocketNotifier* sigSn =
            new QSocketNotifier(g_sigFd[0], QSocketNotifier::Read, &window);
        LambdaSlot* sigSlot = new LambdaSlot(&window, [sigSn, &app]() {
            sigSn->setEnabled(false);          /* 读一次即撤（防抖） */
            char c;
            ssize_t r = read(g_sigFd[0], &c, 1);
            Q_UNUSED(r);
            app.quit();
        });
        QObject::connect(sigSn, SIGNAL(activated(int)), sigSlot, SLOT(call()));
    }
#endif

    const int rc = app.exec();
    /* 退出统一兜底：closeEvent 已存则幂等重写；菜单 quit / Ctrl-C 在此补存 */
    window.saveQueueState();
    return rc;
}
