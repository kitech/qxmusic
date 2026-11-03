#include "mainwindow.h"

#include "CustomTitleBar.h"
#include "FramelessHelper.h"
#include "EmbeddedMenuBar.h"
#include "lambdaslot.h"
#include "sharedstatusbar.h"
#include "systemtrayicon.h"
#include "translator.h"
#include "ThemeManager.h"
#include "StyleParams.h"
#include "appsetup.h"
#include "config.h"
#include "globaluiutil.h"
#include "version.h"
#include "storage.h"
#include "api.h"
#include "eventpoller.h"
#include "sidenav.h"
#include "contentview.h"
#include "queuepanel.h"
#include "playerbar.h"
#include "audio/AudioEngineAdapter.h"
#include "toastwidget.h"
#include "userinfopanel.h"

#ifdef NCM_ENABLE
#include "ncm.h"
#include "cJSON.h"
#include "limelog.h"
#endif

#include <qpixmap.h>
#include <qevent.h>
#include <qtimer.h>
#include <qmessagebox.h>
#include <qlineedit.h>
#include <qsplitter.h>

#include <cstdlib>
#include <cmath>
#include <fstream>

#include "app_icon.xpm"

static QString resolveBackendId()
{
    const char* env = getenv("QLMUSIC_AUDIO");
    if (env && *env) return qFromUtf8(env);
    QString v = Config::value("audioengine");
    if (v.isEmpty()) v = "auto";
    return v;
}

static QString resolveTrackUrl(const Track& t)
{
    if (!t.url.isEmpty()) return t.url;
    const char* env = getenv("QLMUSIC_SAMPLE");
    if (env && *env) return qFromUtf8(env);
    return Config::value("samplemedia");
}

/* —— 音质映射：展示名 ↔ NCM_QUALITY_* 值 ——
 * VIP 五档（LOSSLESS/HIRES/JYEFFECT/SKY/JYMASTER）免登录 decode 大概率
 * resolve 失败（NO_RESOURCE/TRIAL），失败时由既有 showNcmError 状态栏提示
 * 兜底；完整 VIP 登录鉴权 + 降级重选归 Phase 2（二维码登录 + Cookie）。
 * 接入后仅需在此表增加可用性位并按用户 VIP 状态过滤选项。 */
static const struct {
    const char* zh;
    int value;
} kQualityMap[] = {
    { "标准（128k）",      NCM_QUALITY_STANDARD },
    { "较高（192k）",      NCM_QUALITY_HIGHER   },
    { "极高（320k）",      NCM_QUALITY_EXHIGH   },
    { "无损（VIP）",       NCM_QUALITY_LOSSLESS },
    { "Hi-Res（VIP）",     NCM_QUALITY_HIRES    },
    { "高清环绕声（VIP）", NCM_QUALITY_JYEFFECT },
    { "沉浸环绕（VIP）",   NCM_QUALITY_SKY      },
    { "超清母带（VIP）",   NCM_QUALITY_JYMASTER },
};
static const int kQualityCount = (int)(sizeof(kQualityMap) / sizeof(kQualityMap[0]));

static const char* kDefaultDeviceLabel = "系统默认";

static const char* const kBackendOptionKeys[4] = {
    "audio.backend.auto", "audio.backend.ffmpeg",
    "audio.backend.faad", "audio.backend.mpv"
};

static QString qualityLabelForValue(int v)
{
    for (int i = 0; i < kQualityCount; i++)
        if (kQualityMap[i].value == v) return qFromUtf8(kQualityMap[i].zh);
    return qFromUtf8(kQualityMap[2].zh); /* 缺省回极高档 */
}

static int qualityValueForLabel(const QString& label)
{
    for (int i = 0; i < kQualityCount; i++)
        if (label == qFromUtf8(kQualityMap[i].zh)) return kQualityMap[i].value;
    return NCM_QUALITY_EXHIGH;
}

static QString backendIdForLabel(const QString& label)
{
    static const char* const ids[4] = { "auto", "ffmpeg", "faad", "mpv" };
    for (int i = 0; i < 4; i++)
        if (label == _(qFromUtf8(kBackendOptionKeys[i]))) return qFromUtf8(ids[i]);
    return QString();
}

/* 生成 2s 440Hz 正弦 S16 双声道 WAV（设置窗播放测试用） */
static bool writeTestWav(const QString& path, int seconds)
{
    const int sr = 44100, ch = 2;
    const int dataBytes = sr * ch * 2 * seconds;
    std::ofstream f(qToUtf8(path).data(), std::ios::binary | std::ios::trunc);
    if (!f.is_open()) return false;
    uint32_t sampleRate = sr, byteRate = sr * ch * 2;
    uint16_t blockAlign = ch * 2, bits = 16, fmtTag = 1;
    f.write("RIFF", 4);
    { uint32_t v = 36 + dataBytes; f.write((char*)&v, 4); }
    f.write("WAVEfmt ", 8);
    { uint32_t v = 16; f.write((char*)&v, 4); }
    f.write((char*)&fmtTag, 2);
    f.write((char*)&ch, 2);
    f.write((char*)&sampleRate, 4);
    f.write((char*)&byteRate, 4);
    f.write((char*)&blockAlign, 2);
    f.write((char*)&bits, 2);
    f.write("data", 4);
    { uint32_t v = dataBytes; f.write((char*)&v, 4); }
    for (int i = 0; i < sr * seconds; i++) {
        int16_t s = (int16_t)(sin(2.0 * M_PI * 440.0 * i / sr) * 0.25 * 32767.0);
        f.write((char*)&s, 2);
        f.write((char*)&s, 2);
    }
    return f.good();
}

static const char* const kTopMenuKeys[] = {
    "menu.file", "menu.edit", "menu.tool", "menu.help"
};
static const int kTopMenuKeyCount = (int)(sizeof(kTopMenuKeys) / sizeof(kTopMenuKeys[0]));

static const char* const kLangCodes[3] = { "zh-CN", "zh-TW", "en-US" };

// sticonShowStatusMessage 使用的托盘实例（MainWindow 创建/销毁时维护，不导出）
static SystemTrayIcon* s_trayIcon = 0;

static QPushButton* makeTitleMenuButton(QWidget* parent, const QString& glyph)
{
    QPushButton* b = new QPushButton(glyph, parent);
    b->setFixedSize(30, 30);
    return b;
}

static void attachTitleMenu(QPushButton* btn, MenuWidget34* menu)
{
#ifdef QT3_BUILD
    menu->setCheckable(true);
    btn->setPopup(menu);
#else
    btn->setMenu(menu);
#endif
}

static void initDemoWidgets() {
    SharedStatusBar* bar = SharedStatusBar::instance();
    QLabel* leftLabel = new QLabel(qFromUtf8("Left"), bar);
    bar->addWidget(leftLabel);
    QPushButton* leftBtn = new QPushButton(qFromUtf8("LBtn"), bar);
    leftBtn->setFixedWidth(36);
    bar->addWidget(leftBtn);
    QLabel* rightLabel = new QLabel(qFromUtf8("Right"), bar);
    bar->addPermanentWidget(rightLabel);
    QPushButton* rightBtn = new QPushButton(qFromUtf8("RBtn"), bar);
    rightBtn->setFixedWidth(36);
    bar->addPermanentWidget(rightBtn);
    QLineEdit* demoEdit = new QLineEdit(bar);
    demoEdit->setFixedWidth(100);
#ifndef QT3_BUILD
    demoEdit->setPlaceholderText(qFromUtf8("input..."));
#endif
    bar->addPermanentWidget(demoEdit);
    SharedStatusBar::instance()->showMessage(qFromUtf8("Demo: 左端 widgets 隐藏中..."), 3000);
}

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent
#ifdef QT3_BUILD
        , "qlmusic"
        , Qt::WType_TopLevel | Qt::WStyle_Customize | Qt::WSubWindow
              | Qt::WStyle_MinMax | Qt::WStyle_SysMenu | Qt::WStyle_NoBorder
