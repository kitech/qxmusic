#include "contentview.h"
#include "musicstyle.h"
#include "translator.h"
#include "sharedstatusbar.h"
#include "identicon.h"
#include "iconloader.h"
#include "LimeScrollBar.h"

#include <qglobal.h>
#include <qpainter.h>
#include <qevent.h>
#include <qfontmetrics.h>
#ifdef QT3_BUILD
#include <qpopupmenu.h>
#else
#include <qmenu.h>
#endif

namespace {
QString fmtMs(int ms)
{
    int total = ms / 1000;
    int m = total / 60;
    int s = total % 60;
    QString ss = QString::number(s);
    if (s < 10) ss = QString("0") + ss;
    return QString::number(m) + ":" + ss;
}

/* 播放次数：>=1e8 → x.x亿，>=1e4 → x.x万，其余原样 */
QString fmtCount(qint64 n)
{
    if (n >= 100000000LL)
        return QString::number((double)n / 100000000.0, 'f', 1) + _("num.yi");
    if (n >= 10000LL)
        return QString::number((double)n / 10000.0, 'f', 1) + _("num.wan");
    return QString::number((unsigned long long)n);
}
}

ContentView::ContentView(QWidget* parent)
    : QWidget(parent), m_pl(0), m_radioList(false),
      m_radioProgramMode(false), m_netRadioMode(false),
      m_mode(TracksMode), m_scrollBar(0), m_scrollY(0),
      m_scrollDelta(0), m_hover(-1), m_current(-1), m_hoverRefresh(false),
      m_playingNcmId(0), m_playlistId(-1),
      m_headerH(64), m_rowH(48), m_hasMore(false), m_loadingNow(false)
{
    setMouseTracking(true);
    m_scrollBar = new LimeScrollBar(Qt::Vertical, this);
    QObject::connect(m_scrollBar, SIGNAL(valueChanged(int)), this, SLOT(onScroll(int)));
    QObject::connect(&IconLoader::instance(), SIGNAL(iconsReady()),
                     this, SLOT(update()));
    retranslateUi();
}

void ContentView::retranslateUi()
{
    m_playAll = _("content.playall");
    m_back = _("content.back");
    m_loadMore = _("content.load_more");
    m_loading = _("content.loading");
    update();
}

void ContentView::setPlayingTrackId(qint64 ncmId)
{
    if (m_playingNcmId == ncmId) return;
    m_playingNcmId = ncmId;
    update();   /* 只重绘，不重建列表 */
}

void ContentView::setLibrary(const std::vector<Playlist>& lib)
{
    m_lib = lib;
    /* m_pl 指向旧 m_lib 元素，重拷贝后失效：按 m_playlistId 重找，
     * 找不到再回落到第一个歌单 */
    if (m_mode == TracksMode) {
        const Playlist* cur = (m_playlistId >= 0) ? findPlaylist(m_lib, m_playlistId) : 0;
        if (cur) {
            m_pl = cur;
            m_current = -1;
            m_scrollY = 0;
            rebuildRows();
            updateScrollBar();
        } else if (!m_lib.empty()) {
            showPlaylist(m_lib[0].id);
        } else {
            m_pl = 0;
            m_playlistId = -1;
            rebuildRows();
        }
    }
    update();
}

void ContentView::showPlaylist(int playlistId)
{
    m_mode = TracksMode;
    m_radioProgramMode = false;
    m_netRadioMode = false;
    m_cards.clear();
    m_gridCats.clear();
    m_pl = findPlaylist(m_lib, playlistId);
    m_playlistId = playlistId;
    m_current = -1;
    m_scrollY = 0;
    m_hasMore = false;
    m_loadingNow = false;
    rebuildRows();
    updateScrollBar();
    update();
}

