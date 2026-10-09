#include "sidenav.h"
#include "musicstyle.h"
#include "searchline.h"
#include "translator.h"

#include <qglobal.h>
#include <qpainter.h>
#include <qevent.h>
#include <qfontmetrics.h>

namespace {
const int kTop = 8;
const int kSearchH = 28;
const int kItemH = 30;
const int kGroupH = 26;
}

SideNav::SideNav(QWidget* parent)
    : QWidget(parent), m_search(0), m_current(-1), m_hover(-1)
{
    setMouseTracking(true);
    setMinimumWidth(170);
    setMaximumWidth(280);
    m_search = new SearchLineEdit(this);
    retranslateUi();
}

void SideNav::retranslateUi()
{
    m_navRecommend = _("nav.recommend");
    m_navRank = _("nav.rank");
    m_navMine = _("nav.mymusic");
    m_groupCreated = _("nav.created");
    m_groupCollected = _("nav.collected");
    if (m_search)
        m_search->setPlaceholders(_("nav.search"), _("nav.search"));
    relayout();
    update();
}

void SideNav::setLibrary(const std::vector<Playlist>& lib)
{
    m_lib = lib;
    m_current = lib.empty() ? -1 : lib[0].id;
    relayout();
    update();
}

void SideNav::setCurrentPlaylist(int playlistId)
{
    m_current = playlistId;
    update();
}

void SideNav::relayout()
{
    m_items.clear();
    int y = kTop + kSearchH + 8;

    NavItem it;
    it.id = -1; it.text = m_navRecommend; it.y = y; it.h = kItemH; y += kItemH;
    m_items.push_back(it);
    it.id = -2; it.text = m_navRank; it.y = y; it.h = kItemH; y += kItemH;
    m_items.push_back(it);
    it.id = -3; it.text = m_navMine; it.y = y; it.h = kItemH; y += kItemH;
    m_items.push_back(it);

    y += 8;
    it.id = 0; it.text = m_groupCreated; it.y = y; it.h = kGroupH; y += kGroupH;
    m_items.push_back(it);
    for (size_t i = 0; i < m_lib.size(); ++i) {
        if (i % 2 != 0) continue;
        it.id = m_lib[i].id; it.text = m_lib[i].name; it.y = y; it.h = kItemH; y += kItemH;
        m_items.push_back(it);
    }

    y += 6;
    it.id = 0; it.text = m_groupCollected; it.y = y; it.h = kGroupH; y += kGroupH;
    m_items.push_back(it);
    for (size_t i = 0; i < m_lib.size(); ++i) {
        if (i % 2 == 0) continue;
        it.id = m_lib[i].id; it.text = m_lib[i].name; it.y = y; it.h = kItemH; y += kItemH;
        m_items.push_back(it);
    }
}

int SideNav::itemAt(const QPoint& p) const
{
    for (size_t i = 0; i < m_items.size(); ++i)
        if (p.y() >= m_items[i].y && p.y() < m_items[i].y + m_items[i].h)
            return (int)i;
    return -1;
}

void SideNav::resizeEvent(QResizeEvent* e)
{
    QWidget::resizeEvent(e);
    if (m_search)
        m_search->setGeometry(10, kTop, width() - 20, kSearchH);
}

void SideNav::paintEvent(QPaintEvent*)
{
    const StyleParams::Palette& pal = muiPalette();
    QPainter p(this);
    p.fillRect(rect(), pal.baseBg);

    QFont normal = font();
    QFont bold = font();
    bold.setBold(true);
    QFont small = font();
    small.setPointSize(qMax(7, small.pointSize() - 2));

    for (size_t i = 0; i < m_items.size(); ++i) {
        const NavItem& it = m_items[i];
        QRect r(0, it.y, width(), it.h);
        if (it.id == 0) {
            p.setFont(small);
            p.setPen(pal.textMuted);
            p.drawText(QRect(18, r.y(), width() - 24, r.height()), Qt::AlignVCenter | Qt::AlignLeft, it.text);
            continue;
        }
        bool selected = (m_current == it.id);
        bool hovered = ((int)i == m_hover);
        if (hovered)
            p.fillRect(r, pal.hoverBg);
        if (selected) {
            p.fillRect(r, pal.activeBg);
            p.fillRect(QRect(0, r.y() + 4, 3, r.height() - 8), pal.accent);
        }
        p.setFont(selected ? bold : normal);
        p.setPen(selected ? pal.accent : pal.textPrimary);
        p.drawText(QRect(20, r.y(), width() - 28, r.height()), Qt::AlignVCenter | Qt::AlignLeft, it.text);
    }
}

void SideNav::mousePressEvent(QMouseEvent* e)
{
    int idx = itemAt(e->pos());
    if (idx < 0) return;
    const NavItem& it = m_items[idx];
    if (it.id == 0) return;

    if (it.id < 0) {
        int libIdx = -1 - it.id;
        if (libIdx >= 0 && libIdx < (int)m_lib.size()) {
            m_current = m_lib[libIdx].id;
            emit playlistSelected(m_current);
            update();
        }
        return;
    }
    m_current = it.id;
    emit playlistSelected(it.id);
    update();
}

void SideNav::mouseMoveEvent(QMouseEvent* e)
{
    int idx = itemAt(e->pos());
    if (idx != m_hover) {
        m_hover = idx;
        update();
    }
}

void SideNav::leaveEvent(QEvent* e)
{
    QWidget::leaveEvent(e);
    if (m_hover != -1) {
        m_hover = -1;
        update();
    }
}