#endif
#ifndef QT3_BUILD
        , Qt::FramelessWindowHint
#endif
    )
    , framelessHelper(0)
    , titleBar(0)
    , tray(0)
    , sideNav(0)
    , contentView(0)
    , queuePanel(0)
    , playerBar(0)
    , bodySplitter(0)
    , audioEngine(0)
    , m_curPlaylistId(0)
    , m_curTrackIndex(-1)
    , m_backendMenuFirst(0)
    , m_ncmQuality(NCM_QUALITY_EXHIGH)
    , m_ncm(0)
    , m_ncmAvailable(false)
    , m_ncmBusy(false)
    , m_ncmOffset(0)
    , m_ncmHasMore(false)
    , m_ncmSrcPlaylist(0)
    , m_ncmSrcTrack(-1)
    , m_ncmLastToastMs(0)
    , demoLeftLabel(0)
    , demoLeftBtn(0)
    , demoRightLabel(0)
    , demoRightBtn(0)
    , demoEdit(0)
    , m_langBtn(0)
    , m_styleBtn(0)
    , m_darkBtn(0)
    , m_langMenu(0)
    , m_styleMenu(0)
    , m_darkMenu(0)
    , forceQuit(false)
{
    // 初始化本地存储（对照 qldox mainwindow.cpp:513-521）
    QString dataDir = qGetHomePath() + "/.cache/qlmusic";
    Storage::instance().init(
#ifdef QT3_BUILD
        dataDir.utf8()
#else
        dataDir.toUtf8().constData()
#endif
    );

    setGeometry(100, 50, 880, 640);
    buildStatusBar();
    buildCentralWidget();
    buildAppearancePanel();
    buildMenus();
    buildAppearanceMenus();
    buildTray();
    QObject::connect(&Translator::instance(), SIGNAL(languageChanged()),
                     this, SLOT(retranslateUi()));

    /* 设置窗记忆的播放音质还原（audio.conf；缺省极高档） */
    {
        QSettings34 s("qlmusic", "audio");
        int q = qualityValueForLabel(s.valueStr("audio/quality", ""));
        m_ncmQuality = q;
    }

    // 启动事件轮询引擎（对照 qldox mainwindow.cpp:630-631）
    EventPoller::start();
    Api::setEventTarget(this);
}

MainWindow::~MainWindow() {
    s_trayIcon = 0;
#ifdef NCM_ENABLE
    if (m_ncm) m_ncm->shutdown();   /* 先停 worker（join），再走 Qt 清理 */
#endif
    EventPoller::stop();          // 先停泵线程，再关库（qldox mainwindow.cpp:943-945 顺序）
    Storage::instance().close();
}

// 托盘气泡通知：仅显示提示气泡，不触碰 SharedStatusBar（状态栏各管各的）。
void sticonShowStatusMessage(const QString &msg, SticonIcon iconType, int timeout)
{
    if (s_trayIcon && s_trayIcon->isVisible() && SystemTrayIcon::supportsMessages())
        s_trayIcon->showMessage(msg, QString(),
            (SystemTrayIcon::MessageIcon)iconType, timeout);
}

void MainWindow::buildStatusBar()
{
    SharedStatusBar::instance()->show();
    addDemoWidgets();
}

void MainWindow::addDemoWidgets()
{
    if (hasDemoWidgets()) { return; }
    SharedStatusBar* bar = SharedStatusBar::instance();

    demoLeftLabel = new QLabel(qFromUtf8("Left"), bar);
    bar->addWidget(demoLeftLabel);

    demoLeftBtn = new QPushButton(qFromUtf8("LBtn"), bar);
    demoLeftBtn->setFixedWidth(36);
    bar->addWidget(demoLeftBtn);

    demoRightLabel = new QLabel(qFromUtf8("Right"), bar);
    bar->addPermanentWidget(demoRightLabel);

    demoRightBtn = new QPushButton(qFromUtf8("RBtn"), bar);
    demoRightBtn->setFixedWidth(36);
    bar->addPermanentWidget(demoRightBtn);

    demoEdit = new QLineEdit(bar);
    demoEdit->setFixedWidth(100);
#ifndef QT3_BUILD
    demoEdit->setPlaceholderText(qFromUtf8("input..."));
#endif
    bar->addPermanentWidget(demoEdit);

    SharedStatusBar::instance()->showMessage(
        qFromUtf8("Demo: 左端 widgets 隐藏中..."), 3000);
}

void MainWindow::removeDemoWidgets()
{
    SharedStatusBar* bar = SharedStatusBar::instance();
    if (demoLeftLabel)  { bar->removeWidget(demoLeftLabel);  delete demoLeftLabel;  demoLeftLabel = 0; }
    if (demoLeftBtn)    { bar->removeWidget(demoLeftBtn);    delete demoLeftBtn;    demoLeftBtn = 0; }
    if (demoRightLabel) { bar->removeWidget(demoRightLabel); delete demoRightLabel; demoRightLabel = 0; }
    if (demoRightBtn)   { bar->removeWidget(demoRightBtn);   delete demoRightBtn;   demoRightBtn = 0; }
    if (demoEdit)       { bar->removeWidget(demoEdit);       delete demoEdit;       demoEdit = 0; }
}

bool MainWindow::hasDemoWidgets() const
{
    return demoLeftLabel != 0;
}

void MainWindow::buildCentralWidget()
{
    QWidget* central = new QWidget(this);
    QBoxLayout* lay = qNewBoxLayout(central, QBoxLayout::TopToBottom, 0, 0);
    qSetMargins(lay, -1, 0, -1, -1);
    titleBar = new CustomTitleBar(central);
    titleBar->setLabel(_("app_title"));
    lay->addWidget(titleBar, 0);

    bodySplitter = new QSplitter(Qt::Horizontal, central);
    bodySplitter->setOpaqueResize(true);
    sideNav = new SideNav(bodySplitter);
    contentView = new ContentView(bodySplitter);
    queuePanel = new QueuePanel(bodySplitter);
    bodySplitter->addWidget(sideNav);
    bodySplitter->addWidget(contentView);
    bodySplitter->addWidget(queuePanel);
#ifdef QT3_BUILD
    QValueList<int> sizes;
    sizes << 200 << 440 << 240;
#else
    QList<int> sizes;
    sizes << 200 << 440 << 240;
#endif
    bodySplitter->setSizes(sizes);
    lay->addWidget(bodySplitter, 1);

    playerBar = new PlayerBar(central);
    lay->addWidget(playerBar, 0);
    setCentralWidget(central);

    m_library = buildTestLibrary();
    sideNav->setLibrary(m_library);
    contentView->setLibrary(m_library);

    QObject::connect(sideNav, SIGNAL(playlistSelected(int)),
                     contentView, SLOT(showPlaylist(int)));
    QObject::connect(contentView, SIGNAL(trackActivated(int, int)),
                     this, SLOT(onTrackActivated(int, int)));

#ifdef NCM_ENABLE
    m_ncm = new NcmService(this);
    QString ncmDir = qGetHomePath() + "/.cache/qlmusic/ncm";
    qMkdir(ncmDir, true);
    m_ncmAvailable = m_ncm->init(ncmDir);
    ALOG_WARN(qFromUtf8("NCM init: available=%1 dir=%2")
                  .arg(m_ncmAvailable ? 1 : 0).arg(ncmDir));
    if (!m_ncmAvailable) {
        SharedStatusBar::instance()->showMessageTyped(_("ncm.unavailable"),
                                                      StatusWarning, 4000);
    } else {
        QObject::connect(m_ncm, SIGNAL(categoriesLoaded(int, const QString&, int)),
                         this, SLOT(onNcmCategories(int, const QString&, int)));
        QObject::connect(m_ncm, SIGNAL(topPlaylistsLoaded(int, const QString&, int)),
                         this, SLOT(onNcmTopPlaylists(int, const QString&, int)));
        QObject::connect(m_ncm, SIGNAL(playlistTracksLoaded(int, const QString&, int)),
                         this, SLOT(onNcmPlaylistTracks(int, const QString&, int)));
        QObject::connect(m_ncm, SIGNAL(sourceResolved(int, const QString&, qint64)),
                         this, SLOT(onNcmSource(int, const QString&, qint64)));
        QObject::connect(m_ncm, SIGNAL(authRequired(const QString&)),
                         this, SLOT(onNcmAuthRequired(const QString&)));
        /* 启动即拉分类（侧栏），随后自动进入"推荐=热门歌单"视图，
         * 内容区直接展示网络歌单列表，无需手点 */
        m_ncm->loadCategories();
        m_ncmListTitle = _("nav.recommend");
        sideNav->setCurrentCategory(QString("__hot__"));
        requestTopPlaylists(QString("__hot__"), 0);
    }

    QObject::connect(sideNav, SIGNAL(categorySelected(const QString&)),
                     this, SLOT(onNcmCategorySelected(const QString&)));
    /* 侧栏左上用户区（登录占位；Phase 2 接二维码登录）
     * 登录成功后经 sideNav->setUserLoggedIn(昵称, 头像 seed) 更新该区域 */
    QObject::connect(sideNav, SIGNAL(loginRequested()),
                     this, SLOT(onUserLoginPlaceholder()));
    QObject::connect(sideNav, SIGNAL(actionTriggered(int)),
                     this, SLOT(onNavAction(int)));
    QObject::connect(contentView, SIGNAL(playlistActivated(qint64)),
                     this, SLOT(onNcmPlaylistActivated(qint64)));
    QObject::connect(contentView, SIGNAL(loadMoreRequested()),
                     this, SLOT(onNcmLoadMore()));
    QObject::connect(contentView, SIGNAL(backRequested()),
                     this, SLOT(onNcmBack()));
#endif /* NCM_ENABLE */

    if (playerBar) {
        QObject::connect(playerBar, SIGNAL(playPauseRequested()),
                         this, SLOT(onPlayPause()));
        QObject::connect(playerBar, SIGNAL(prevRequested()),
                         this, SLOT(onPrev()));
        QObject::connect(playerBar, SIGNAL(nextRequested()),
                         this, SLOT(onNext()));
        QObject::connect(playerBar, SIGNAL(seekRequested(int)),
                         this, SLOT(onSeek(int)));
    }

    framelessHelper = new FramelessHelper(this);
    framelessHelper->setup(this);
    titleBar->connectFramelessHelper(framelessHelper);
    QObject::connect(titleBar, SIGNAL(appMenuClicked()), titleBar, SLOT(toggleMenu()));
}

