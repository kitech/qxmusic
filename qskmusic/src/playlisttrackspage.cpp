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
#include <private/qquicktaphandler_p.h>

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

    // 安卓触摸坑（AGENTS.md 4.2）：曲目行点击走 QQuickTapHandler
    // （行 = 整行可点，点即以该行为起点投全歌单队列）
    {
        auto* tap = new QQuickTapHandler(m_rows);
        tap->setGesturePolicy(QQuickTapHandler::DragThreshold);
        connect(tap, &QQuickTapHandler::singleTapped, this,
            [this](QEventPoint point, Qt::MouseButton) {
                const int idx = songRowAt(point.position());
                if (idx < 0 || idx >= m_songs.size()) return;
                Playback::instance().setQueue(m_songs, idx);
            });
    }

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

    // 实锤灰歌 → 本地行摘对号并重建（qlmusic 实时兜底同款）
    connect(&Playback::instance(), &Playback::playableChanged, this,
        [this](qint64 songId) {
            for (auto& s : m_songs) {
                if (s.id == songId && s.playable) {
                    s.playable = false;
                    buildRows();
                    break;
                }
            }
        });
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
        QString durText = QString("%1:%2").arg(total / 60)
            .arg(total % 60, 2, 10, QLatin1Char('0'));
        if (s.id > 0 && s.playable)
            durText += QString::fromUtf8(" ✓");   // 非灰歌对号（本地歌 id=0 不标）
        auto* dur = new QskTextLabel(durText, row);
        dur->setPreferredWidth(56);
        dur->setAlignment(Qt::AlignVCenter | Qt::AlignRight);
    }
    m_rows->update();
}

int PlaylistTracksPage::songRowAt(const QPointF& localPos) const
{
    const auto rows = m_rows->childItems();
    for (int i = 0; i < rows.size(); ++i) {
        if (localPos.y() >= rows[i]->y()
            && localPos.y() < rows[i]->y() + rows[i]->height()) {
            return i;   // 曲目行全部可点
        }
    }
    return -1;
}
