#include "mainpage.h"
#include "pagemanager.h"
#include "playerbar.h"
#include "navbar.h"
#include "queuepopup.h"
#include "toastpopup.h"
#include "logmodel.h"
#include <QskLinearBox.h>
#include <QskTextLabel.h>
#include <QskPushButton.h>
#include <QskScrollArea.h>
#include <QskStackBox.h>
#include <QskSeparator.h>
#include <QskFontRole.h>
#include <QQuickWindow>

MainPage::MainPage(QQuickItem* parent)
    : Page(parent)
{
    // 桩数据：20 首示例歌曲
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

    // 4 个歌单 tab 子视图（QskStackBox 切换，状态互不干扰）
    m_contentStack = new QskStackBox(contentCol);
    m_contentStack->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Expanding);
    for (int i = 0; i < 4; ++i) {
        auto* rows = new QskLinearBox(Qt::Vertical);
        rows->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Minimum);
        auto* scroll = new QskScrollArea(m_contentStack);
        scroll->setScrolledItem(rows);
        scroll->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Expanding);
        buildSongList(rows);
        m_tabViews.append(scroll);
    }
    m_contentStack->setCurrentItem(m_tabViews.first());

    m_playerBar = new PlayerBar(contentCol);

    // ── 底部导航（窄屏形态；宽屏由 modeChanged 隐藏）──
    m_navBottom = new NavBar(layout);
    connect(m_navBottom, &NavBar::currentIndexChanged, this, &MainPage::switchTab);
    m_navRail->setVisible(false);   // 初始窄屏形态，挂 window 后按实际宽度校正

    // ── 底部播放条 ──
    connect(m_playerBar, &PlayerBar::playPauseRequested, this, [this]() {
        m_playing = !m_playing;
        m_playerBar->setPlaying(m_playing);
        if (m_currentTrack >= 0) {
            LogModel::instance().append(LogModel::Info, "player",
                m_playing ? QString("play: %1").arg(m_titles.at(m_currentTrack))
                          : QStringLiteral("pause"));
        }
    });
    connect(m_playerBar, &PlayerBar::prevRequested, this, [this]() {
        if (m_currentTrack > 0) {
            m_currentTrack--;
            m_playerBar->setDurationMs(213000);
            m_playerBar->setProgressMs(0);
        }
    });
    connect(m_playerBar, &PlayerBar::nextRequested, this, [this]() {
        if (m_currentTrack < m_titles.size() - 1) {
            m_currentTrack++;
            m_playerBar->setDurationMs(213000);
            m_playerBar->setProgressMs(0);
        }
    });
    connect(m_playerBar, &PlayerBar::queueRequested, this, [this]() {
        openQueue();
    });
    connect(m_playerBar, &PlayerBar::volumeClicked, this, [this]() {
        ToastPopup::show(m_playerBar, QString::fromUtf8("音量弹层：下轮实装"));
    });
}

void MainPage::buildSongList(QskLinearBox* rows)
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
        connect(playBtn, &QskAbstractButton::clicked, this, [this, i]() {
            m_currentTrack = i;
            m_playing = true;
            m_playerBar->setPlaying(true);
            m_playerBar->setDurationMs(213000);
            m_playerBar->setProgressMs(0);
            LogModel::instance().append(LogModel::Info, "player",
                QString("play: %1").arg(m_titles.at(i)));
        });
    }
    rows->update();
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
            m_currentTrack = idx;
            m_playing = true;
            m_playerBar->setPlaying(true);
            m_playerBar->setDurationMs(213000);
            m_playerBar->setProgressMs(0);
        });
    }
    m_queuePopup->setTracks(m_titles);
    m_queuePopup->setCurrentIndex(m_currentTrack);
    if (!m_queuePopup->isOpen())
        m_queuePopup->open();
    QTimer::singleShot(0, m_queuePopup, [this]() {
        // open 后再走一次几何布局（父窗口尺寸已就绪）
    });
}