void MainWindow::buildAppearancePanel()
{
    QWidget* panel = new QWidget(titleBar);
    QBoxLayout* lay = qNewBoxLayout(panel, QBoxLayout::LeftToRight, 2, 0);
    m_langBtn = makeAppearanceMenuButton(panel, qFromUtf8("文"));
    lay->addWidget(m_langBtn, 0);
    m_styleBtn = makeAppearanceMenuButton(panel, qFromUtf8("饰"));
    lay->addWidget(m_styleBtn, 0);
    m_darkBtn = makeAppearanceMenuButton(panel, qFromUtf8("☾"));
    lay->addWidget(m_darkBtn, 0);
    titleBar->addTitleWidget(panel, 0);
}

void MainWindow::addMenuItem(void* menu, const QObject* receiver, const char* slot,
                             const char* key, bool darkToggle)
{
    QString text = _(qFromUtf8(key));
    MenuItemRef ref;
    ref.key = key;
    ref.darkToggle = darkToggle;
    ref.owner = menu;
#ifdef QT3_BUILD
    QPopupMenu* m = static_cast<QPopupMenu*>(menu);
    ref.id = m->insertItem(text, receiver, slot);
#else
    QMenu* m = static_cast<QMenu*>(menu);
    ref.action = m->addAction(text, receiver, slot);
#endif
    menuItemRefs.push_back(ref);
}

void* MainWindow::addTopMenu(const char* key)
{
    EmbeddedMenuBar* mb = titleBar->menuBar();
    QString title = _(qFromUtf8(key));
    MenuItemRef ref;
    ref.key = key;
    ref.owner = mb;
    void* sub = 0;
#ifdef QT3_BUILD
    QPopupMenu* menu = new QPopupMenu(mb);
    menu->setFont(mb->font());
    ref.id = mb->insertItem(title, menu);
    sub = menu;
#else
    QMenu* menu = mb->addMenu(title);
    sub = menu;
#endif
    topMenuRefs.push_back(ref);
    return sub;
}

void* MainWindow::addSubMenu(void* parentMenu, const QString& title, const char* key)
{
    Q_UNUSED(key);
    EmbeddedMenuBar* mb = titleBar->menuBar();
    void* sub = 0;
#ifdef QT3_BUILD
    QPopupMenu* pm = static_cast<QPopupMenu*>(parentMenu);
    QPopupMenu* subMenu = new QPopupMenu(mb);
    subMenu->setFont(mb->font());
    pm->insertItem(title, subMenu);
    sub = subMenu;
#else
    QMenu* pm = static_cast<QMenu*>(parentMenu);
    QMenu* subMenu = pm->addMenu(title);
    sub = subMenu;
#endif
    return sub;
}

void MainWindow::addMenuSeparator(void* menu)
{
#ifdef QT3_BUILD
    static_cast<QPopupMenu*>(menu)->insertSeparator();
#else
    static_cast<QMenu*>(menu)->addSeparator();
#endif
}

void MainWindow::setTopMenuText(int index, const QString& text)
{
    if (index < 0 || index >= (int)topMenuRefs.size()) { return; }
#ifdef QT3_BUILD
    EmbeddedMenuBar* mb = static_cast<EmbeddedMenuBar*>(topMenuRefs[index].owner);
    mb->changeItem(topMenuRefs[index].id, text);
#else
    EmbeddedMenuBar* mb = titleBar->menuBar();
    QList<QAction*> acts = mb->actions();
    if (index < acts.size()) acts[index]->setText(text);
#endif
}

void MainWindow::refreshMenuTexts()
{
    for (size_t i = 0; i < menuItemRefs.size(); i++) {
        const MenuItemRef& ref = menuItemRefs[i];
        if (!ref.key) continue;
        QString text = _(qFromUtf8(ref.key));
#ifdef QT3_BUILD
        static_cast<QPopupMenu*>(ref.owner)->changeItem(ref.id, text);
#else
        if (ref.action) static_cast<QAction*>(ref.action)->setText(text);
#endif
    }
    for (int i = 0; i < (int)topMenuRefs.size() && i < kTopMenuKeyCount; i++) {
        setTopMenuText(i, _(qFromUtf8(kTopMenuKeys[i])));
    }
}

void MainWindow::buildMenus()
{
    EmbeddedMenuBar* mb = titleBar->menuBar();
    menuItemRefs.clear();
    topMenuRefs.clear();
    MenuWidget34* file = static_cast<MenuWidget34*>(addTopMenu("menu.file"));
    addMenuItem(file, this, SLOT(onMenu1Stub()), "action.new");
    addMenuItem(file, this, SLOT(openSettings()), "menu.settings");
    addMenuSeparator(file);
    addMenuItem(file, this, SLOT(quitApp()), "action.quit");
    MenuWidget34* edit = static_cast<MenuWidget34*>(addTopMenu("menu.edit"));
    addMenuItem(edit, this, SLOT(onMenu1Stub()), "action.undo");
    addMenuItem(edit, this, SLOT(onMenu1Stub()), "action.redo");
    MenuWidget34* tool = static_cast<MenuWidget34*>(addTopMenu("menu.tool"));
    addMenuItem(tool, this, SLOT(onDemoStatusInfo()), "demo.status_info");
    addMenuItem(tool, this, SLOT(onDemoStatusWarning()), "demo.status_warning");
    addMenuItem(tool, this, SLOT(onDemoStatusError()), "demo.status_error");
    addMenuSeparator(tool);
    addMenuItem(tool, this, SLOT(onDemoStatusClear()), "demo.status_clear");
    addMenuItem(tool, this, SLOT(onDemoToggleStatusWidgets()), "demo.status_widgets");
    addMenuSeparator(tool);
    addMenuItem(tool, this, SLOT(onDemoTrayBubble()), "demo.tray_bubble");
    addMenuSeparator(tool);
    addMenuItem(tool, this, SLOT(onDemoApiRequest()), "demo.api_request");
    addMenuSeparator(tool);
    buildBackendMenu(tool);
    addMenuSeparator(tool);
    addMenuItem(tool, this, SLOT(openSettings()), "menu.settings");
    MenuWidget34* help = static_cast<MenuWidget34*>(addTopMenu("menu.help"));
    addMenuItem(help, qApp, SLOT(aboutQt()), "menu.aboutqt");
    addMenuItem(help, this, SLOT(onAboutApp()), "menu.about_qlmusic");
    mb->finalize();
}

