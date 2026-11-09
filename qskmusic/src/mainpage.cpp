#include "mainpage.h"
#include "pagemanager.h"
#include "playerbar.h"
#include "navbar.h"
#include "lyricpopup.h"
#include "categoryview.h"
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
    title->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Preferred);
    title->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);

    auto* logsBtn = new QskPushButton(tr("Logs"), topBar);
    auto* settingsBtn = new QskPushButton(tr("Settings"), topBar);
    auto* aboutBtn = new QskPushButton(tr("About"), topBar);

    connect(logsBtn, &QskAbstractButton::clicked, this, [this]() {
        if (auto* pm = pageManager()) pm->open("logs");
    });
    connect(settingsBtn, &QskAbstractButton::clicked, this, [this]() {
        if (auto* pm = pageManager()) pm->open("settings");
    });
    connect(aboutBtn, &QskAbstractButton::clicked, this, [this]() {
        if (auto* pm = pageManager()) pm->open("about");
    });

    new QskSeparator(Qt::Horizontal, layout);

    // ── 主体：宽屏 nav rail + 内容列；窄屏底栏挂在 layout 末尾 ──
    auto* body = new QskLinearBox(Qt::Horizontal, layout);
    body->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Expanding);

    m_navRail = new NavBar(body);
    connect(m_navRail, &NavBar::currentIndexChanged, this, &MainPage::switchTab);
    connect(m_navRail, &NavBar::modeChanged, this, [this](bool rail) {
        m_navRail->setVisible(rail);
        m_navBottom->setVisible(!rail);
    });

    auto* contentCol = new QskLinearBox(Qt::Vertical, body);
    contentCol->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Expanding);

    // 5 个 tab 子视图（QskStackBox 切换，状态互不干扰）
    m_contentStack = new QskStackBox(contentCol);
    m_contentStack->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Expanding);
    for (int i = 0; i < 5; ++i) {
        if (i == 0) {
            // 「歌曲」tab：搜索框 + 真实搜索结果
            auto* col = new QskLinearBox(Qt::Vertical, m_contentStack);
            col->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Expanding);

            auto* searchBar = new QskLinearBox(Qt::Horizontal, col);
            searchBar->setMargins(8, 8, 8, 0);
            searchBar->setSpacing(8);
            m_searchField = new QskTextField(searchBar);
            m_searchField->setPlaceholderText(QStringLiteral("搜索歌曲、歌手…"));
            m_searchField->setFixedHeight(30);   // QSkinny 输入框后半高字坑：必须锁死高
            m_searchField->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Preferred);
            auto* searchBtn = new QskPushButton(QStringLiteral("搜索"), searchBar);
            connect(searchBtn, &QskAbstractButton::clicked, this, [this]() {
                const QString kw = m_searchField->text().trimmed();
                if (!kw.isEmpty())
                    NcmApi::instance().searchAsync(kw, 30);
            });

            auto* rows = new QskLinearBox(Qt::Vertical);
            rows->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Minimum);
            auto* scroll = new QskScrollArea(col);
            scroll->setScrolledItem(rows);
            scroll->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Expanding);
            m_searchRows = rows;
            m_tabViews.append(col);
        } else if (i == 1) {
            // 「分类」tab：qlmusic 分类树 + 歌单列表（push 页面栈）
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
    connect(&ncm, &NcmApi::searchDone, this,
        [this](const QList<Song>& songs, const QString& err) {
            if (!err.isEmpty()) {
                ToastPopup::show(this, QString::fromUtf8("搜索失败：%1").arg(err));
                return;
            }
            m_songs = songs;
            buildSearchList();
        });
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

    // 默认热词自动搜一次，启动即有内容
    NcmApi::instance().searchAsync(QStringLiteral("周杰伦"), 30);

    // ── 底部导航（窄屏形态；宽屏由 modeChanged 隐藏）──
    m_navBottom = new NavBar(layout);
    connect(m_navBottom, &NavBar::currentIndexChanged, this, &MainPage::switchTab);
    m_navRail->setVisible(false);   // 初始窄屏形态，挂 window 后按实际宽度校正

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

void MainPage::buildSearchList()
{
    const auto children = m_searchRows->childItems();
    for (auto* c : children) c->deleteLater();

    for (int i = 0; i < m_songs.size(); ++i) {
        const auto& s = m_songs.at(i);
        auto* row = new QskLinearBox(Qt::Horizontal, m_searchRows);
        row->setPreferredHeight(44);

        auto* num = new QskTextLabel(QString::number(i + 1), row);
        num->setPreferredWidth(36);
        num->setAlignment(Qt::AlignVCenter | Qt::AlignRight);

        auto* name = new QskTextLabel(
            s.artists.isEmpty() ? s.name
                                : QString("%1 - %2").arg(s.name, s.artists), row);
        name->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Preferred);
        name->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);

        const int total = s.durationMs / 1000;
        auto* dur = new QskTextLabel(
            QString("%1:%2").arg(total / 60).arg(total % 60, 2, 10, QLatin1Char('0')), row);
        dur->setPreferredWidth(48);
        dur->setAlignment(Qt::AlignVCenter | Qt::AlignRight);

        auto* playBtn = new QskPushButton(QString::fromUtf8("▶"), row);
        playBtn->setPreferredSize(32, 32);
        connect(playBtn, &QskAbstractButton::clicked, this, [this, i]() {
            playSong(i);
        });
    }
    m_searchRows->update();
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
    // 同步两个导航条选中态（blockSignals 防信号回环）
    for (auto* nav : { m_navRail, m_navBottom }) {
        const QSignalBlocker blocker(nav);
        nav->setCurrentIndex(index);
    }
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
