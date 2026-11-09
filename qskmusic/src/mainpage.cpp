#include "mainpage.h"
#include "pagemanager.h"
#include "menuoverlay.h"
#include "playerbar.h"
#include "navbar.h"
#include "lyricpopup.h"
#include "categoryview.h"
#include "recview.h"
#include "queuepopup.h"
#include "toastpopup.h"
#include "logmodel.h"
#include "audioplayer.h"
#include <QskLinearBox.h>
#include <QskTextLabel.h>
#include <QskPushButton.h>
#include <QskScrollArea.h>
#include <QskStackBox.h>
#include <QskTextField.h>
#include <QskSeparator.h>
#include <QskFontRole.h>
#include <QskMenu.h>
#include <QskLabelData.h>
#include <QskPopup.h>
#include <QQuickWindow>
#include <QUrl>

MainPage::MainPage(QQuickItem* parent)
    : Page(parent)
{
    // 桩数据：仅「喜欢/最近/下载」三个 tab 用；「歌曲」tab 走 libncm 真实搜索
    for (int i = 1; i <= 20; ++i)
        m_titles << QString("示例歌曲 %1").arg(i);
}

void MainPage::onCreate(const QVariantMap&, const QVariantMap&)
{
    setAutoLayoutChildren(true);
    auto* layout = new QskLinearBox(Qt::Vertical, this);
    layout->setPanel(true);

    // ── 顶栏 ──
    auto* topBar = new QskLinearBox(Qt::Horizontal, layout);
    topBar->setPanel(true);
    topBar->setPreferredHeight(56);

    auto* title = new QskTextLabel("qskmusic", topBar);
    title->setFontRole(QskFontRole(QskFontRole::Title, QskFontRole::Normal));
    title->setSizePolicy(QskSizePolicy::Preferred, QskSizePolicy::Preferred);
    title->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);

    // 搜索为次要入口（qlmusic 侧栏搜索同定位）：顶栏 🔍 → 独立搜索页
    auto* searchBtn = new QskPushButton(QString::fromUtf8("🔍"), topBar);
    searchBtn->setPreferredSize(40, 40);
    connect(searchBtn, &QskAbstractButton::clicked, this, [this]() {
        if (auto* pm = pageManager()) pm->open("search");
    });

    // 顶栏只留一个「☰」：3 个文字按钮在窄屏会溢出被裁（QSkinny 按钮
    // 默认 Minimum 策略不可收缩）。菜单范式照 anystik showOptionsMenu。
    auto* menuBtn = new QskPushButton(QString::fromUtf8("☰"), topBar);
    menuBtn->setPreferredSize(40, 40);
    connect(menuBtn, &QskAbstractButton::clicked, this, [this, menuBtn]() {
        showOptionsMenu(menuBtn->mapToItem(this, QPointF(0, menuBtn->height())));
    });

    new QskSeparator(Qt::Horizontal, layout);

    // ── 主体（anystik 思路单列：顶栏 → tab 内容 → 播放条 → 底栏）──
    // 不做双导航条/宽窄屏切换：Android 启动时窗口宽度多次跳变，
    // orientation 重排会踩 Qt 6.7.3 render 线程 clip batch 空指针
    auto* contentCol = new QskLinearBox(Qt::Vertical, layout);
    contentCol->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Expanding);

    // 3 个 tab 子视图（对齐 qlmusic SideNav 顶级入口：推荐/分类/我的）
    m_contentStack = new QskStackBox(contentCol);
    m_contentStack->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Expanding);
    for (int i = 0; i < 3; ++i) {
        if (i == 0) {
            // 「推荐」tab：启动即拉热门歌单（qlmusic 启动口径）
            m_recView = new RecView(m_contentStack);
            connect(m_recView, &RecView::playlistSelected, this,
                [this](qint64 playlistId, const QString& name) {
                    if (auto* pm = pageManager()) {
                        QVariantMap args;
                        args.insert("playlistId", playlistId);
                        args.insert("title", name);
                        pm->open("tracks", args);
                    }
                });
            m_tabViews.append(m_recView);
        } else if (i == 1) {
            // 「分类」tab：qlmusic 分类树（push 歌单列表页）
            m_categoryView = new CategoryView(m_contentStack);
            connect(m_categoryView, &CategoryView::categorySelected, this,
                [this](const QString& cat) {
                    if (auto* pm = pageManager()) {
                        QVariantMap args;
                        args.insert("cat", cat);
                        args.insert("title", cat.isEmpty()
                            ? QString::fromUtf8("热门歌单") : cat);
                        pm->open("playlists", args);
                    }
                });
            m_tabViews.append(m_categoryView);
        } else {
            auto* rows = new QskLinearBox(Qt::Vertical);
            rows->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Minimum);
            auto* scroll = new QskScrollArea(m_contentStack);
            scroll->setScrolledItem(rows);
            scroll->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Expanding);
            buildStubList(rows);
            m_tabViews.append(scroll);
        }
    }
    m_contentStack->setCurrentItem(m_tabViews.first());

    m_playerBar = new PlayerBar(contentCol);

    // ── libncm 信号接线（NcmApi/AudioPlayer 信号均跨线程队列到主线程）──
    auto& ncm = NcmApi::instance();
    connect(&ncm, &NcmApi::resolveDone, this,
        [this](qint64 songId, const QString& url, const QString& err) {
            m_resolving = false;
            if (songId != currentSongId()) return;   // 已切歌，丢弃
            if (!err.isEmpty() || url.isEmpty()) {
                ToastPopup::show(this, err == QStringLiteral("NoResource")
                    ? QString::fromUtf8("灰色歌曲，无可播资源")
                    : QString::fromUtf8("获取播放链接失败：%1").arg(err));
                return;
            }
            AudioPlayer::instance()->playUrl(QString::number(songId), QUrl(url));
            m_playing = true;
            m_playerBar->setPlaying(true);
            NcmApi::instance().lyricAsync(songId);   // 歌词预取
        });
    connect(&ncm, &NcmApi::lyricDone, this,
        [this](qint64 songId, const QString& lrc, const QString&) {
            m_lrcSongId = songId;
            m_lrcText = lrc;
            if (m_lyricPopup && m_lyricPopup->isOpen() && songId == currentSongId())
                m_lyricPopup->setLyric(currentTitle(), lrc);
        });
    connect(&ncm, &NcmApi::playFailReceived, this,
        [this](qint64, const QString& reason) {
            ToastPopup::show(this, QString::fromUtf8("播放失败：%1，切下一首").arg(reason));
            playSong(m_currentTrack + 1);
        });
    auto* ap = AudioPlayer::instance();
    connect(ap, &AudioPlayer::finished, this, [this](const QString& id) {
        m_playing = false;
        m_playerBar->setPlaying(false);
        if (id.toLongLong() != currentSongId()) return;   // 非本页队列（子页面在播）
        playSong(m_currentTrack + 1);   // 自然播完自动下一首（越界则停）
    });
    connect(ap, &AudioPlayer::positionChanged, this,
        [this](qint64 ms) { m_playerBar->setProgressMs(int(ms)); });
    connect(ap, &AudioPlayer::durationChanged, this,
        [this](qint64 ms) { m_playerBar->setDurationMs(int(ms)); });

    // ── 底部导航（窄屏形态；宽屏由 modeChanged 隐藏）──
    m_navBottom = new NavBar(layout);
    connect(m_navBottom, &NavBar::currentIndexChanged, this, &MainPage::switchTab);

    // ── 底部播放条 ──
    connect(m_playerBar, &PlayerBar::playPauseRequested, this, [this]() {
        auto* ap = AudioPlayer::instance();
        if (!ap->playingId().isEmpty()) {
            ap->pause();
            m_playing = false;
        } else if (!ap->pausedId().isEmpty()) {
            ap->resume();
            m_playing = true;
        } else if (m_currentTrack >= 0) {
            playSong(m_currentTrack);
            return;
        }
        m_playerBar->setPlaying(m_playing);
    });
    connect(m_playerBar, &PlayerBar::prevRequested, this, [this]() {
        playSong(m_currentTrack - 1);
    });
    connect(m_playerBar, &PlayerBar::nextRequested, this, [this]() {
        playSong(m_currentTrack + 1);
    });
    connect(m_playerBar, &PlayerBar::queueRequested, this, [this]() {
        openQueue();
    });
    connect(m_playerBar, &PlayerBar::lyricRequested, this, [this]() {
        openLyric();
    });
    connect(m_playerBar, &PlayerBar::volumeClicked, this, [this]() {
        ToastPopup::show(m_playerBar, QString::fromUtf8("音量弹层：下轮实装"));
    });
}