void MainWindow::buildAppearanceMenus()
{
    m_langMenu = new MenuWidget34(this);
    static_cast<MenuWidget34*>(m_langMenu)->setMinimumWidth(150);
    {
        const char* const* langKeys = appearanceLangLabelKeys();
        for (int i = 0; i < 3; i++) {
            LambdaSlot* slot = new LambdaSlot(this, [this, i]() {
                onTitleUilang(i);
                updateAppearanceTooltips();
                updateAppearanceMenuChecks();
            });
            addAppearanceMenuItem(&m_appearanceItems, static_cast<MenuWidget34*>(m_langMenu),
                                  slot, SLOT(call()), langKeys[i], 0, i);
        }
    }
    attachTitleMenu(m_langBtn, static_cast<MenuWidget34*>(m_langMenu));

    m_styleMenu = new MenuWidget34(this);
    static_cast<MenuWidget34*>(m_styleMenu)->setMinimumWidth(150);
    {
        const auto& panelStyles = StyleParams::registeredStyles();
        for (int i = 0; i < (int)panelStyles.size(); i++) {
            LambdaSlot* slot = new LambdaSlot(this, [this, i]() {
                onTitleStyle(i);
                updateAppearanceTooltips();
                updateAppearanceMenuChecks();
            });
            addAppearanceMenuItem(&m_appearanceItems, static_cast<MenuWidget34*>(m_styleMenu),
                                  slot, SLOT(call()), panelStyles[i].displayKey, 1, i);
        }
    }
    attachTitleMenu(m_styleBtn, static_cast<MenuWidget34*>(m_styleMenu));

    m_darkMenu = new MenuWidget34(this);
    static_cast<MenuWidget34*>(m_darkMenu)->setMinimumWidth(150);
    {
        LambdaSlot* onSlot = new LambdaSlot(this, [this]() {
            onTitleDark(true);
            updateAppearanceTooltips();
            updateAppearanceMenuChecks();
        });
        addAppearanceMenuItem(&m_appearanceItems, static_cast<MenuWidget34*>(m_darkMenu),
                              onSlot, SLOT(call()), "theme_dark", 2, 0);
        LambdaSlot* offSlot = new LambdaSlot(this, [this]() {
            onTitleDark(false);
            updateAppearanceTooltips();
            updateAppearanceMenuChecks();
        });
        addAppearanceMenuItem(&m_appearanceItems, static_cast<MenuWidget34*>(m_darkMenu),
                              offSlot, SLOT(call()), "theme_light", 2, 1);
    }
    attachTitleMenu(m_darkBtn, static_cast<MenuWidget34*>(m_darkMenu));
    updateAppearanceTooltips();
    updateAppearanceMenuChecks();
}

void MainWindow::buildBackendMenu(void* toolMenu)
{
    void* sub = addSubMenu(toolMenu, _("menu.audio_backend"), "menu.audio_backend");
    const char* const keys[4] = {
        "audio.backend.auto", "audio.backend.ffmpeg",
        "audio.backend.faad", "audio.backend.mpv"
    };
    m_backendMenuFirst = (int)menuItemRefs.size();
    for (int i = 0; i < 4; i++) {
        LambdaSlot* slot = new LambdaSlot(this, [this, i]() { onBackendSelected(i); });
        addMenuItem(sub, slot, SLOT(call()), keys[i]);
#ifndef QT3_BUILD
        if (!menuItemRefs.empty() && menuItemRefs.back().action)
            static_cast<QAction*>(menuItemRefs.back().action)->setCheckable(true);
#endif
    }
    updateBackendChecks();
}

void MainWindow::buildTray()
{
    if (!SystemTrayIcon::isSystemTrayAvailable()) return;
    tray = new SystemTrayIcon(QPixmap(app_icon), this);
    s_trayIcon = tray;
    tray->setToolTip(_("app_title"));
    tray->setVisible(true);
    QObject::connect(tray, SIGNAL(activated(int)), this, SLOT(trayActivated(int)));
    QObject::connect(tray, SIGNAL(messageClicked()), this, SLOT(trayShowMainWindow()));
    PopupMenu* trayMenu = new PopupMenu(this);
    addMenuItem(trayMenu, this, SLOT(trayShowMainWindow()), "tray.open");
    addMenuSeparator(trayMenu);
    addMenuItem(trayMenu, this, SLOT(quitApp()), "tray.quit");
    tray->setContextMenu(trayMenu);
}

void MainWindow::trayActivated(int reason)
{
    if (reason == SystemTrayIcon::Trigger || reason == SystemTrayIcon::DoubleClick) {
        trayShowMainWindow();
    }
}

void MainWindow::trayShowMainWindow()
{
    show();
    raise();
    qActivateWindow(this);
}

void MainWindow::quitApp()
{
    forceQuit = true;
    close();
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (!forceQuit && tray && tray->isVisible()) {
        hide();
        event->ignore();
        return;
    }
    event->accept();
}

void MainWindow::onTitleUilang(int index)
{
    if (index < 0 || index > 2) return;
    QString langCode = qFromUtf8(kLangCodes[index]);
    Config::setValue("uilang", langCode);
    Translator::instance().loadLanguage(langCode);
    QtappSetup::installQtTranslations(langCode);
    updateAppearanceTooltips();
    updateAppearanceMenuChecks();
}

void MainWindow::onTitleStyle(int index)
{
    const std::vector<StyleParams::Definition>& styles = StyleParams::registeredStyles();
    if (index >= 0 && index < (int)styles.size()) {
        ThemeManager::setStyle(styles[index].id, ThemeManager::isDarkMode());
    }
}

void MainWindow::onTitleDark(bool on)
{
    if (on == ThemeManager::isDarkMode()) return;
    ThemeManager::applyTheme(on);
    refreshMenuTexts();
    updateAppearanceTooltips();
    updateAppearanceMenuChecks();
}

void MainWindow::onDarkMenuItem()
{
    onTitleDark(!ThemeManager::isDarkMode());
}

void MainWindow::refreshAppearanceMenuTexts()
{
    for (size_t i = 0; i < m_appearanceItems.size(); i++) {
        const AppearanceMenuItemRef& ref = m_appearanceItems[i];
        if (!ref.key) continue;
        QString text = _(qFromUtf8(ref.key));
#ifdef QT3_BUILD
        static_cast<QPopupMenu*>(ref.owner)->changeItem(ref.id, text);
#else
        if (ref.action) static_cast<QAction*>(ref.action)->setText(text);
#endif
    }
    refreshMenuTexts();
}

