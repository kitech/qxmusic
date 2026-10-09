#include "contentview.h"
#include "musicstyle.h"
#include "translator.h"
#include "identicon.h"
#include "LimeScrollBar.h"

#include <qglobal.h>
#include <qpainter.h>
#include <qevent.h>
#include <qfontmetrics.h>

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
}

ContentView::ContentView(QWidget* parent)
    : QWidget(parent), m_pl(0), m_scrollBar(0), m_scrollY(0), m_scrollDelta(0),
      m_hover(-1), m_current(-1), m_playlistId(-1), m_headerH(64), m_rowH(48)
{
    setMouseTracking(true);
    m_scrollBar = new LimeScrollBar(Qt::Vertical, this);
    QObject::connect(m_scrollBar, SIGNAL(valueChanged(int)), this, SLOT(onScroll(int)));
    retranslateUi();
}

void ContentView::retranslateUi()
{
    m_playAll = _("content.playall");
    update();
}

void ContentView::setLibrary(const std::vector<Playlist>& lib)
{
    m_lib = lib;
    if (!m_lib.empty())
        showPlaylist(m_lib[0].id);
}

void ContentView::showPlaylist(int playlistId)
{
    m_pl = findPlaylist(m_lib, playlistId);
    m_playlistId = playlistId;
    m_current = -1;
    m_scrollY = 0;
    rebuildRows();
    updateScrollBar();
    update();
}

void ContentView::rebuildRows()
{
    m_rows.clear();
    if (!m_pl) return;
    for (size_t i = 0; i < m_pl->tracks.size(); ++i) {
        Row r;
        r.trackIndex = (int)i;
        r.y = m_headerH + (int)i * m_rowH;
        r.h = m_rowH;
        m_rows.push_back(r);
    }
}

int ContentView::totalHeight() const
{
    return m_headerH + (int)m_rows.size() * m_rowH;
}

QRect ContentView::playAllRect() const
{
    return QRect(20, 34, 92, 24);
}

int ContentView::rowAt(const QPoint& p) const
{
    int y = p.y() + m_scrollY;
    if (y < m_headerH) return -1;
    int idx = (y - m_headerH) / m_rowH;
    if (idx < 0 || idx >= (int)m_rows.size()) return -1;
    return idx;
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

    QString title = m_pl ? m_pl->name : QString();
    p.setFont(titleFont);
    p.setPen(pal.textPrimary);
    p.drawText(QRect(20, 4, width() - 40, 30), Qt::AlignVCenter | Qt::AlignLeft, title);

    QRect pa = playAllRect();
    p.setPen(Qt::NoPen);
    p.setBrush(pal.accent);
    muiDrawRoundRect(p, pa, pa.height() / 2, pa.height() / 2);
    p.setPen(pal.accentText);
    p.setFont(small);
    p.drawText(pa, Qt::AlignCenter, m_playAll);

    p.setPen(pal.border);
    p.drawLine(0, m_headerH - 1, width(), m_headerH - 1);

    p.setClipRect(0, m_headerH, width(), height() - m_headerH);

    if (m_pl) {
        for (size_t i = 0; i < m_rows.size(); ++i) {
            const Row& row = m_rows[i];
            int y = row.y - m_scrollY;
            if (y + row.h < m_headerH || y > height()) continue;
            const Track& t = m_pl->tracks[row.trackIndex];

            QRect rr(0, y, width() - 12, row.h);
            bool selected = ((int)i == m_current);
            bool hovered = ((int)i == m_hover);
            if (selected)
                p.fillRect(rr, pal.activeBg);
            else if (hovered)
                p.fillRect(rr, pal.hoverBg);

            p.setFont(small);
            p.setPen(selected ? pal.accent : pal.textMuted);
            p.drawText(QRect(16, y, 28, row.h), Qt::AlignVCenter | Qt::AlignRight,
                       QString::number(row.trackIndex + 1));

            QPixmap cover = generateIdenticon(t.coverSeed, 36);
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

            p.drawText(QRect(width() - 90, y, 60, row.h), Qt::AlignVCenter | Qt::AlignRight,
                       fmtMs(t.durationMs));
        }
    }
}

void ContentView::mousePressEvent(QMouseEvent* e)
{
    if (playAllRect().contains(e->pos())) {
        if (m_pl && !m_pl->tracks.empty()) {
            m_current = 0;
            emit trackActivated(m_playlistId, 0);
            update();
        }
        return;
    }
    int idx = rowAt(e->pos());
    if (idx < 0 || !m_pl) return;
    m_current = idx;
    emit trackActivated(m_playlistId, m_rows[idx].trackIndex);
    update();
}

void ContentView::mouseMoveEvent(QMouseEvent* e)
{
    int idx = rowAt(e->pos());
    if (idx != m_hover) {
        m_hover = idx;
        update();
    }
}

void ContentView::leaveEvent(QEvent* e)
{
    QWidget::leaveEvent(e);
    if (m_hover != -1) {
        m_hover = -1;
        update();
    }
}
