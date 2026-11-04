#include "playerbar.h"
#include "musicstyle.h"
#include "translator.h"
#include "identicon.h"

#include <qglobal.h>
#include <qpainter.h>
#include <qevent.h>
#include <qfontmetrics.h>
#ifdef QT3_BUILD
#include <qpointarray.h>
#include <qtooltip.h>
#else
#include <QToolTip>
#endif

#ifdef QT3_BUILD
/* Qt3 区段级动态 tooltip：maybeTip 虚口，每次命中现读 PlayerBar 公开
 * tipRectFor/tipForPos —— 三态/语言切换即时生效，无需 remove/re-add */
class PlayerBarToolTip : public QToolTip {
public:
    explicit PlayerBarToolTip(QWidget* w) : QToolTip(w), m_w(w) {}
    void maybeTip(const QPoint& p)
    {
        PlayerBar* bar = static_cast<PlayerBar*>(m_w);
        QRect r = bar->tipRectFor(p);
        if (!r.isValid()) return;
        QString t = bar->tipForPos(p);
        if (!t.isEmpty()) tip(r, t);
    }
private:
    QWidget* m_w;
};
#endif

#ifdef QT3_BUILD
typedef QPointArray MuiPolygon;
#else
typedef QPolygon MuiPolygon;
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

void drawTriangle(QPainter& p, const QRect& r, bool left, const QColor& c)
{
    MuiPolygon tri(3);
    if (left) {
        tri.setPoint(0, r.right(), r.top());
        tri.setPoint(1, r.right(), r.bottom());
        tri.setPoint(2, r.left(), r.center().y());
    } else {
        tri.setPoint(0, r.left(), r.top());
        tri.setPoint(1, r.left(), r.bottom());
        tri.setPoint(2, r.right(), r.center().y());
    }
    p.setPen(Qt::NoPen);
    p.setBrush(c);
    p.drawPolygon(tri);
}
}

PlayerBar::PlayerBar(QWidget* parent)
    : QWidget(parent), m_hasTrack(false), m_playing(false),
      m_progressMs(0), m_durationMs(0), m_repeatMode(0), m_autoSkip(true),
      m_tip(0),
      m_hover(HitNone), m_dragging(false)
{
    setMouseTracking(true);
    setFixedHeight(80);
    retranslateUi();
#ifdef QT3_BUILD
    m_tip = new PlayerBarToolTip(this);
#else
    m_tip = 0;
#endif
}

PlayerBar::~PlayerBar()
{
#ifdef QT3_BUILD
    delete (PlayerBarToolTip*)m_tip;
#endif
}

void PlayerBar::retranslateUi()
{
    m_tipRepeat = _("player.tip_repeat_off");
    m_tipAutoSkip = _("player.tip_autoskip_on");
    update();
}

void PlayerBar::setTrack(const Track& t)
{
    m_track = t;
    m_hasTrack = true;
    m_progressMs = 0;
    m_durationMs = t.durationMs;
    update();
}

void PlayerBar::setProgress(int ms)
{
    if (m_dragging) return;
    m_progressMs = ms;
    update();
}

void PlayerBar::setDuration(int ms)
{
    if (ms > 0) m_durationMs = ms;
    update();
}

void PlayerBar::setPlaying(bool on)
{
    if (m_playing == on) return;
    m_playing = on;
    emit playStateChanged(on);
    update();
}

void PlayerBar::setRepeatMode(int mode)
{
    if (mode < 0 || mode > 2) mode = 0;
    if (m_repeatMode == mode) return;
    m_repeatMode = mode;
    /* 三态 tooltip 文案即时生效（qt3 maybeTip / qt5 event 均现读成员） */
    if (m_repeatMode == 1) m_tipRepeat = _("player.tip_repeat_all");
    else if (m_repeatMode == 2) m_tipRepeat = _("player.tip_repeat_one");
    else m_tipRepeat = _("player.tip_repeat_off");
    update();
}

void PlayerBar::setAutoSkip(bool on)
{
    if (m_autoSkip == on) return;
    m_autoSkip = on;
    m_tipAutoSkip = on ? _("player.tip_autoskip_on") : _("player.tip_autoskip_off");
    update();
}

QRect PlayerBar::playRect() const
{
    int cx = width() / 2;
    return QRect(cx - 15, 14, 30, 30);
}

QRect PlayerBar::prevRect() const
{
    int cx = width() / 2;
    return QRect(cx - 60, 19, 20, 20);
}

QRect PlayerBar::nextRect() const
{
    int cx = width() / 2;
    return QRect(cx + 40, 19, 20, 20);
}

QRect PlayerBar::repeatRect() const
{
    int cx = width() / 2;
    return QRect(cx + 75, 19, 20, 20);      /* next 右邻 */
}

QRect PlayerBar::autoSkipRect() const
{
    int cx = width() / 2;
    return QRect(cx - 95, 19, 20, 20);      /* prev 左邻，Spotify 式两侧对称 */
}