void MainWindow::updateAppearanceTooltips()
{
    if (m_langBtn) {
        const char* const* langKeys = appearanceLangLabelKeys();
        int li = appearanceLangIndex(Translator::instance().currentLang());
        qSetToolTip(m_langBtn, _("title.tip_lang") + ": " + qFromUtf8(langKeys[li]));
    }
    if (m_styleBtn) {
        const std::vector<StyleParams::Definition>& styles = StyleParams::registeredStyles();
        int si = 0;
        for (int i = 0; i < (int)styles.size(); i++) {
            if (QString(styles[i].id) == QString(ThemeManager::styleId())) { si = i; break; }
        }
        QString label = (si >= 0 && si < (int)styles.size()) ? _(qFromUtf8(styles[si].displayKey)) : QString();
        qSetToolTip(m_styleBtn, _("title.tip_style") + ": " + label);
    }
    if (m_darkBtn) {
        qSetToolTip(m_darkBtn, _("title.tip_dark") + ": " + (ThemeManager::isDarkMode() ? _("title.dark_on") : _("title.dark_off")));
    }
}

void MainWindow::updateAppearanceMenuChecks()
{
    int cur[3];
    cur[0] = appearanceLangIndex(Translator::instance().currentLang());
    cur[1] = 0;
    {
        const std::vector<StyleParams::Definition>& styles = StyleParams::registeredStyles();
        for (int i = 0; i < (int)styles.size(); i++) {
            if (QString(styles[i].id) == QString(ThemeManager::styleId())) { cur[1] = i; break; }
        }
    }
    cur[2] = ThemeManager::isDarkMode() ? 0 : 1;
    for (size_t i = 0; i < m_appearanceItems.size(); i++) {
        const AppearanceMenuItemRef& ref = m_appearanceItems[i];
        if (ref.group < 0) continue;
        bool on = (ref.index == cur[ref.group]);
#ifdef QT3_BUILD
        static_cast<QPopupMenu*>(ref.owner)->setItemChecked(ref.id, on);
#else
        if (ref.action) static_cast<QAction*>(ref.action)->setChecked(on);
#endif
    }
}

void MainWindow::retranslateUi()
{
    QString title = _("app_title");
    qSetWindowTitle(this, title);
    if (titleBar) titleBar->setLabel(title);
    if (tray) tray->setToolTip(title);
    if (sideNav) sideNav->retranslateUi();
    if (contentView) contentView->retranslateUi();
    if (queuePanel) queuePanel->retranslateUi();
    if (playerBar) playerBar->retranslateUi();
    updateAppearanceTooltips();
    refreshAppearanceMenuTexts();
    updateAppearanceMenuChecks();
}

void MainWindow::ensureEngine()
{
    if (audioEngine) return;
    audioEngine = new AudioEngineAdapter(this);
    QObject::connect(audioEngine, SIGNAL(positionChanged(int)), this, SLOT(onAudioPosition(int)));
    QObject::connect(audioEngine, SIGNAL(durationChanged(int)), this, SLOT(onAudioDuration(int)));
    QObject::connect(audioEngine, SIGNAL(stateChanged(int)), this, SLOT(onAudioState(int)));
    QObject::connect(audioEngine, SIGNAL(finished()), this, SLOT(onAudioFinished()));
    QObject::connect(audioEngine, SIGNAL(errorOccurred(const QString&)),
                     this, SLOT(onAudioError(const QString&)));
    audioEngine->setBackend(resolveBackendId());
    /* 设置窗记忆的输出设备（audio.conf 镜像真源；空=系统默认） */
    QSettings34 s("qlmusic", "audio");
    QString dev = s.valueStr("audio/device", "");
    if (!dev.isEmpty() && dev != qFromUtf8(kDefaultDeviceLabel))
        audioEngine->setOutputDevice(dev);
}

void MainWindow::playTrack(int playlistId, int trackIndex)
{
    const Playlist* pl = findPlaylist(m_library, playlistId);
    if (!pl) return;
    if (trackIndex < 0 || trackIndex >= (int)pl->tracks.size()) return;

    const Track& t = pl->tracks[trackIndex];
    m_curPlaylistId = playlistId;
    m_curTrackIndex = trackIndex;
    playerBar->setTrack(t);
    queuePanel->setCurrentTrack(t);
    queuePanel->setQueue(pl->tracks, trackIndex);
    sideNav->setCurrentPlaylist(playlistId);

#ifdef NCM_ENABLE
    /* NCM 远端曲目：先走 resolveSource（worker 线程），回调里再开播 */
    if (t.ncmId > 0 && m_ncmAvailable) {
        m_ncmSrcPlaylist = playlistId;
        m_ncmSrcTrack = trackIndex;
        SharedStatusBar::instance()->showMessage(_("ncm.loading"), 3000);
        m_ncm->resolveSource(t.ncmId, m_ncmQuality);
        return;
    }
#endif

    QString url = resolveTrackUrl(t);
    if (url.isEmpty()) {
        playerBar->setPlaying(false);
        SharedStatusBar::instance()->showMessage(_("player.no_media"), 4000);
        return;
    }

    ensureEngine();
    audioEngine->open(url);
    audioEngine->play();
}

void MainWindow::onTrackActivated(int playlistId, int trackIndex)
{
    playTrack(playlistId, trackIndex);
}

void MainWindow::onBackendSelected(int index)
{
    static const char* const ids[4] = { "auto", "ffmpeg", "faad", "mpv" };
    if (index < 0 || index > 3) return;
    Config::setValue("audioengine", qFromUtf8(ids[index]));
    if (audioEngine) audioEngine->setBackend(qFromUtf8(ids[index]));
    updateBackendChecks();
    SharedStatusBar::instance()->showMessage(
        _("audio.backend_changed") + qFromUtf8(" ") + qFromUtf8(ids[index]), 3000);
}

void MainWindow::updateBackendChecks()
{
    static const char* const ids[4] = { "auto", "ffmpeg", "faad", "mpv" };
    QString cur = resolveBackendId();
    for (int i = 0; i < 4; i++) {
        int idx = m_backendMenuFirst + i;
        if (idx < 0 || idx >= (int)menuItemRefs.size()) continue;
        const MenuItemRef& ref = menuItemRefs[idx];
        bool on = (cur == qFromUtf8(ids[i]));
#ifdef QT3_BUILD
        static_cast<QPopupMenu*>(ref.owner)->setItemChecked(ref.id, on);
#else
        if (ref.action) static_cast<QAction*>(ref.action)->setChecked(on);
#endif
    }
}

void MainWindow::openSettings()
{
    /* 双入口（Tool 菜单后端子菜单 ↔ 设置窗）共享真源：
     * 后端 = config.json "audioengine"；音质/设备 = audio.conf（QSettings34）。
     * 后端键缺省时惰性 seed，保持两个视图一致。 */
    {
        QSettings34 s("qlmusic", "audio");
        if (!s.contains("audio/backend"))
            s.setValue("audio/backend", QVariant(Config::value("audioengine")));
        s.sync();
    }
    ensureEngine();

    ConfigDialog* dlg = new ConfigDialog(_("settings.window_title"), this);
    CategoryPage* page = new CategoryPage(_("settings.audio_tab"), dlg);

    SelectConfigItem* backendItem = new SelectConfigItem(
        "audio/backend", "auto", "",
        QStringList() << _(qFromUtf8(kBackendOptionKeys[0]))
                      << _(qFromUtf8(kBackendOptionKeys[1]))
                      << _(qFromUtf8(kBackendOptionKeys[2]))
                      << _(qFromUtf8(kBackendOptionKeys[3])), page);
    page->addLabeledControl(_("settings.backend_label"), backendItem->comboBox());

    /* 输出设备：实时枚举系统 playback 设备（cxaudio listOutputDevices） */
    QStringList deviceLabels;
    deviceLabels << qFromUtf8(kDefaultDeviceLabel);
    std::vector<AudioEngineAdapter::OutputDevice> devs = audioEngine->outputDevices();
    for (size_t i = 0; i < devs.size(); i++)
        if (!devs[i].name.isEmpty()) deviceLabels << devs[i].name;
    SelectConfigItem* devItem = new SelectConfigItem(
        "audio/device", qFromUtf8(kDefaultDeviceLabel), "", deviceLabels, page);
    page->addLabeledControl(_("settings.device_label"), devItem->comboBox());

    SelectConfigItem* qualityItem = new SelectConfigItem(
        "audio/quality", qualityLabelForValue(m_ncmQuality), "",
        QStringList() << qFromUtf8("标准（128k）") << qFromUtf8("较高（192k）")
                      << qFromUtf8("极高（320k）") << qFromUtf8("无损（VIP）")
                      << qFromUtf8("Hi-Res（VIP）") << qFromUtf8("高清环绕声（VIP）")
                      << qFromUtf8("沉浸环绕（VIP）") << qFromUtf8("超清母带（VIP）"), page);
    page->addLabeledControl(_("settings.quality_label"), qualityItem->comboBox());

    QPushButton* testBtn = new QPushButton(_("settings.test_btn"), page);
    LambdaSlot* testSlot = new LambdaSlot(this, [this]() { onAudioTestPlay(); });
    connect(testBtn, SIGNAL(clicked()), testSlot, SLOT(call()));
    page->addLabeledControl(_("settings.test_label"), testBtn);

    dlg->addCategory(_("settings.audio_tab"), page);
    dlg->registerConfigItem(backendItem);
    dlg->registerConfigItem(devItem);
    dlg->registerConfigItem(qualityItem);

    dlg->setSettingsName("qlmusic", "audio");
    QObject::connect(dlg, SIGNAL(settingsSaved(const SettingsChangedMap&)),
                     this, SLOT(onSettingsSaved(const SettingsChangedMap&)));
    dlg->loadSettings();
    dlg->exec();
    delete dlg;
}