void ContentView::showPlaylistList(const QString& title, const std::vector<RemotePlaylistCard>& cards,
                                   bool hasMore, bool loading)
{
    m_mode = PlaylistListMode;
    m_radioList = false;
    m_netRadioMode = false;
    m_radioProgramMode = false;
    m_pl = 0;
    m_playlistId = -1;
    m_listTitle = title;
    m_cards = cards;
    m_hasMore = hasMore;
    m_loadingNow = loading;
    m_hover = -1;
    m_current = -1;
    m_scrollY = 0;
    rebuildRows();
    updateScrollBar();
    update();
}

void ContentView::showRadioList(const QString& title,
                                const std::vector<RemotePlaylistCard>& radios,
                                bool hasMore, bool loading)
{
    m_mode = PlaylistListMode;
    m_radioList = true;
    m_netRadioMode = false;
    m_radioProgramMode = false;
    m_pl = 0;
    m_playlistId = -1;
    m_listTitle = title;
    m_cards = radios;
    m_hasMore = hasMore;
    m_loadingNow = loading;
    m_hover = -1;
    m_current = -1;
    m_scrollY = 0;
    rebuildRows();
    updateScrollBar();
    update();
}

void ContentView::showRadioPrograms(qint64 rid, const QString& title,
                                    const std::vector<Track>& programs)
{
    m_tempPl = Playlist();
    m_tempPl.id = (int)(rid & 0x7fffffff);
    m_tempPl.name = title;
    m_tempPl.tracks = programs;
    m_tempPl.trackCount = (int)programs.size();
    m_mode = TracksMode;
    m_radioList = false;
    m_netRadioMode = false;
    m_radioProgramMode = true;   /* 计数单位「期」 */
    m_pl = &m_tempPl;
    m_playlistId = m_tempPl.id;
    m_current = -1;
    m_scrollY = 0;
    m_hasMore = false;
    m_loadingNow = false;
    rebuildRows();
    updateScrollBar();
    update();
}

void ContentView::setListLoading(bool loading)
{
    if (m_mode != PlaylistListMode) return;
    m_loadingNow = loading;
    rebuildRows();
    updateScrollBar();
    update();
}

void ContentView::showCategoryGrid(const QStringList& cats)
{
    m_mode = CategoryGridMode;
    m_pl = 0;
    m_playlistId = -1;
    m_listTitle = _("content.categories");
    m_gridCats = cats;
    m_netRadioMode = false;
    m_radioProgramMode = false;
    m_hasMore = false;
    m_loadingNow = false;
    m_hover = -1;
    m_current = -1;
    m_scrollY = 0;
    rebuildRows();
    updateScrollBar();
    update();
}

/* 网络电台 · 地区内固定清单：复用 TracksMode 行，HLS 行灰态不可播 */
void ContentView::showNetRadioRegion(const QString& regionKey, const QString& title)
{
    m_netRadioStreams = radiolist::byRegion(regionKey);
    m_netRadioMode = true;
    m_radioProgramMode = false;
    m_netRegion = regionKey;
    m_tempPl = Playlist();
    m_tempPl.id = -90;                       /* 伪 playlist id，避免与现有冲突 */
    m_tempPl.name = title;
    m_tempPl.trackCount = (int)m_netRadioStreams.size();
    for (size_t i = 0; i < m_netRadioStreams.size(); ++i) {
        const RadioStation& s = m_netRadioStreams[i];
        Track t;
        t.id = (int)(i + 1);
        t.ncmId = 0;
        t.albumId = 0;
        t.title = QString::fromUtf8(s.name);
        t.artist = s.hls ? QString::fromUtf8("· HLS") : QString();   /* 信息标签，不再灰态 */
        t.album = QString();
        t.durationMs = 0;
        t.coverUrl = QString();
        t.coverSeed = QString("radio-%1-%2").arg(regionKey).arg((int)i);
        t.url = QString::fromUtf8(s.url);
        t.playable = true;   /* HLS 走 ffmpeg/mpv 后端原生 demux */
        m_tempPl.tracks.push_back(t);
    }
    m_mode = TracksMode;
    m_pl = &m_tempPl;
    m_playlistId = m_tempPl.id;
    m_listTitle = title;
    m_hasMore = false;
    m_loadingNow = false;
    m_hover = -1;
    m_current = -1;
    m_scrollY = 0;
    rebuildRows();
    updateScrollBar();
    update();
}

