#include "favoritespage.h"
#include "favorites.h"
#include "playback.h"
#include "pagemanager.h"
#include "menuoverlay.h"
#include "statusline.h"
#include "toastpopup.h"
#include "iconitem.h"
#include "icongen.h"

#include <QskLinearBox.h>
#include <QskTextLabel.h>
#include <QskPushButton.h>
#include <QskScrollArea.h>
#include <QskSeparator.h>
#include <QskFontRole.h>
#include <QskMenu.h>
#include <QskLabelData.h>
#include <QskPopup.h>
#include <private/qquicktaphandler_p.h>
#include <QDebug>

namespace {

QString elided(const QString& t, int n)
{
    return t.size() > n ? t.left(n - 1) + QString::fromUtf8("…") : t;
}

} // namespace

FavoritesPage::FavoritesPage(QQuickItem* parent)
    : Page(parent)
{
}

void FavoritesPage::onCreate(const QVariantMap&, const QVariantMap&)
{
    setAutoLayoutChildren(true);
    auto* layout = new QskLinearBox(Qt::Vertical, this);
    layout->setPanel(true);

    // ── 顶栏：返回 + 标题 ──
    auto* topBar = new QskLinearBox(Qt::Horizontal, layout);
    topBar->setPanel(true);
    topBar->setPreferredHeight(56);

    auto* backBtn = new QskPushButton(QString::fromUtf8("←"), topBar);
    connect(backBtn, &QskAbstractButton::clicked, this, [this]() {
        if (auto* pm = pageManager()) pm->back();
    });

    auto* title = new QskTextLabel(tr("My Favorites"), topBar);
    title->setFontRole(QskFontRole(QskFontRole::Title, QskFontRole::Normal));
    title->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Preferred);
    title->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);

    new QskSeparator(Qt::Horizontal, layout);

    // 信息行：共 N 首 + 播放全部
    auto* infoRow = new QskLinearBox(Qt::Horizontal, layout);
    infoRow->setSpacing(8);
    infoRow->setPreferredHeight(38);

    m_count = new QskTextLabel(QString(), infoRow);
    m_count->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Preferred);
    m_count->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);

    auto* playAllBtn = new QskPushButton(tr("▶ Play All"), infoRow);
    playAllBtn->setPreferredHeight(34);
    connect(playAllBtn, &QskAbstractButton::clicked, this, [this]() {
        if (m_songs.isEmpty()) {
            ToastPopup::show(this, tr("No favorites yet"));
            return;
        }
        ToastPopup::show(this, tr("Resolving play URL…"));
        Playback::instance().setQueue(m_songs, 0);
    });

    m_status = new StatusLine(layout);

    m_rows = new QskLinearBox(Qt::Vertical);
    m_rows->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Minimum);
    auto* scroll = new QskScrollArea(layout);
    scroll->setScrolledItem(m_rows);
    scroll->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Expanding);

    // 安卓触摸坑：行点击走 QQuickTapHandler
    {
        auto* tap = new QQuickTapHandler(m_rows);
        tap->setGesturePolicy(QQuickTapHandler::DragThreshold);
        connect(tap, &QQuickTapHandler::singleTapped, this,
            [this](QEventPoint point, Qt::MouseButton) {
                const int idx = songRowAt(point.position());
                if (idx < 0 || idx >= m_songs.size()) return;
                Playback::instance().setQueue(m_songs, idx);
            });
        // 长按 → 取消收藏
        connect(tap, &QQuickTapHandler::longPressed, this, [this, tap]() {
            const int idx = songRowAt(tap->point().position());
            if (idx < 0 || idx >= m_songs.size()) return;

            for (auto* old : findChildren<QskMenu*>())
                old->deleteLater();
            for (auto* old : findChildren<MenuOverlay*>())
                old->deleteLater();

            auto* menu = new QskMenu(this);
            menu->setModal(true);
            menu->setPopupFlag(QskPopup::DeleteOnClose, false);
            const int idxUnfav = menu->addOption(
                QskLabelData(tr("Remove favorite: %1")
                    .arg(elided(m_songs.at(idx).name, 10))));
            menu->setOrigin(tap->point().position());
            const Song song = m_songs.at(idx);
            connect(menu, &QskMenu::triggered, this, [this, song, idxUnfav](int i) {
                if (i == idxUnfav) {
                    Favorites::instance().toggle(song);
                    ToastPopup::show(this, tr("Removed from favorites"));
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
        });
    }

    connect(&Favorites::instance(), &Favorites::changed, this,
        [this]() { reload(); });
    connect(&Playback::instance(), &Playback::songChanged, this,
        [this]() { updatePlayingMarker(); });

    reload();
}

void FavoritesPage::reload()
{
    m_songs = Favorites::instance().songs();
    if (m_songs.isEmpty()) {
        m_status->setStatus(StatusLine::Info,
            tr("No favorites yet: long-press a song to favorite it"));
        m_count->setText(QString());
    } else {
        m_status->clearStatus();
        m_count->setText(tr("%1 tracks").arg(m_songs.size()));
    }
    buildRows();
}

void FavoritesPage::buildRows()
{
    const auto children = m_rows->childItems();
    for (auto* c : children) c->deleteLater();
    m_playingMarkers.clear();

    for (int i = 0; i < m_songs.size(); ++i) {
        const auto& s = m_songs.at(i);
        auto* row = new QskLinearBox(Qt::Horizontal, m_rows);
        row->setPreferredHeight(44);

        auto* icon = new IconItem(row);
        icon->setPreferredSize(36, 36);
        icon->setCornerRadius(6);
        icon->setFallback(qximage::identicon(
            s.albumId > 0 ? QString::number(s.albumId) : QString::number(s.id),
            IconItem::pxForDp(36)));
        icon->setSource(s.cover, 36);

        auto* marker = new QskTextLabel(QString(), row);
        marker->setPreferredWidth(14);
        marker->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
        marker->setTextColor(QColor(0x3d, 0x8b, 0xfd));
        m_playingMarkers.append({ s.id, marker });

        auto* num = new QskTextLabel(QString::number(i + 1), row);
        num->setPreferredWidth(36);
        num->setAlignment(Qt::AlignVCenter | Qt::AlignRight);

        QString label = s.artists.isEmpty()
            ? elided(s.name, 14)
            : elided(QString("%1 - %2").arg(s.name, s.artists), 20);
        if (Favorites::instance().isFromAccount(s.id))
            label = QString::fromUtf8("☁ ") + label;   // 账号同步项标记（图标，不上屏文案）
        auto* name = new QskTextLabel(label, row);
        name->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Preferred);
        name->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);

        const int total = s.durationMs / 1000;
        QString durText = QString("%1:%2").arg(total / 60)
            .arg(total % 60, 2, 10, QLatin1Char('0'));
        auto* dur = new QskTextLabel(durText, row);
        dur->setPreferredWidth(56);
        dur->setAlignment(Qt::AlignVCenter | Qt::AlignRight);

        auto* playBtn = new QskPushButton(QString::fromUtf8("▶"), row);
        playBtn->setPreferredSize(30, 30);
        connect(playBtn, &QskAbstractButton::clicked, this, [this, i]() {
            Playback::instance().setQueue(m_songs, i);
        });
    }
    updatePlayingMarker();
    m_rows->update();
}

void FavoritesPage::updatePlayingMarker()
{
    const qint64 playing = Playback::instance().currentSongId();
    for (auto& p : m_playingMarkers) {
        if (p.second)
            p.second->setText(p.first != 0 && p.first == playing
                ? QString::fromUtf8("▶") : QString());
    }
}

int FavoritesPage::songRowAt(const QPointF& localPos) const
{
    const auto rows = m_rows->childItems();
    for (int i = 0; i < rows.size(); ++i) {
        if (localPos.y() >= rows[i]->y()
            && localPos.y() < rows[i]->y() + rows[i]->height())
            return i;
    }
    return -1;
}