void MainWindow::onSettingsSaved(const SettingsChangedMap& changed)
{
    if (changed.contains("audio/backend")) {
        QString id = backendIdForLabel(changed["audio/backend"].toString());
        if (!id.isEmpty()) {
            Config::setValue("audioengine", id);
            if (audioEngine) audioEngine->setBackend(id);
            updateBackendChecks();
            SharedStatusBar::instance()->showMessage(
                _("audio.backend_changed") + qFromUtf8(" ") + id, 3000);
        }
    }
    if (changed.contains("audio/quality")) {
        int q = qualityValueForLabel(changed["audio/quality"].toString());
        if (q > 0 && q != m_ncmQuality) {
            m_ncmQuality = q;
            /* VIP 档 resolve 失败由 showNcmError 兜底；Phase 2 接登录鉴权+降级 */
            SharedStatusBar::instance()->showMessage(
                _("settings.quality_changed") + qFromUtf8(" ")
                + changed["audio/quality"].toString()
                + _("settings.next_play"), 3000);
        }
    }
    if (changed.contains("audio/device")) {
        QString v = changed["audio/device"].toString();
        if (audioEngine)
            audioEngine->setOutputDevice(v == qFromUtf8(kDefaultDeviceLabel)
                                             ? QString() : v);
        SharedStatusBar::instance()->showMessage(
            _("settings.device_changed") + qFromUtf8(" ") + v
            + _("settings.next_play"), 3000);
    }
}

void MainWindow::onAudioTestPlay()
{
    ensureEngine();
    /* 440Hz 正弦 2s S16 WAV → 同一条真实链路（当前后端 + 当前输出设备） */
    QString path = qGetHomePath() + "/.cache/qlmusic/audio_test.wav";
    if (!writeTestWav(path, 2)) {
        SharedStatusBar::instance()->showMessage(_("settings.test_gen_fail"), 4000);
        return;
    }
    audioEngine->open(path);
    audioEngine->play();
    SharedStatusBar::instance()->showMessage(
        _("settings.test_playing") + audioEngine->backendId()
        + qFromUtf8("）…"), 3000);
}

void MainWindow::onUserLoginPlaceholder()
{
    /* Phase 2：二维码登录（login/qrcode_generate/qrcode_check + Cookie 落盘）；
     * 登录成功后经 userPanel->setLoggedIn(昵称, 头像 seed) 更新右上角区域 */
    ToastWidget::showCenter(this, qFromUtf8("登录功能开发中（Phase 2）"),
                            ToastWidget::Info, 2500);
}

void MainWindow::onPlayPause()
{
    if (audioEngine) audioEngine->toggle();
}

void MainWindow::onPrev()
{
    if (m_curTrackIndex > 0) playTrack(m_curPlaylistId, m_curTrackIndex - 1);
}

void MainWindow::onNext()
{
    const Playlist* pl = findPlaylist(m_library, m_curPlaylistId);
    if (!pl) return;
    if (m_curTrackIndex + 1 < (int)pl->tracks.size())
        playTrack(m_curPlaylistId, m_curTrackIndex + 1);
}

void MainWindow::onSeek(int ms)
{
    if (audioEngine) audioEngine->seek(ms);
}

void MainWindow::onAudioPosition(int ms)
{
    playerBar->setProgress(ms);
}

void MainWindow::onAudioDuration(int ms)
{
    playerBar->setDuration(ms);
}

void MainWindow::onAudioState(int state)
{
    playerBar->setPlaying(state == cxaudio::Playing);
}

void MainWindow::onAudioFinished()
{
    onNext();
}

void MainWindow::onAudioError(const QString& msg)
{
    playerBar->setPlaying(false);
    SharedStatusBar::instance()->showMessage(
        _("player.error") + qFromUtf8(": ") + msg, 5000);
}

void MainWindow::onMenu1Stub() {}
void MainWindow::onDemoStatusInfo() { SharedStatusBar::instance()->showMessageTyped(_("demo.status_info"), StatusInfo, 3000); }
void MainWindow::onDemoStatusWarning() { SharedStatusBar::instance()->showMessageTyped(_("demo.status_warning"), StatusWarning, 3000); }
void MainWindow::onDemoStatusError() { SharedStatusBar::instance()->showMessageTyped(_("demo.status_error"), StatusError, 3000); }
void MainWindow::onDemoStatusClear() { SharedStatusBar::instance()->clearMessage(); }
void MainWindow::onDemoToggleStatusWidgets()
{
    if (hasDemoWidgets()) {
        removeDemoWidgets();
        SharedStatusBar::instance()->showMessage(_("demo.status_clear"), 3000);
    } else {
        addDemoWidgets();
        SharedStatusBar::instance()->showMessage(_("demo.ready"), 3000);
    }
}
void MainWindow::onDemoTrayBubble()
{
    if (!tray || !tray->isVisible() || !SystemTrayIcon::supportsMessages()) {
        SharedStatusBar::instance()->showMessage(_("demo.tray_bubble"), 3000);
        return;
    }
    tray->showMessage(_("app_title"), _("demo.tray_bubble"), SystemTrayIcon::Information, 5000);
}
void MainWindow::onAboutApp()
{
    QString text = QString("<h3>qlmusic %1</h3><p>qlmusic shell (Qt Widgets)</p><p>Qt: %2</p>").arg(APP_VERSION_FULL).arg(qVersion());
    QMessageBox::about(this, _("menu.about_qlmusic"), text);
}

void MainWindow::onDemoApiRequest()
{
    HttpRequest req("http://localhost:8181/api/self", "GET", "", 10);
    Api::request(req, ApiGetSelf);
}

void MainWindow::customEvent(CustomEventBase* event)
{
    if (event->type() == NcmResultType) {
#ifdef NCM_ENABLE
        if (m_ncm)
            m_ncm->handleResult(static_cast<NcmResultEvent*>(event));
#endif
        return;   // postEvent 投递的事件由事件循环自动删除，不手动 delete
    }
    if (event->type() == ApiResultReadyType) {
        ApiHttpResultEvent* e = static_cast<ApiHttpResultEvent*>(event);
        if (e->resp.curlErrStr.empty()) {
            stbarShowStatusMessage(qFromUtf8("HTTP ") + QString::number(e->resp.httpCode),
                                   SticonInfo, 3000);
        } else {
            stbarShowStatusMessage(
                qFromUtf8("HTTP ") + QString::number(e->resp.httpCode) + " " +
                    qFromUtf8(e->resp.curlErrStr),
                SticonCritical, 8000);
        }
        return;   // postEvent 投递的事件由事件循环自动删除，不手动 delete
    }
    QMainWindow::customEvent(event);
}