void ContentView::rebuildRows()
{
    m_rows.clear();
    if (m_mode == TracksMode) {
        if (!m_pl) return;
        for (size_t i = 0; i < m_pl->tracks.size(); ++i) {
            Row r;
            r.index = (int)i;
            r.y = m_headerH + (int)i * m_rowH;
            r.h = m_rowH;
            m_rows.push_back(r);
        }
    } else if (m_mode == PlaylistListMode) {
        for (size_t i = 0; i < m_cards.size(); ++i) {
            Row r;
            r.index = (int)i;
            r.y = m_headerH + (int)i * 56;
            r.h = 56;
            m_rows.push_back(r);
        }
    } else if (m_mode == CategoryGridMode) {
        const int cols = qMax(1, (width() - 40) / 180);
        for (int i = 0; i < (int)m_gridCats.size(); ++i) {
            Row r;
            r.index = i;
            r.y = m_headerH + (i / cols) * 52;
            r.h = 44;
            m_rows.push_back(r);
        }
    }
}

int ContentView::totalHeight() const
{
    if (m_mode == TracksMode)
        return m_headerH + (int)m_rows.size() * m_rowH;
    if (m_mode == PlaylistListMode) {
        int h = m_headerH + (int)m_rows.size() * 56;
        if (m_loadingNow) h += 44;
        else if (m_hasMore) h += 56;
        return h;
    }
    if (m_mode == CategoryGridMode) {
        const int cols = qMax(1, (width() - 40) / 180);
        if (m_rows.empty()) return m_headerH;
        const Row& last = m_rows.back();
        return last.y + last.h + 8;
    }
    return m_headerH;
}

QRect ContentView::playAllRect() const
{
    return QRect(20, 34, 92, 24);
}

QRect ContentView::backRect() const
{
    return QRect(width() - 92, 34, 72, 24);
}

QRect ContentView::refreshRect() const
{
    /* 紧贴返回按钮左侧：28x24 圆角小按钮 */
    return QRect(width() - 128, 34, 28, 24);
}

qint64 ContentView::currentNcmPlaylistId() const
{
    return (m_mode == TracksMode && m_pl) ? m_pl->ncmId : 0;
}

QRect ContentView::loadMoreRect() const
{
    return QRect(width() / 2 - 90, m_headerH + (int)m_rows.size() * 56 + 6, 180, 36);
}

int ContentView::rowAt(const QPoint& p) const
{
    int y = p.y() + m_scrollY;
    if (y < m_headerH) return -1;
    if (m_mode == TracksMode) {
        int idx = (y - m_headerH) / m_rowH;
        if (idx < 0 || idx >= (int)m_rows.size()) return -1;
        return idx;
    }
    if (m_mode == PlaylistListMode) {
        int idx = (y - m_headerH) / 56;
        if (idx < 0 || idx >= (int)m_rows.size()) return -1;
        return idx;
    }
    if (m_mode == CategoryGridMode) {
        for (size_t i = 0; i < m_rows.size(); ++i)
            if (y >= m_rows[i].y && y < m_rows[i].y + m_rows[i].h)
                return (int)i;
    }
    return -1;
}

void ContentView::onScroll(int value)
{
    if (value == m_scrollY) return;
    m_scrollY = value;
    update();
}

void ContentView::updateScrollBar()
{
    int viewH = height() - m_headerH;
    int totalH = totalHeight();
    int maxScroll = qMax(0, totalH - viewH);
    if (m_scrollY > maxScroll) m_scrollY = maxScroll;
    if (m_scrollBar) {
        m_scrollBar->blockSignals(true);
        m_scrollBar->setRange(0, maxScroll);
        m_scrollBar->setPageStep(qMax(1, viewH));
        m_scrollBar->setValue(m_scrollY);
        m_scrollBar->blockSignals(false);
    }
}