void MainPage::showOptionsMenu(const QPointF& origin)
{
    // 旧菜单/遮罩先清（重复点击不叠加），anystik stickerhomepage 同款
    for (auto* old : findChildren<QskMenu*>())
        old->deleteLater();
    for (auto* old : findChildren<MenuOverlay*>())
        old->deleteLater();

    auto* menu = new QskMenu(this);
    menu->setModal(true);
    menu->setPopupFlag(QskPopup::DeleteOnClose, false);

    const int idxLog = menu->addOption(QskLabelData(tr("Logs")));
    const int idxSettings = menu->addOption(QskLabelData(tr("Settings")));
    const int idxAbout = menu->addOption(QskLabelData(tr("About")));
    menu->setOrigin(origin);

    connect(menu, &QskMenu::triggered, this,
        [this, idxLog, idxSettings, idxAbout](int index) {
            if (auto* pm = pageManager()) {
                if (index == idxLog) pm->open("logs");
                else if (index == idxSettings) pm->open("settings");
                else if (index == idxAbout) pm->open("about");
            }
            if (auto* m = qobject_cast<QskMenu*>(sender()))
                m->close();
        });

    {
        auto* overlay = new MenuOverlay(menu);
        connect(menu, &QObject::destroyed, overlay, &QObject::deleteLater);
        connect(menu, &QskPopup::closed, overlay, &QObject::deleteLater);
    }
    menu->open();
}