QRect PlayerBar::tipRectFor(const QPoint& p) const
{
    if (repeatRect().contains(p)) return repeatRect();
    if (autoSkipRect().contains(p)) return autoSkipRect();
    return QRect();
}

QString PlayerBar::tipForPos(const QPoint& p) const
{
    if (repeatRect().contains(p)) return m_tipRepeat;
    if (autoSkipRect().contains(p)) return m_tipAutoSkip;
    return QString();
}

bool PlayerBar::event(QEvent* e)
{
#ifdef QT3_BUILD
    return QWidget::event(e);
#else
    if (e->type() == QEvent::ToolTip) {
        QHelpEvent* he = static_cast<QHelpEvent*>(e);
        QString t = tipForPos(he->pos());
        if (!t.isEmpty()) {
            QToolTip::showText(he->globalPos(), t, this);
            return true;
        }
    }
    return QWidget::event(e);
#endif
}

QRect PlayerBar::progressRect() const
{
    int cx = width() / 2;
    return QRect(cx - 130, 58, 260, 6);
}

void PlayerBar::setProgressFromX(int x)
{
    QRect pr = progressRect();
    if (!m_hasTrack || pr.width() <= 0) return;
    int rel = qBound(0, x - pr.x(), pr.width());
    m_progressMs = (int)((double)rel / pr.width() * m_durationMs);
    update();
}

/* Qt3/Qt5 painter 兼容小工具（Qt3 无 QRect::adjusted/drawPolygon(QPoint*)） */
static QRect qqAdjusted(const QRect& r, int dl, int dt, int dr, int db)
{
#ifdef QT3_BUILD
    return QRect(r.x() + dl, r.y() + dt, r.width() + (dr - dl), r.height() + (db - dt));
#else
    return r.adjusted(dl, dt, dr, db);
#endif
}

