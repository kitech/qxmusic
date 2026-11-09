#include "playlisttrackspage.h"
#include "pagemanager.h"
#include "playback.h"
#include "ncmcatalog.h"
#include <QskLinearBox.h>
#include <QskTextLabel.h>
#include <QskPushButton.h>
#include <QskScrollArea.h>
#include <QskSeparator.h>
#include <QskFontRole.h>

namespace {

QString elided(const QString& text, int maxChars)
{
    return text.size() > maxChars
        ? text.left(maxChars - 1) + QString::fromUtf8("…") : text;
}

} // namespace

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
    m_header = new QskTextLabel(elided(m_title, 12), topBar);
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
                m_header->setText(elided(m_title, 12));
            }
            m_status->setText(m_songs.isEmpty()
                ? QString::fromUtf8("歌单暂无曲目")
                : QString::fromUtf8("%1 首，点 ▶ 播放").arg(m_songs.size()));
            buildRows();
        });

    NcmCatalog::instance().loadPlaylistTracksAsync(m_playlistId);
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
            s.artists.isEmpty()
                ? elided(s.name, 14)
                : elided(QString("%1 - %2").arg(s.name, s.artists), 20), row);
        name->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Preferred);
        name->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);

        const int total = s.durationMs / 1000;
        auto* dur = new QskTextLabel(
            QString("%1:%2").arg(total / 60).arg(total % 60, 2, 10, QLatin1Char('0')), row);
        dur->setPreferredWidth(44);
        dur->setAlignment(Qt::AlignVCenter | Qt::AlignRight);

        auto* playBtn = new QskPushButton(QString::fromUtf8("▶"), row);
        playBtn->setPreferredSize(32, 32);
        connect(playBtn, &QskAbstractButton::clicked, this, [this, i]() {
            Playback::instance().setQueue(m_songs, i);
        });
    }
    m_rows->update();
}