void MainPage::buildStubList(QskLinearBox* rows)
{
    const auto children = rows->childItems();
    for (auto* c : children) c->deleteLater();

    for (int i = 0; i < m_titles.size(); ++i) {
        auto* row = new QskLinearBox(Qt::Horizontal, rows);
        row->setPreferredHeight(44);

        auto* num = new QskTextLabel(QString::number(i + 1), row);
        num->setPreferredWidth(36);
        num->setAlignment(Qt::AlignVCenter | Qt::AlignRight);

        auto* name = new QskTextLabel(m_titles.at(i), row);
        name->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Preferred);
        name->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);

        auto* playBtn = new QskPushButton(QString::fromUtf8("▶"), row);
        playBtn->setPreferredSize(32, 32);
        connect(playBtn, &QskAbstractButton::clicked, this, [this, playBtn]() {
            ToastPopup::show(playBtn,
                QString::fromUtf8("桩数据分类，请到「歌曲」tab 搜索播放"));
        });
    }
    rows->update();
}

void MainPage::playSong(int index)
{
    if (index < 0 || index >= m_songs.size()) return;
    const auto& s = m_songs.at(index);
    if (!s.playable) {
        ToastPopup::show(this, QString::fromUtf8("灰色歌曲，无可播资源"));
        return;
    }
    if (m_resolving) return;
    m_currentTrack = index;
    m_resolving = true;
    m_playerBar->setDurationMs(s.durationMs);
    m_playerBar->setProgressMs(0);
    LogModel::instance().append(LogModel::Info, "player",
        QString("play: %1 - %2").arg(s.name, s.artists));
    NcmApi::instance().resolveAsync(s.id, NCM_Q_EXHIGH);
}

qint64 MainPage::currentSongId() const
{
    return (m_currentTrack >= 0 && m_currentTrack < m_songs.size())
        ? m_songs.at(m_currentTrack).id : 0;
}

QString MainPage::currentTitle() const
{
    if (m_currentTrack < 0 || m_currentTrack >= m_songs.size())
        return QStringLiteral("qskmusic");
    const auto& s = m_songs.at(m_currentTrack);
    return s.artists.isEmpty() ? s.name
                               : QString("%1 - %2").arg(s.name, s.artists);
}

void MainPage::openLyric()
{
    if (m_currentTrack < 0) {
        ToastPopup::show(this, QString::fromUtf8("先播放一首歌"));
        return;
    }
    if (!m_lyricPopup) {
        m_lyricPopup = new LyricPopup(this);
    }
    const qint64 id = currentSongId();
    if (m_lrcSongId == id && !m_lrcText.isEmpty()) {
        m_lyricPopup->setLyric(currentTitle(), m_lrcText);
    } else {
        m_lyricPopup->setLyric(currentTitle(), QString::fromUtf8("歌词加载中…"));
        NcmApi::instance().lyricAsync(id);
    }
    if (!m_lyricPopup->isOpen())
        m_lyricPopup->open();
    QTimer::singleShot(0, m_lyricPopup, [this]() {
        // open 后再走一次几何布局（父窗口尺寸已就绪，queuepopup 同款）
    });
}

void MainPage::switchTab(int index)
{
    if (index < 0 || index >= m_tabViews.size()) return;
    if (m_contentStack->currentItem() != m_tabViews.at(index))
        m_contentStack->setCurrentItem(m_tabViews.at(index));
    if (m_navBottom) m_navBottom->setCurrentIndex(index);
}

void MainPage::openQueue()
{
    if (!m_queuePopup) {
        m_queuePopup = new QueuePopup(this);
        connect(m_queuePopup, &QueuePopup::trackSelected, this, [this](int idx) {
            playSong(idx);
        });
    }
    QStringList names;
    for (const auto& s : m_songs)
        names << (s.artists.isEmpty() ? s.name
                                      : QString("%1 - %2").arg(s.name, s.artists));
    m_queuePopup->setTracks(names);
    m_queuePopup->setCurrentIndex(m_currentTrack);
    if (!m_queuePopup->isOpen())
        m_queuePopup->open();
    QTimer::singleShot(0, m_queuePopup, [this]() {
        // open 后再走一次几何布局（父窗口尺寸已就绪）
    });
}