void PlayerBar::paintEvent(QPaintEvent*)
{
    const StyleParams::Palette& pal = muiPalette();
    QPainter p(this);
    p.fillRect(rect(), pal.surfaceBg);
    p.setPen(pal.border);
    p.drawLine(0, 0, width(), 0);

    QFont bold = font();
    bold.setBold(true);
    QFont small = font();
    small.setPointSize(qMax(7, small.pointSize() - 2));

    if (m_hasTrack) {
        QPixmap cover = generateIdenticon(m_track.coverSeed, 52);
        p.drawPixmap(14, 14, cover);
        p.setFont(bold);
        p.setPen(pal.textPrimary);
        p.drawText(QRect(78, 18, width() / 2 - 100, 22), Qt::AlignVCenter | Qt::AlignLeft, m_track.title);
        p.setFont(small);
        p.setPen(pal.textMuted);
        p.drawText(QRect(78, 42, width() / 2 - 100, 20), Qt::AlignVCenter | Qt::AlignLeft, m_track.artist);
    }

    QColor ctrl = (m_hover == HitPlay || m_hover == HitPrev || m_hover == HitNext
                   || m_hover == HitRepeat || m_hover == HitAutoSkip)
                      ? pal.accent : pal.textPrimary;

    QRect pv = prevRect();
    p.setPen(Qt::NoPen);
    p.setBrush(ctrl);
    p.drawRect(QRect(pv.left(), pv.top(), 3, pv.height()));
    drawTriangle(p, QRect(pv.left() + 5, pv.top(), pv.width() - 5, pv.height()), true, ctrl);

    QRect pl = playRect();
    if (m_playing) {
        p.setBrush(ctrl);
        p.drawRect(QRect(pl.left() + 5, pl.top(), 7, pl.height()));
        p.drawRect(QRect(pl.right() - 12, pl.top(), 7, pl.height()));
    } else {
        drawTriangle(p, QRect(pl.left() + 4, pl.top(), pl.width() - 4, pl.height()), false, ctrl);
    }

    QRect nx = nextRect();
    drawTriangle(p, QRect(nx.left(), nx.top(), nx.width() - 5, nx.height()), false, ctrl);
    p.setBrush(ctrl);
    p.drawRect(QRect(nx.right() - 3, nx.top(), 3, nx.height()));

    /* 失败跳过开关：圆圈 + 勾线（开=accent、关=textMuted） */
    QRect as = autoSkipRect();
    QColor asCol = (m_hover == HitAutoSkip) ? pal.accent
                   : (m_autoSkip ? pal.accent : pal.textMuted);
    p.save();
    p.setPen(QPen(asCol, 1));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(qqAdjusted(as, 1, 1, -1, -1));
    p.drawLine(as.left() + 5, as.top() + 10, as.left() + 8, as.top() + 14);
    p.drawLine(as.left() + 8, as.top() + 14, as.left() + 15, as.top() + 5);
    p.restore();

    /* 循环重复：两段圆弧 + 箭头；one 态中央画 "1"；off=textMuted */
    QRect rp = repeatRect();
    QColor rpCol = (m_hover == HitRepeat) ? pal.accent
                   : (m_repeatMode ? pal.accent : pal.textMuted);
    p.save();
    p.setPen(QPen(rpCol, 1));
    p.setBrush(Qt::NoBrush);
    if (m_repeatMode == 1) {                    /* 列表循环 */
        p.drawArc(qqAdjusted(rp, 1, 1, -1, -1), 30 * 16, 150 * 16);
        p.drawArc(qqAdjusted(rp, 1, 1, -1, -1), 210 * 16, 150 * 16);
        int axp[3] = { rp.right() - 5, rp.right() - 1, rp.right() - 3 };
        int ayp[3] = { rp.top() + 1, rp.top() + 3, rp.top() + 6 };
        int bxp[3] = { rp.left() + 5, rp.left() + 1, rp.left() + 3 };
        int byp[3] = { rp.bottom() - 1, rp.bottom() - 3, rp.bottom() - 6 };
        p.setBrush(rpCol);
#ifdef QT3_BUILD
        QPointArray ap(3);
        ap.setPoint(0, axp[0], ayp[0]);
        ap.setPoint(1, axp[1], ayp[1]);
        ap.setPoint(2, axp[2], ayp[2]);
        p.drawPolygon(ap);
        QPointArray bp(3);
        bp.setPoint(0, bxp[0], byp[0]);
        bp.setPoint(1, bxp[1], byp[1]);
        bp.setPoint(2, bxp[2], byp[2]);
        p.drawPolygon(bp);
#else
        QPoint ap[3] = { QPoint(axp[0], ayp[0]), QPoint(axp[1], ayp[1]),
                         QPoint(axp[2], ayp[2]) };
        p.drawPolygon(ap, 3);
        QPoint bp[3] = { QPoint(bxp[0], byp[0]), QPoint(bxp[1], byp[1]),
                         QPoint(bxp[2], byp[2]) };
        p.drawPolygon(bp, 3);
#endif
    } else if (m_repeatMode == 2) {             /* 单曲重复：圆 + 中央 1 */
        p.drawEllipse(qqAdjusted(rp, 1, 1, -1, -1));
        p.setFont(small);
        p.setPen(rpCol);
        p.drawText(rp, Qt::AlignCenter, QString::fromUtf8("1"));
    } else {
        p.drawEllipse(qqAdjusted(rp, 1, 1, -1, -1));
        p.setFont(small);
        p.setPen(rpCol);
        p.drawText(rp, Qt::AlignCenter, QString::fromUtf8("⟳"));
    }
    p.restore();

    QRect pr = progressRect();
    p.setPen(Qt::NoPen);
    p.setBrush(pal.border);
    muiDrawRoundRect(p, pr, 3, 3);
    int ratio = m_hasTrack && m_durationMs > 0
                    ? (int)((double)m_progressMs / m_durationMs * pr.width()) : 0;
    ratio = qBound(0, ratio, pr.width());
    if (ratio > 0) {
        p.setBrush(pal.accent);
        muiDrawRoundRect(p, QRect(pr.x(), pr.y(), ratio, pr.height()), 3, 3);
    }

    p.setFont(small);
    p.setPen(pal.textMuted);
    p.drawText(QRect(pr.x() - 44, pr.y() - 8, 40, 20), Qt::AlignVCenter | Qt::AlignRight,
               fmtMs(m_progressMs));
    p.drawText(QRect(pr.right() + 4, pr.y() - 8, 44, 20), Qt::AlignVCenter | Qt::AlignLeft,
               fmtMs(m_hasTrack ? m_durationMs : 0));
}

void PlayerBar::mousePressEvent(QMouseEvent* e)
{
    if (playRect().contains(e->pos())) {
        emit playPauseRequested();
    } else if (prevRect().contains(e->pos())) {
        emit prevRequested();
    } else if (nextRect().contains(e->pos())) {
        emit nextRequested();
    } else if (repeatRect().contains(e->pos())) {
        emit repeatClicked();
    } else if (autoSkipRect().contains(e->pos())) {
        emit autoSkipClicked();
    } else if (progressRect().contains(e->pos())) {
        m_dragging = true;
        setProgressFromX(e->pos().x());
    }
}

void PlayerBar::mouseMoveEvent(QMouseEvent* e)
{
    if (m_dragging) {
        setProgressFromX(e->pos().x());
        return;
    }
    Hit h = HitNone;
    if (playRect().contains(e->pos())) h = HitPlay;
    else if (prevRect().contains(e->pos())) h = HitPrev;
    else if (nextRect().contains(e->pos())) h = HitNext;
    else if (repeatRect().contains(e->pos())) h = HitRepeat;
    else if (autoSkipRect().contains(e->pos())) h = HitAutoSkip;
    else if (progressRect().contains(e->pos())) h = HitProgress;
    if (h != m_hover) {
        m_hover = h;
        update();
    }
}

void PlayerBar::mouseReleaseEvent(QMouseEvent*)
{
    if (m_dragging) {
        m_dragging = false;
        emit seekRequested(m_progressMs);
    }
}

void PlayerBar::leaveEvent(QEvent* e)
{
    QWidget::leaveEvent(e);
    if (m_hover != HitNone) {
        m_hover = HitNone;
        update();
    }
}