void ContentView::resizeEvent(QResizeEvent* e)
{
    QWidget::resizeEvent(e);
    if (m_scrollBar)
        m_scrollBar->setGeometry(width() - 12, m_headerH, 12, qMax(0, height() - m_headerH));
    if (m_mode == CategoryGridMode)
        rebuildRows();
    updateScrollBar();
}

void ContentView::wheelEvent(QWheelEvent* e)
{
    m_scrollDelta += qWheelDeltaY(e);
    int steps = m_scrollDelta / 120;
    if (steps == 0) return;
    m_scrollDelta -= steps * 120;
    int newY = m_scrollY - steps * (m_rowH * 3);
    int viewH = height() - m_headerH;
    int maxScroll = qMax(0, totalHeight() - viewH);
    m_scrollY = qBound(0, newY, maxScroll);
    updateScrollBar();
    update();
}

void ContentView::paintEvent(QPaintEvent*)
{
    const StyleParams::Palette& pal = muiPalette();
    QPainter p(this);
    p.fillRect(rect(), pal.windowBg);

    QFont titleFont = font();
    titleFont.setBold(true);
    titleFont.setPointSize(qMax(10, titleFont.pointSize() + 4));
    QFont normal = font();
    QFont bold = font();
    bold.setBold(true);
    QFont small = font();
    small.setPointSize(qMax(7, small.pointSize() - 1));

    /* ── 标题区 ── */
    if (m_mode == TracksMode) {
        QString title = m_pl ? m_pl->name : QString();
        if (m_pl) {
            /* 计数单位随模式：歌曲=首 / 电台节目=期 / 网络电台=个 */
            const char* unit = m_netRadioMode ? " %1 个"
                             : (m_radioProgramMode ? " %1 期" : " %1 首");
            title += QString::fromUtf8(unit).arg((int)m_pl->tracks.size());
        }
        p.setFont(titleFont);
        p.setPen(pal.textPrimary);
        /* 歌单封面（40px 圆角 8）：标题右移留位 */
        int tx = 20;
        if (m_pl && m_pl->ncmId > 0 && !m_pl->coverUrl.isEmpty()) {
            const int cs = 40;
            QPixmap c = IconLoader::instance().rounded(
                IconLoader::instance().cover(
                    m_pl->coverUrl, QString::number((long long)m_pl->ncmId), cs),
                cs, 8);
            if (!c.isNull()) {
                p.drawPixmap(20, 4, c);
                tx = 20 + cs + 10;
            }
        }
        p.drawText(QRect(tx, 4, width() - tx - 20, 30), Qt::AlignVCenter | Qt::AlignLeft, title);

        QRect pa = playAllRect();
        p.setPen(Qt::NoPen);
        p.setBrush(pal.accent);
        muiDrawRoundRect(p, pa, pa.height() / 2, pa.height() / 2);
        p.setPen(pal.accentText);
        p.setFont(small);
        p.drawText(pa, Qt::AlignCenter, m_playAll);

        /* 刷新 ⟳（仅 NCM 歌单可重取；本地歌单/节目/静态电台不画） */
        if (m_pl && m_pl->ncmId > 0) {
            QRect fr = refreshRect();
            p.setPen(Qt::NoPen);
            p.setBrush(m_hoverRefresh ? pal.hoverBg : pal.windowBg);
            muiDrawRoundRect(p, fr, fr.height() / 2, fr.height() / 2);
            p.setPen(pal.textPrimary);
            p.setFont(small);
            p.drawText(fr, Qt::AlignCenter, QString::fromUtf8("⟳"));
        }
    } else {
        /* List/Grid 模式：标题 + 返回按钮（列表模式带条目计数） */
        QString title = m_listTitle;
        if (m_mode == PlaylistListMode) {
            title += m_radioList
                ? QString::fromUtf8(" · %1 个电台").arg((int)m_cards.size())
                : QString::fromUtf8(" · %1 个歌单").arg((int)m_cards.size());
        }
        p.setFont(titleFont);
        p.setPen(pal.textPrimary);
        p.drawText(QRect(20, 4, width() - 220, 30), Qt::AlignVCenter | Qt::AlignLeft, title);

        /* 刷新 ⟳（分类歌单/站点电台列表可重载） */
        if (m_mode == PlaylistListMode) {
            QRect fr = refreshRect();
            p.setPen(Qt::NoPen);
            p.setBrush(m_hoverRefresh ? pal.hoverBg : pal.windowBg);
            muiDrawRoundRect(p, fr, fr.height() / 2, fr.height() / 2);
            p.setPen(pal.textPrimary);
            p.setFont(small);
            p.drawText(fr, Qt::AlignCenter, QString::fromUtf8("⟳"));
        }

        QRect br = backRect();
        p.setPen(Qt::NoPen);
        p.setBrush(pal.hoverBg);
        muiDrawRoundRect(p, br, br.height() / 2, br.height() / 2);
        p.setPen(pal.textPrimary);
        p.setFont(small);
        p.drawText(br, Qt::AlignCenter, QString::fromUtf8("← ") + m_back);
    }

    p.setPen(pal.border);
    p.drawLine(0, m_headerH - 1, width(), m_headerH - 1);

    p.setClipRect(0, m_headerH, width(), height() - m_headerH);

    if (m_mode == TracksMode) {
        if (m_pl) {
            for (size_t i = 0; i < m_rows.size(); ++i) {
                const Row& row = m_rows[i];
                int y = row.y - m_scrollY;
                if (y + row.h < m_headerH || y > height()) continue;
                const Track& t = m_pl->tracks[row.index];

                QRect rr(0, y, width() - 12, row.h);
                bool selected = ((int)i == m_current);
                bool hovered = ((int)i == m_hover);
                if (selected)
                    p.fillRect(rr, pal.activeBg);
                else if (hovered)
                    p.fillRect(rr, pal.hoverBg);

                /* 正在播放标识：非本地曲、ncmId 命中 → 序号前 ▶ */
                if (t.ncmId > 0 && t.ncmId == m_playingNcmId) {
                    p.setFont(small);
                    p.setPen(pal.accent);
                    p.drawText(QRect(2, y, 14, row.h),
                               Qt::AlignVCenter | Qt::AlignLeft,
                               QString::fromUtf8("▶"));
                }

                p.setFont(small);
                p.setPen(selected ? pal.accent : pal.textMuted);
                p.drawText(QRect(16, y, 28, row.h), Qt::AlignVCenter | Qt::AlignRight,
                           QString::number(row.index + 1));

                QPixmap cover = IconLoader::instance().rounded(
                    IconLoader::instance().cover(t.coverUrl, t.coverSeed, 36), 36, 6);
                p.drawPixmap(56, y + (row.h - 36) / 2, cover);

                p.setFont(selected ? bold : normal);
                p.setPen(selected ? pal.accent : pal.textPrimary);
                p.drawText(QRect(104, y, width() - 200, row.h), Qt::AlignVCenter | Qt::AlignLeft, t.title);

                QFontMetrics fm(selected ? bold : normal);
                int titleW = qFontWidth(fm, t.title);
                p.setFont(small);
                p.setPen(pal.textMuted);
                QString sub = t.artist + QString::fromUtf8(" · ") + t.album;
                p.drawText(QRect(104 + titleW + 14, y, width() - 260 - titleW, row.h),
                           Qt::AlignVCenter | Qt::AlignLeft, sub);

                QString dur = fmtMs(t.durationMs);
                /* 非灰歌（NCM 源 + 可播）时长后缀对号区分；
                 * 本地歌曲（ncmId==0）不标，避免满屏 ✓ 噪音 */
                if (t.ncmId > 0 && t.playable)
                    dur += QString::fromUtf8(" ✓");
                p.drawText(QRect(width() - 90, y, 60, row.h), Qt::AlignVCenter | Qt::AlignRight,
                           dur);
            }
        }
    } else if (m_mode == PlaylistListMode) {
        for (size_t i = 0; i < m_rows.size(); ++i) {
            const Row& row = m_rows[i];
            int y = row.y - m_scrollY;
            if (y + row.h < m_headerH || y > height()) continue;
            const RemotePlaylistCard& c = m_cards[row.index];

            QRect rr(0, y, width() - 12, row.h);
            bool hovered = ((int)i == m_hover);
            if (hovered)
                p.fillRect(rr, pal.hoverBg);

            p.setFont(small);
            p.setPen(pal.textMuted);
            p.drawText(QRect(16, y, 28, row.h), Qt::AlignVCenter | Qt::AlignRight,
                       QString::number(row.index + 1));

            QPixmap cover = IconLoader::instance().rounded(
                IconLoader::instance().cover(
                    c.coverUrl, QString::number((long long)c.id), 40), 40, 6);
            p.drawPixmap(56, y + (row.h - 40) / 2, cover);

            p.setFont(bold);
            p.setPen(pal.textPrimary);
            p.drawText(QRect(108, y, width() - 220, row.h), Qt::AlignVCenter | Qt::AlignLeft, c.name);

            QFontMetrics fm(bold);
            int nameW = qFontWidth(fm, c.name);
            p.setFont(small);
            p.setPen(pal.textMuted);
            QString sub = c.creator + QString::fromUtf8(" · ") +
                          QString::number(c.trackCount) + QString::fromUtf8(" · ") +
                          fmtCount(c.playCount);
            p.drawText(QRect(108 + nameW + 14, y, width() - 280 - nameW, row.h),
                       Qt::AlignVCenter | Qt::AlignLeft, sub);
        }

        /* footer：加载中 or 加载更多 */
        if (m_loadingNow) {
            QRect lr(0, m_headerH + (int)m_rows.size() * 56 - m_scrollY, width(), 44);
            p.setFont(small);
            p.setPen(pal.textMuted);
            p.drawText(lr, Qt::AlignCenter, m_loading);
        } else if (m_hasMore) {
            QRect lr = loadMoreRect();
            lr.moveTop(lr.top() - m_scrollY);
            bool hovered = (m_hover == 1000000);
            const StyleParams::Palette& pal2 = muiPalette();
            p.setPen(Qt::NoPen);
            p.setBrush(hovered ? pal2.activeBg : pal2.hoverBg);
            muiDrawRoundRect(p, lr, lr.height() / 2, lr.height() / 2);
            p.setPen(pal2.textPrimary);
            p.setFont(small);
            p.drawText(lr, Qt::AlignCenter, m_loadMore);
        }
    } else if (m_mode == CategoryGridMode) {
        int cols = qMax(1, (width() - 40) / 180);
        int cellW = (width() - 40 - (cols - 1) * 12) / cols;
        for (size_t i = 0; i < m_rows.size(); ++i) {
            const Row& row = m_rows[i];
            int y = row.y - m_scrollY;
            if (y + row.h < m_headerH || y > height()) continue;
            int col = (int)i % cols;
            QRect cr(20 + col * (cellW + 12), y, cellW, row.h);
            bool hovered = ((int)i == m_hover);
            p.setPen(Qt::NoPen);
            p.setBrush(hovered ? pal.activeBg : pal.hoverBg);
            muiDrawRoundRect(p, cr, cr.height() / 2, cr.height() / 2);
            p.setPen(pal.textPrimary);
            p.setFont(normal);
            p.drawText(cr, Qt::AlignCenter, m_gridCats[row.index]);
        }
    }
}

