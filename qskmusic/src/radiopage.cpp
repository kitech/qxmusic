#include "radiopage.h"
#include "ncmcatalog.h"
#include "playback.h"
#include "pagemanager.h"
#include "menuoverlay.h"
#include "statusline.h"
#include "toastpopup.h"
#include "downloadservice.h"
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
#include <QUrl>
#include <private/qquicktaphandler_p.h>
#include <QDebug>

namespace {

QString elided(const QString& t, int n)
{
    return t.size() > n ? t.left(n - 1) + QString::fromUtf8("…") : t;
}

} // namespace

RadioPage::RadioPage(QQuickItem* parent)
    : Page(parent)
{
}

void RadioPage::onCreate(const QVariantMap& launchArgs, const QVariantMap&)
{
    m_rid = launchArgs.value("rid").toLongLong();
    m_title = launchArgs.value("title").toString();

    setAutoLayoutChildren(true);
    auto* layout = new QskLinearBox(Qt::Vertical, this);
    layout->setPanel(true);

    // ── 顶栏：返回 + 电台名 ──
    auto* topBar = new QskLinearBox(Qt::Horizontal, layout);
    topBar->setPanel(true);
    topBar->setPreferredHeight(56);

    auto* backBtn = new QskPushButton(QString::fromUtf8("←"), topBar);
    connect(backBtn, &QskAbstractButton::clicked, this, [this]() {
        if (auto* pm = pageManager()) pm->back();
    });

    auto* header = new QskTextLabel(elided(m_title, 12), topBar);
    header->setFontRole(QskFontRole(QskFontRole::Title, QskFontRole::Normal));
    header->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Preferred);
    header->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);

    new QskSeparator(Qt::Horizontal, layout);

    // 信息行：共 N 期 + 播放全部
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
            ToastPopup::show(this, tr("No programs"));
            return;
        }
        ToastPopup::show(this, tr("Resolving play URL…"));
        Playback::instance().setQueue(m_songs, 0);
    });

    m_status = new StatusLine(layout);
    m_status->setStatus(StatusLine::Loading, tr("Loading radio programs…"));

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
                if (!m_songs.at(idx).playable) {
                    ToastPopup::show(this, tr("No audio for this program"));
                    return;
                }
                Playback::instance().setQueue(m_songs, idx);
            });
        // 长按 → 下载（节目 mainSong 即普通歌曲，可下载）
        connect(tap, &QQuickTapHandler::longPressed, this, [this, tap]() {
            const int idx = songRowAt(tap->point().position());
            if (idx < 0 || idx >= m_songs.size()) return;
            if (!m_songs.at(idx).playable) {
                ToastPopup::show(this, tr("No audio for this program"));
                return;
            }

            for (auto* old : findChildren<QskMenu*>())
                old->deleteLater();
            for (auto* old : findChildren<MenuOverlay*>())
                old->deleteLater();

            auto* menu = new QskMenu(this);
            menu->setModal(true);
            menu->setPopupFlag(QskPopup::DeleteOnClose, false);
            const int idxDl = menu->addOption(
                QskLabelData(tr("Download: %1")
                    .arg(elided(m_songs.at(idx).name, 10))));
            menu->setOrigin(tap->point().position());
            connect(menu, &QskMenu::triggered, this, [this, idx, idxDl](int i) {
                if (i == idxDl)
                    requestDownload(idx);
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

    connect(&NcmCatalog::instance(), &NcmCatalog::djProgramsDone, this,
        [this](qint64 rid, const QList<DjProgram>& programs, const QString& err) {
            if (rid != m_rid) return;
            if (!err.isEmpty()) {
                m_status->setStatus(StatusLine::Error,
                    tr("Load failed: %1").arg(err));
                return;
            }
            m_songs.clear();
            for (const auto& p : programs) {
                Song s;
                s.id = p.mainSongId;          // 播放/下载的唯一依据
                s.name = p.name;
                s.artists = m_title;
                s.cover = p.cover;
                s.durationMs = p.durationMs;
                s.playable = p.mainSongId > 0;
                m_songs.append(s);
            }
            if (m_songs.isEmpty()) {
                m_status->setStatus(StatusLine::Info, tr("No programs"));
                m_count->setText(QString());
            } else {
                m_status->clearStatus();
                m_count->setText(tr("%1 programs").arg(m_songs.size()));
            }
            buildRows();
        });

    NcmCatalog::instance().loadDjProgramsAsync(m_rid, 50, 0);

    // 手动下载：先取 URL（不触发播放），再入下载队列
    connect(&NcmApi::instance(), &NcmApi::downloadUrlDone, this,
        [this](qint64 id, const QString& url, const QString& err) {
            if (id != m_pendingDlId) return;
            m_pendingDlId = 0;
            if (!err.isEmpty() || url.isEmpty()) {
                ToastPopup::show(this, tr("Failed to get download URL: %1").arg(err));
                return;
            }
            QString suffix = QUrl(url).path().section(QLatin1Char('.'), -1);
            if (suffix.isEmpty() || suffix.size() > 5)
                suffix = QStringLiteral("mp3");
            DownloadService::instance().enqueue(id, url, suffix);
            ToastPopup::show(this, tr("Added to downloads"));
        });

    connect(&Playback::instance(), &Playback::songChanged, this,
        [this]() { updatePlayingMarker(); });
}

void RadioPage::buildRows()
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
            QString::number(s.id), IconItem::pxForDp(36)));
        icon->setSource(s.cover, 36);

        auto* marker = new QskTextLabel(QString(), row);
        marker->setPreferredWidth(14);
        marker->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
        marker->setTextColor(QColor(0x3d, 0x8b, 0xfd));
        m_playingMarkers.append({ s.id, marker });

        auto* num = new QskTextLabel(QString::number(i + 1), row);
        num->setPreferredWidth(36);
        num->setAlignment(Qt::AlignVCenter | Qt::AlignRight);

        auto* name = new QskTextLabel(elided(s.name, 20), row);
        name->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Preferred);
        name->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);

        const int total = s.durationMs / 1000;
        QString durText = QString("%1:%2").arg(total / 60)
            .arg(total % 60, 2, 10, QLatin1Char('0'));
        if (!s.playable)
            durText += QString::fromUtf8(" ✕");
        auto* dur = new QskTextLabel(durText, row);
        dur->setPreferredWidth(56);
        dur->setAlignment(Qt::AlignVCenter | Qt::AlignRight);

        auto* playBtn = new QskPushButton(QString::fromUtf8("▶"), row);
        playBtn->setPreferredSize(30, 30);
        connect(playBtn, &QskAbstractButton::clicked, this, [this, i]() {
            if (i < 0 || i >= m_songs.size()) return;
            if (!m_songs.at(i).playable) {
                ToastPopup::show(this, tr("No audio for this program"));
                return;
            }
            Playback::instance().setQueue(m_songs, i);
        });
    }
    updatePlayingMarker();
    m_rows->update();
}

void RadioPage::updatePlayingMarker()
{
    const qint64 playing = Playback::instance().currentSongId();
    for (auto& p : m_playingMarkers) {
        if (p.second)
            p.second->setText(p.first != 0 && p.first == playing
                ? QString::fromUtf8("▶") : QString());
    }
}

int RadioPage::songRowAt(const QPointF& localPos) const
{
    const auto rows = m_rows->childItems();
    for (int i = 0; i < rows.size(); ++i) {
        if (localPos.y() >= rows[i]->y()
            && localPos.y() < rows[i]->y() + rows[i]->height())
            return i;
    }
    return -1;
}

void RadioPage::requestDownload(int index)
{
    if (index < 0 || index >= m_songs.size()) return;
    const qint64 id = m_songs.at(index).id;
    if (id <= 0) {
        ToastPopup::show(this, tr("No audio for this program"));
        return;
    }
    if (DownloadService::instance().isCached(id, QStringLiteral("mp3"))) {
        ToastPopup::show(this, tr("Already cached"));
        return;
    }
    m_pendingDlId = id;
    NcmApi::instance().resolveForDownloadAsync(id);
}
