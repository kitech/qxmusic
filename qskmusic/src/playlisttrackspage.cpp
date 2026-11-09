#include "playlisttrackspage.h"
#include "pagemanager.h"
#include "playerbar.h"
#include "ncmcatalog.h"
#include "toastpopup.h"
#include "logmodel.h"
#include "audioplayer.h"
#include <QskLinearBox.h>
#include <QskTextLabel.h>
#include <QskPushButton.h>
#include <QskScrollArea.h>
#include <QskSeparator.h>
#include <QskFontRole.h>
#include <QUrl>

PlaylistTracksPage::PlaylistTracksPage(QQuickItem* parent)
    : Page(parent)
{
}

void PlaylistTracksPage::onCreate(const QVariantMap& launchArgs,
    const QVariantMap&)
{
    m_playlistId = launchArgs.value("playlistId").toLongLong();
    m_title = launchArgs.value("title").toString();

    setAutoLayoutChildren(true);
    auto* layout = new QskLinearBox(Qt::Vertical, this);
    layout->setPanel(true);

    // ── 顶栏：返回 + 歌单名 ──
    auto* topBar = new QskLinearBox(Qt::Horizontal, layout);
    topBar->setPanel(true);
    topBar->setPreferredHeight(56);

    auto* backBtn = new QskPushButton(QString::fromUtf8("←"), topBar);
    connect(backBtn, &QskAbstractButton::clicked, this, [this]() {
        if (auto* pm = pageManager()) pm->back();
    });
    m_header = new QskTextLabel(m_title, topBar);
    m_header->setFontRole(QskFontRole(QskFontRole::Title, QskFontRole::Normal));
    m_header->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Preferred);
    m_header->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);

    new QskSeparator(Qt::Horizontal, layout);

    m_status = new QskTextLabel(QString::fromUtf8("加载中…"), layout);
    m_status->setPreferredHeight(32);
    m_status->setAlignment(Qt::AlignCenter);

    m_rows = new QskLinearBox(Qt::Vertical);
    m_rows->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Minimum);
    auto* scroll = new QskScrollArea(layout);
    scroll->setScrolledItem(m_rows);
    scroll->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Expanding);

    m_playerBar = new PlayerBar(layout);

    // ── 数据 / 播放接线 ──
    connect(&NcmCatalog::instance(), &NcmCatalog::playlistTracksDone, this,
        [this](const PlaylistInfo& info, const QString& err) {
            if (info.id != m_playlistId) return;
            if (!err.isEmpty()) {
                m_status->setText(QString::fromUtf8("加载失败：%1").arg(err));
                return;
            }
            m_songs = info.songs;
            if (!info.name.isEmpty()) {
                m_title = info.name;
                m_header->setText(m_title);
            }
            m_status->setText(m_songs.isEmpty()
                ? QString::fromUtf8("歌单暂无曲目")
                : QString::fromUtf8("%1 首").arg(m_songs.size()));
            buildRows();
        });

    connect(&NcmApi::instance(), &NcmApi::resolveDone, this,
        [this](qint64 songId, const QString& url, const QString& err) {
            m_resolving = false;
            if (songId != currentSongId()) return;   // 非本页请求
            if (!err.isEmpty() || url.isEmpty()) {
                ToastPopup::show(this, QString::fromUtf8("获取播放链接失败：%1").arg(err));
                return;
            }
            AudioPlayer::instance()->playUrl(QString::number(songId), QUrl(url));
            m_playerBar->setPlaying(true);
        });

    auto* ap = AudioPlayer::instance();
    connect(ap, &AudioPlayer::finished, this, [this](const QString& id) {
        m_playerBar->setPlaying(false);
        if (id.toLongLong() != currentSongId()) return;   // 不是本页队列
        playSong(m_current + 1);
    });
    connect(ap, &AudioPlayer::positionChanged, this,
        [this](qint64 ms) { m_playerBar->setProgressMs(int(ms)); });
    connect(ap, &AudioPlayer::durationChanged, this,
        [this](qint64 ms) { m_playerBar->setDurationMs(int(ms)); });

    connect(m_playerBar, &PlayerBar::playPauseRequested, this, [this]() {
        auto* ap = AudioPlayer::instance();
        if (!ap->playingId().isEmpty()) {
            ap->pause();
            m_playerBar->setPlaying(false);
        } else if (!ap->pausedId().isEmpty()) {
            ap->resume();
            m_playerBar->setPlaying(true);
        } else if (m_current >= 0) {
            playSong(m_current);
        }
    });
    connect(m_playerBar, &PlayerBar::prevRequested, this, [this]() {
        playSong(m_current - 1);
    });
    connect(m_playerBar, &PlayerBar::nextRequested, this, [this]() {
        playSong(m_current + 1);
    });
    connect(m_playerBar, &PlayerBar::volumeClicked, this, [this]() {
        ToastPopup::show(m_playerBar, QString::fromUtf8("音量弹层：下轮实装"));
    });

    NcmCatalog::instance().loadPlaylistTracksAsync(m_playlistId);
}

void PlaylistTracksPage::onResume()
{
    // 从子场景回来时按全局播放器状态校正播放条
    auto* ap = AudioPlayer::instance();
    m_playerBar->setPlaying(!ap->playingId().isEmpty());
}

void PlaylistTracksPage::buildRows()
{
    const auto children = m_rows->childItems();
    for (auto* c : children) c->deleteLater();

    for (int i = 0; i < m_songs.size(); ++i) {
        const auto& s = m_songs.at(i);
        auto* row = new QskLinearBox(Qt::Horizontal, m_rows);
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
    m_rows->update();
}

void PlaylistTracksPage::playSong(int index)
{
    if (index < 0 || index >= m_songs.size()) return;
    const auto& s = m_songs.at(index);
    if (!s.playable) {
        ToastPopup::show(this, QString::fromUtf8("灰色歌曲，无可播资源"));
        return;
    }
    if (m_resolving) return;
    m_current = index;
    m_resolving = true;
    m_playerBar->setDurationMs(s.durationMs);
    m_playerBar->setProgressMs(0);
    LogModel::instance().append(LogModel::Info, "player",
        QString("play: %1 - %2").arg(s.name, s.artists));
    NcmApi::instance().resolveAsync(s.id, NCM_Q_EXHIGH);
}

qint64 PlaylistTracksPage::currentSongId() const
{
    return (m_current >= 0 && m_current < m_songs.size())
        ? m_songs.at(m_current).id : 0;
}