#ifdef NCM_ENABLE

#include <ctime>

/* Qt3 QString::utf8()（QCString）与 Qt4+ QString::toUtf8() 的统一入口，
 * 供 cJSON_Parse 使用 */
static const char* cjsonUtf8(const QString& s)
{
#ifdef QT3_BUILD
    return s.utf8();
#else
    return s.toUtf8().constData();
#endif
}

/* ───────────────────────── NCM（libncm）编排 ─────────────────────────
 * UI 线程只做：发起请求 + 解析回投结果 + 三处提示。
 * 阻塞的网络调用全部在 NcmService 的 worker 线程里。
 */

static QString cJSONStr(cJSON* obj, const char* key, const QString& def)
{
    if (!obj) return def;
    cJSON* n = cJSON_GetObjectItem(obj, key);
    if (!n || n->type != cJSON_String || !n->valuestring) return def;
    return qFromUtf8(n->valuestring);
}

static qint64 cJSONInt(cJSON* obj, const char* key, qint64 def)
{
    if (!obj) return def;
    cJSON* n = cJSON_GetObjectItem(obj, key);
    if (!n) return def;
    if (n->type == cJSON_Number) return (qint64)n->valuedouble;
    return def;
}

static bool cJSONBool(cJSON* obj, const char* key, bool def)
{
    if (!obj) return def;
    cJSON* n = cJSON_GetObjectItem(obj, key);
    if (!n) return def;
    if (n->type == cJSON_True) return true;
    if (n->type == cJSON_False) return false;
    if (n->type == cJSON_Number) return n->valueint != 0;
    return def;
}

static QStringList cJSONStrArray(cJSON* arr)
{
    QStringList out;
    if (!arr) return out;
    for (int i = 0; i < cJSON_GetArraySize(arr); ++i) {
        cJSON* it = cJSON_GetArrayItem(arr, i);
        if (it && it->type == cJSON_String && it->valuestring)
            out.append(qFromUtf8(it->valuestring));
    }
    return out;
}

void MainWindow::onNcmCategories(int rc, const QString& json, int /*sel*/)
{
    if (rc != NCM_OK) {
        /* 静默：离线/未实现时只是侧栏没有分类树 */
        return;
    }
    cJSON* root = cJSON_Parse(cjsonUtf8(json));
    if (!root) return;

    QStringList hot = cJSONStrArray(cJSON_GetObjectItem(root, "hot"));
    QStringList groupNames;
    QStringList groupCounts;
    QStringList matrix;
    cJSON* groups = cJSON_GetObjectItem(root, "groups");
    for (int i = 0; groups && i < cJSON_GetArraySize(groups); ++i) {
        cJSON* g = cJSON_GetArrayItem(groups, i);
        if (!g) continue;
        groupNames.append(cJSONStr(g, "name", ""));
        cJSON* cats = cJSON_GetObjectItem(g, "categories");
        int cnt = 0;
        for (int c = 0; cats && c < cJSON_GetArraySize(cats); ++c) {
            cJSON* it = cJSON_GetArrayItem(cats, c);
            if (it && it->type == cJSON_String && it->valuestring) {
                matrix.append(qFromUtf8(it->valuestring));
                ++cnt;
            }
        }
        groupCounts.append(QString::number(cnt));
    }
    m_ncmAllCats = hot;
    for (int i = 0; i < matrix.size(); ++i)
        m_ncmAllCats.append(matrix[i]);
    cJSON_Delete(root);

    if (sideNav)
        sideNav->setCategories(hot, groupNames, groupCounts, matrix);
}

void MainWindow::requestTopPlaylists(const QString& catKey, int offset)
{
    if (!m_ncmAvailable) {
        SharedStatusBar::instance()->showMessageTyped(_("ncm.unavailable"),
                                                      StatusWarning, 4000);
        return;
    }
    if (m_ncmBusy) return;
    m_ncmBusy = true;

    /* "__hot__" 是 sidenav 的热门歌单视图哨兵 → 服务端 cat=全部 order=hot */
    m_ncmCatKey = catKey;
    m_ncmOffset = offset;
    if (offset == 0) {
        m_ncmHasMore = false;
        m_ncmPendingCards.clear();
        if (contentView)
            contentView->showPlaylistList(m_ncmListTitle, m_ncmPendingCards, false, true);
    }

    QString cat = (catKey == QString("__hot__")) ? _("ncm.cat_all") : catKey;
    const char* order = "hot";
    m_ncm->loadTopPlaylists(cat, qFromUtf8(order), 30, offset);
}

void MainWindow::onNcmTopPlaylists(int rc, const QString& json, int /*sel*/)
{
    m_ncmBusy = false;
    if (rc != NCM_OK) {
        showNcmError(rc, _("nav.recommend"));
        if (contentView)
            contentView->setListLoading(false);
        return;
    }

    cJSON* root = cJSON_Parse(cjsonUtf8(json));
    if (!root) {
        showNcmError(NCM_ERR_UPSTREAM, _("nav.recommend"));
        if (contentView)
            contentView->setListLoading(false);
        return;
    }
    cJSON* arr = cJSON_GetObjectItem(root, "playlists");
    for (int i = 0; arr && i < cJSON_GetArraySize(arr); ++i) {
        cJSON* it = cJSON_GetArrayItem(arr, i);
        if (!it) continue;
        RemotePlaylistCard card;
        card.id = cJSONInt(it, "id", 0);
        card.name = cJSONStr(it, "name", QString::number((long long)card.id));
        card.creator = cJSONStr(it, "creator", "");
        card.coverUrl = cJSONStr(it, "cover", "");
        card.trackCount = (int)cJSONInt(it, "track_count", 0);
        card.playCount = cJSONInt(it, "play_count", 0);
        m_ncmPendingCards.push_back(card);
    }
    m_ncmHasMore = cJSONBool(root, "more", false);
    cJSON_Delete(root);

    /* 服务端 offset = 已取条数（load more 追加语义） */
    m_ncmOffset = (int)m_ncmPendingCards.size();
    if (contentView)
        contentView->showPlaylistList(m_ncmListTitle, m_ncmPendingCards,
                                      m_ncmHasMore && !m_ncmPendingCards.empty(), false);
}

void MainWindow::onNcmPlaylistActivated(qint64 ncmPlaylistId)
{
    if (ncmPlaylistId <= 0) return;

    /* 已拉取过：直接展示 */
    for (size_t i = 0; i < m_library.size(); ++i) {
        if (m_library[i].ncmId == ncmPlaylistId) {
            int lid = m_library[i].id;
            contentView->showPlaylist(lid);
            sideNav->setCurrentPlaylist(lid);
            return;
        }
    }

    if (!m_ncmAvailable) {
        SharedStatusBar::instance()->showMessageTyped(_("ncm.unavailable"),
                                                      StatusWarning, 4000);
        return;
    }
    if (m_ncmBusy) return;
    m_ncmBusy = true;
    SharedStatusBar::instance()->showMessage(_("ncm.loading"), 8000);

    /* 新分配一个不与现有本地歌单冲突的 id 给这个远端歌单 */
    int localId = 1;
    for (size_t i = 0; i < m_library.size(); ++i)
        if (m_library[i].id >= localId) localId = m_library[i].id + 1;
    m_ncm->loadPlaylistTracks(ncmPlaylistId, localId);
}

