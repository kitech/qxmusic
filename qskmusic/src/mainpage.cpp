#include "mainpage.h"
#include "pagemanager.h"
#include "playerbar.h"
#include "queuepopup.h"
#include "toastpopup.h"
#include "logmodel.h"
#include <QskLinearBox.h>
#include <QskTextLabel.h>
#include <QskPushButton.h>
#include <QskScrollArea.h>
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

    // ── 主体：左导航 + 右歌曲列表 ──
    auto* body = new QskLinearBox(Qt::Horizontal, layout);
    body->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Expanding);

    // 左侧导航桩
    auto* nav = new QskLinearBox(Qt::Vertical, body);
    nav->setPanel(true);
    nav->setPreferredWidth(160);

    auto* navTitle = new QskTextLabel(QString::fromUtf8("音乐库"), nav);
    navTitle->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    navTitle->setPreferredHeight(40);

    const QStringList navItems = {
        QString::fromUtf8("全部歌曲"),
        QString::fromUtf8("我喜欢的音乐"),
        QString::fromUtf8("最近播放"),
        QString::fromUtf8("下载音乐"),
    };
    for (const auto& it : navItems) {
        auto* btn = new QskPushButton(it, nav);
        btn->setPreferredHeight(40);
        connect(btn, &QskAbstractButton::clicked, this, [btn]() {
            ToastPopup::show(btn, QString::fromUtf8("桩导航：%1").arg(btn->text()));
        });
    }
    nav->addStretch(1);

    // 右侧歌曲列表（QskScrollArea + 内容盒 Expanding/Minimum 范式）
    m_songRows = new QskLinearBox(Qt::Vertical);
    m_songRows->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Minimum);

    auto* scroll = new QskScrollArea(body);
    scroll->setScrolledItem(m_songRows);
    scroll->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Expanding);

    buildSongList();

    // ── 底部播放条 ──
    m_playerBar = new PlayerBar(layout);
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

void MainPage::buildSongList()
{
    const auto children = m_songRows->childItems();
    for (auto* c : children) c->deleteLater();

    for (int i = 0; i < m_titles.size(); ++i) {
        auto* row = new QskLinearBox(Qt::Horizontal, m_songRows);
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
    m_songRows->update();
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