void ContentView::contextMenuEvent(QContextMenuEvent* e)
{
    /* TracksMode 曲目行右键：收藏/取消收藏（收藏状态由 MainWindow 维护） */
    if (m_mode != TracksMode || !m_pl || m_netRadioMode)
        return;
    const int idx = rowAt(e->pos());
    if (idx < 0 || idx >= (int)m_rows.size())
        return;
    const int trackIndex = m_rows[idx].index;
    /* 文案固定双态合并（状态由 MainWindow 查 FavoriteStore 决定，菜单只发意图） */
    const QString favText = _("fav.toggle");
#ifdef QT3_BUILD
    QPopupMenu menu(this);
    const int idFav = menu.insertItem(favText);
    const int chosen = menu.exec(e->globalPos());
    if (chosen == idFav)
        emit favToggled(m_playlistId, trackIndex);
#else
    QMenu menu(this);
    QAction* actFav = menu.addAction(favText);
    QAction* chosen = menu.exec(e->globalPos());
    if (chosen == actFav)
        emit favToggled(m_playlistId, trackIndex);
#endif
}

void ContentView::mousePressEvent(QMouseEvent* e)
{
    /* 标题行 ⟳：NCM 歌单/列表两种模式都可能出现 */
    if ((m_mode == PlaylistListMode
            || (m_mode == TracksMode && m_pl && m_pl->ncmId > 0))
        && refreshRect().contains(e->pos())) {
        emit refreshRequested();
        return;
    }
    if (m_mode == PlaylistListMode || m_mode == CategoryGridMode) {
        if (backRect().contains(e->pos())) {
            emit backRequested();
            return;
        }
    }
    if (m_mode == PlaylistListMode) {
        /* footer 劫持：load more */
        if (m_hasMore && !m_loadingNow) {
            QRect lr = loadMoreRect();
            lr.moveTop(lr.top() - m_scrollY);
            if (lr.contains(e->pos())) {
                emit loadMoreRequested();
                return;
            }
        }
        int idx = rowAt(e->pos());
        if (idx < 0) return;
        if (m_radioList)
            emit radioSelected(m_cards[idx].id);
        else
            emit playlistActivated(m_cards[idx].id);
        return;
    }
    if (m_mode == CategoryGridMode) {
        int idx = rowAt(e->pos());
        if (idx < 0) return;
        emit categoryActivated(m_gridCats[idx]);
        return;
    }

    /* TracksMode */
    if (playAllRect().contains(e->pos())) {
        if (m_pl && !m_pl->tracks.empty()) {
            /* 网络电台「播放全部」：从第 1 台开始（HLS 首台则提示） */
            if (m_netRadioMode) {
                m_current = 0;
                emit netRadioStreamSelected(0);
                update();
                return;
            }
            m_current = 0;
            emit trackActivated(m_playlistId, 0);
            update();
        }
        return;
    }
    int idx = rowAt(e->pos());
    if (idx < 0 || !m_pl) return;
    /* 网络电台：行=直播流（HLS 行灰态提示，不走 trackActivated） */
    if (m_netRadioMode) {
        m_current = idx;
        emit netRadioStreamSelected(idx);
        update();
        return;
    }
    m_current = idx;
    emit trackActivated(m_playlistId, m_rows[idx].index);
    update();
}