void MainWindow::onNcmPlaylistTracks(int rc, const QString& json, int localId)
{
    m_ncmBusy = false;
    if (rc != NCM_OK) {
        showNcmError(rc, _("nav.recommend"));
        return;
    }
    cJSON* root = cJSON_Parse(cjsonUtf8(json));
    if (!root) {
        showNcmError(NCM_ERR_UPSTREAM, _("nav.recommend"));
        return;
    }
    cJSON* pl = cJSON_GetObjectItem(root, "playlist");
    Playlist p;
    p.id = localId;
    p.name = pl ? cJSONStr(pl, "name", _("nav.recommend")) : _("nav.recommend");
    p.coverSeed = pl ? cJSONStr(pl, "cover", "") : "";
    p.creator = pl ? cJSONStr(pl, "creator", "") : "";
    p.coverUrl = p.coverSeed;
    p.trackCount = pl ? (int)cJSONInt(pl, "track_count", 0) : 0;

    static int nextTrackId = 100;
    cJSON* tracks = cJSON_GetObjectItem(root, "tracks");
    for (int i = 0; tracks && i < cJSON_GetArraySize(tracks); ++i) {
        cJSON* it = cJSON_GetArrayItem(tracks, i);
        if (!it) continue;
        QString name = cJSONStr(it, "name", "");
        if (name.isEmpty()) continue;
        QString artists;
        cJSON* art = cJSON_GetObjectItem(it, "artists");
        for (int a = 0; art && a < cJSON_GetArraySize(art); ++a) {
            cJSON* ai = cJSON_GetArrayItem(art, a);
            if (ai && ai->type == cJSON_String && ai->valuestring) {
                if (!artists.isEmpty()) artists += qFromUtf8("/");
                artists += qFromUtf8(ai->valuestring);
            }
        }
        Track t = makeNcmTrack(nextTrackId++,
                               cJSONInt(it, "id", 0),
                               name, artists,
                               cJSONStr(it, "album", ""),
                               (int)cJSONInt(it, "duration_ms", 0),
                               cJSONStr(it, "cover", ""),
                               cJSONInt(it, "album_id", 0));
        t.playable = cJSONBool(it, "playable", true);
        p.tracks.push_back(t);
    }
    p.trackCount = p.trackCount > 0 ? p.trackCount : (int)p.tracks.size();
    cJSON_Delete(root);

    m_library.push_back(p);
    sideNav->setLibrary(m_library);
    contentView->setLibrary(m_library);
    contentView->showPlaylist(p.id);
    sideNav->setCurrentPlaylist(p.id);
}

void MainWindow::onNcmSource(int rc, const QString& json, qint64 /*songId*/)
{
    if (rc != NCM_OK) {
        showNcmError(rc, _("ncm.playing"));
        return;
    }
    cJSON* root = cJSON_Parse(cjsonUtf8(json));
    if (!root) {
        showNcmError(NCM_ERR_UPSTREAM, _("ncm.playing"));
        return;
    }
    QString url = cJSONStr(root, "url", "");
    QString level = cJSONStr(root, "level", "");
    cJSON_Delete(root);

    Playlist* pl = 0;
    for (size_t i = 0; i < m_library.size(); ++i)
        if (m_library[i].id == m_ncmSrcPlaylist) { pl = &m_library[i]; break; }
    if (!pl || m_ncmSrcTrack < 0 || m_ncmSrcTrack >= (int)pl->tracks.size()) return;

    if (url.isEmpty()) {
        (*pl).tracks[m_ncmSrcTrack].url = QString();
        showNcmError(NCM_ERR_NO_RESOURCE, _("ncm.playing"));
        return;
    }
    (*pl).tracks[m_ncmSrcTrack].url = url;
    const Track& t = (*pl).tracks[m_ncmSrcTrack];

    ensureEngine();
    audioEngine->open(url);
    audioEngine->play();
    SharedStatusBar::instance()->showMessage(
        _("ncm.loaded_quality") + qFromUtf8(" ") + level, 3000);
}

void MainWindow::onNcmLoadMore()
{
    if (m_ncmBusy || m_ncmCatKey.isEmpty()) return;
    requestTopPlaylists(m_ncmCatKey, m_ncmOffset);
}

void MainWindow::onNcmBack()
{
    /* 列表/宫格返回：回到分类宫格（若无分类数据则回热门歌单） */
    if (sideNav)
        sideNav->setCurrentCategory(QString("__hot__"));
    if (contentView && !m_ncmAllCats.isEmpty()) {
        contentView->showCategoryGrid(m_ncmAllCats);
        return;
    }
    m_ncmListTitle = _("nav.recommend");
    requestTopPlaylists(QString("__hot__"), 0);
}

void MainWindow::onNcmCategorySelected(const QString& catKey)
{
    m_ncmListTitle = (catKey == QString("__hot__")) ? _("nav.recommend") : catKey;
    requestTopPlaylists(catKey, 0);
}

void MainWindow::onNavAction(int action)
{
    if (action == 0) {
        /* 排行榜：暂缓，Phase 2 */
        SharedStatusBar::instance()->showMessage(_("nav.rank_coming"), 3000);
        return;
    }
    if (action == 1) {
        /* 我的音乐：需要登录（Phase 2），提示即回 */
        showNcmAuthToast(_("nav.mymusic"));
        return;
    }
}

void MainWindow::showNcmError(int rc, const QString& op)
{
    switch (rc) {
    case NCM_ERR_UNAUTHORIZED:
        showNcmAuthToast(op);
        return;
    case NCM_ERR_NETWORK:
        SharedStatusBar::instance()->showMessageTyped(_("ncm.error_network"),
                                                      StatusError, 5000);
        ToastWidget::showCenter(this, _("ncm.error_network"), ToastWidget::Error, 2500);
        return;
    case NCM_ERR_CAPTCHA:
        ToastWidget::showCenter(this, _("ncm.error_captcha"), ToastWidget::Warning, 3000);
        return;
    case NCM_ERR_GEO_BLOCK:
        ToastWidget::showCenter(this, _("ncm.error_geo"), ToastWidget::Warning, 3000);
        return;
    case NCM_ERR_NOT_FOUND:
        ToastWidget::showCenter(this, _("ncm.error_not_found"), ToastWidget::Warning, 3000);
        return;
    case NCM_ERR_NO_RESOURCE:
        ToastWidget::showCenter(this, _("ncm.error_no_resource"), ToastWidget::Error, 3000);
        return;
    case NCM_ERR_TRIAL_ONLY:
        ToastWidget::showCenter(this, _("ncm.error_trial"), ToastWidget::Warning, 3000);
        return;
    default:
        ToastWidget::showCenter(this, _("ncm.error_generic"), ToastWidget::Error, 3000);
        return;
    }
}

void MainWindow::showNcmAuthToast(const QString& op)
{
    Q_UNUSED(op);
    /* 认证要求：状态栏 + 托盘气泡 + 屏幕中央 toast，5 秒节流 */
    qint64 now = (qint64)time(0);
    if (m_ncmLastToastMs > 0 && now - m_ncmLastToastMs < 5) return;
    m_ncmLastToastMs = now;

    SharedStatusBar::instance()->showMessageTyped(_("ncm.auth_required"),
                                                  StatusWarning, 5000);
    sticonShowStatusMessage(_("ncm.auth_required"), SticonWarning, 4000);
    ToastWidget::showCenter(this, _("ncm.auth_required"), ToastWidget::Warning, 2500);
}

void MainWindow::onNcmAuthRequired(const QString& /*op*/)
{
    showNcmAuthToast(_("nav.recommend"));
}

#else /* !NCM_ENABLE：libncm 不可用，仅保留符号满足 moc */

void MainWindow::onNcmCategorySelected(const QString&) {}
void MainWindow::onNavAction(int) {}
void MainWindow::onNcmPlaylistActivated(qint64) {}
void MainWindow::onNcmLoadMore() {}
void MainWindow::onNcmBack() {}
void MainWindow::onNcmCategories(int, const QString&, int) {}
void MainWindow::onNcmTopPlaylists(int, const QString&, int) {}
void MainWindow::onNcmPlaylistTracks(int, const QString&, int) {}
void MainWindow::onNcmSource(int, const QString&, qint64) {}
void MainWindow::onNcmAuthRequired(const QString&) {}

#endif /* NCM_ENABLE */
