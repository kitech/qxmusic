#include "playlisttrackspage.h"
#include "pagemanager.h"
#include "playback.h"
#include "favorites.h"
#include "ncmcatalog.h"
#include "ncmapi.h"
#include "downloadservice.h"
#include "statusline.h"
#include "menuoverlay.h"
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
#include <QUrl>
#include <QColor>
#include <QTimer>

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

    // 歌单封面（48dp 圆角，数据到了再显示）
    m_plCover = new IconItem(topBar);
    m_plCover->setPreferredSize(48, 48);
    m_plCover->setCornerRadius(8);
    m_plCover->setVisible(false);

    m_header = new QskTextLabel(elided(m_title, 12), topBar);
    m_header->setFontRole(QskFontRole(QskFontRole::Title, QskFontRole::Normal));
    m_header->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Preferred);
    m_header->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);

    new QskSeparator(Qt::Horizontal, layout);

    // 信息行：共 N 首 + 播放全部（滚动区外，QskPushButton 可靠通道；
    // qlmusic contentview playAll → trackActivated(0) 同义）
    auto* infoRow = new QskLinearBox(Qt::Horizontal, layout);
    infoRow->setSpacing(8);
    infoRow->setPreferredHeight(38);

    m_count = new QskTextLabel(QString(), infoRow);
    m_count->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Preferred);
    m_count->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);

    auto* playAllBtn = new QskPushButton(QString::fromUtf8("▶ 播放全部"), infoRow);
    playAllBtn->setPreferredHeight(34);
    connect(playAllBtn, &QskAbstractButton::clicked, this, [this]() {
        qDebug() << "[DIAG] playAll clicked songs=" << m_songs.size();
        if (m_songs.isEmpty()) {
            ToastPopup::show(this, QString::fromUtf8("歌单尚未加载"));
            return;
        }
        ToastPopup::show(this, QString::fromUtf8("正在获取播放链接…"));
        Playback::instance().setQueue(m_songs, 0);
    });

    m_status = new StatusLine(layout);
    m_status->setStatus(StatusLine::Loading, QString::fromUtf8("正在加载歌单…"));
    connect(m_status, &StatusLine::retryClicked, this, [this]() {
        m_status->setStatus(StatusLine::Loading, QString::fromUtf8("正在加载歌单…"));
        NcmCatalog::instance().loadPlaylistTracksAsync(m_playlistId);
    });

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
        // 长按 → 下载菜单（手动下载通道；qlmusic 曲目右键下载同义）
        connect(tap, &QQuickTapHandler::longPressed, this, [this, tap]() {
            const int idx = songRowAt(tap->point().position());
            if (idx < 0 || idx >= m_songs.size()) return;

            // 旧菜单清理（重复长按不叠加）
            for (auto* old : findChildren<QskMenu*>())
                old->deleteLater();
            for (auto* old : findChildren<MenuOverlay*>())
                old->deleteLater();

            auto* menu = new QskMenu(this);
            menu->setModal(true);
            menu->setPopupFlag(QskPopup::DeleteOnClose, false);
            const bool faved = Favorites::instance().contains(m_songs.at(idx).id);
            const int idxFav = menu->addOption(
                QskLabelData(QString::fromUtf8(faved ? "取消收藏：%1" : "收藏：%1")
                    .arg(elided(m_songs.at(idx).name, 10))));
            const int idxDl = menu->addOption(
                QskLabelData(QString::fromUtf8("下载：%1")
                    .arg(elided(m_songs.at(idx).name, 10))));
            menu->setOrigin(tap->point().position());
            const Song song = m_songs.at(idx);
            connect(menu, &QskMenu::triggered, this, [this, idx, song, idxFav, idxDl](int i) {
                if (i == idxFav) {
                    Favorites::instance().toggle(song);
                    ToastPopup::show(this,
                        Favorites::instance().contains(song.id)
                            ? QString::fromUtf8("已收藏")
                            : QString::fromUtf8("已取消收藏"));
                }
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

    connect(&NcmCatalog::instance(), &NcmCatalog::playlistTracksDone, this,
        [this](const PlaylistInfo& info, const QString& err) {
            if (info.id != m_playlistId) return;
            if (!err.isEmpty()) {
                m_status->setStatus(StatusLine::Error,
                    QString::fromUtf8("加载失败：%1").arg(err));
                return;
            }
            m_songs = info.songs;
            if (!info.name.isEmpty()) {
                m_title = info.name;
                m_header->setText(elided(m_title, 12));
            }
            if (!info.cover.isEmpty()) {
                m_plCover->setFallback(qximage::identicon(
                    QString::number(info.id), IconItem::pxForDp(48)));
                m_plCover->setSource(info.cover, 48);
                m_plCover->setVisible(true);
            }
            if (m_songs.isEmpty()) {
                m_status->setStatus(StatusLine::Info, QString::fromUtf8("歌单暂无曲目"));
                m_count->setText(QString());
            } else {
                m_status->clearStatus();
                m_count->setText(QString::fromUtf8("共 %1 首").arg(m_songs.size()));
            }
            buildRows();
        });

    NcmCatalog::instance().loadPlaylistTracksAsync(m_playlistId);

    // 手动下载：先取 URL（不触发播放），再入下载队列
    connect(&NcmApi::instance(), &NcmApi::downloadUrlDone, this,
        [this](qint64 id, const QString& url, const QString& err) {
            if (id != m_pendingDlId) return;
            m_pendingDlId = 0;
            if (!err.isEmpty() || url.isEmpty()) {
                ToastPopup::show(this, QString::fromUtf8("获取下载链接失败：%1").arg(err));
                return;
            }
            QString suffix = QUrl(url).path().section(QLatin1Char('.'), -1);
            if (suffix.isEmpty() || suffix.size() > 5)
                suffix = QStringLiteral("mp3");
            DownloadService::instance().enqueue(id, url, suffix);
            ToastPopup::show(this, QString::fromUtf8("已加入下载"));
        });

    // 对号只由列表数据（服务端 playable）决定；播放失败不改列表标记

    // 正在播放 ▶ 标识：切歌只改文本，不重建列表
    connect(&Playback::instance(), &Playback::songChanged, this,
        [this]() { updatePlayingMarker(); });
}

void PlaylistTracksPage::buildRows()
{
    const auto children = m_rows->childItems();
    for (auto* c : children) c->deleteLater();
    m_playingMarkers.clear();

    for (int i = 0; i < m_songs.size(); ++i) {
        const auto& s = m_songs.at(i);
        auto* row = new QskLinearBox(Qt::Horizontal, m_rows);
        row->setPreferredHeight(44);

        // 专辑封面（36dp 固定尺寸，不参与布局收缩）
        auto* icon = new IconItem(row);
        icon->setPreferredSize(36, 36);
        icon->setCornerRadius(6);
        icon->setFallback(qximage::identicon(
            s.albumId > 0 ? QString::number(s.albumId) : QString::number(s.id),
            IconItem::pxForDp(36)));
        icon->setSource(s.cover, 36);

        // 正在播放标识列（▶ / 空，14dp）
        auto* marker = new QskTextLabel(QString(), row);
        marker->setPreferredWidth(14);
        marker->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
        marker->setTextColor(QColor(0x3d, 0x8b, 0xfd));
        m_playingMarkers.append({ s.id, marker });

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

        // 单独播放按钮（保留；桌面 clicked 生效，安卓触摸由外层 TapHandler 兜底）
        auto* playBtn = new QskPushButton(QString::fromUtf8("▶"), row);
        playBtn->setPreferredSize(30, 30);
        connect(playBtn, &QskAbstractButton::clicked, this, [this, i]() {
            qDebug() << "[DIAG] rowPlay clicked row=" << i;
            Playback::instance().setQueue(m_songs, i);
        });
    }
    updatePlayingMarker();
    m_rows->update();
}

void PlaylistTracksPage::updatePlayingMarker()
{
    const qint64 playing = Playback::instance().currentSongId();
    for (auto& p : m_playingMarkers) {
        if (p.second)
            p.second->setText(p.first != 0 && p.first == playing
                ? QString::fromUtf8("▶") : QString());
    }
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

void PlaylistTracksPage::requestDownload(int index)
{
    if (index < 0 || index >= m_songs.size()) return;
    const qint64 id = m_songs.at(index).id;
    if (DownloadService::instance().isCached(id, QStringLiteral("mp3"))) {
        ToastPopup::show(this, QString::fromUtf8("已缓存，无需下载"));
        return;
    }
    m_pendingDlId = id;
    NcmApi::instance().resolveForDownloadAsync(id);
}