void ContentView::mouseMoveEvent(QMouseEvent* e)
{
    /* ⟳ 悬停态（两种模式通用） */
    {
        const bool hr = refreshRect().contains(e->pos())
            && (m_mode == PlaylistListMode
                || (m_mode == TracksMode && m_pl && m_pl->ncmId > 0));
        if (hr != m_hoverRefresh) {
            m_hoverRefresh = hr;
            update();
        }
    }
    if (m_mode == PlaylistListMode) {
        int hIdx = -1;
        if (m_hasMore && !m_loadingNow) {
            QRect lr = loadMoreRect();
            lr.moveTop(lr.top() - m_scrollY);
            if (lr.contains(e->pos()))
                hIdx = 1000000;   /* load more 悬停哨兵 */
        }
        if (hIdx < 0)
            hIdx = rowAt(e->pos());
        if (hIdx != m_hover) {
            m_hover = hIdx;
            update();
        }
        return;
    }
    if (m_mode == TracksMode || m_mode == CategoryGridMode) {
        int idx = rowAt(e->pos());
        if (idx != m_hover) {
            m_hover = idx;
            update();
        }
        return;
    }
}

void ContentView::leaveEvent(QEvent* e)
{
    QWidget::leaveEvent(e);
    if (m_hover != -1) {
        m_hover = -1;
        update();
    }
    if (m_hoverRefresh) {
        m_hoverRefresh = false;
        update();
    }
}
