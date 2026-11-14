#include "vinylpaint.h"

#include <qcolor.h>
#include <qregion.h>
#include <qpoint.h>

namespace qximage {

namespace {

/* Qt3 无 qMin：整数三目 */
inline int qxMin(int a, int b) { return a < b ? a : b; }

QColor discColor()     { return QColor(0x1c, 0x1c, 0x20); }
QColor grooveColor()   { return QColor(0x2e, 0x2e, 0x34); }
QColor labelColor()    { return QColor(0xc9, 0x8f, 0x8a); }
QColor highlightColor(){ return QColor(0xfb, 0xff, 0xfe); }
QColor armColor()      { return QColor(0xc9, 0xb2, 0x7a); }

} // namespace

int vinylDiameter(const QRect& rect)
{
    return qxMin(rect.width(), rect.height()) * 86 / 100;
}

void paintVinyl(QPainter& p, const QRect& rect, int angleDeg,
                const QPixmap& cover, bool drawTonearm)
{
    const int d = vinylDiameter(rect);
    const QPoint c = rect.center();
    const QRect disc(c.x() - d / 2, c.y() - d / 2, d, d);
    const int radius = d / 2;

#if QT_VERSION >= 0x040000
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
#endif

    /* 盘体 */
    p.setPen(Qt::NoPen);
    p.setBrush(discColor());
    p.drawEllipse(disc);

    /* 同心沟纹（3 圈细线） */
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(grooveColor(), 1));
    for (int i = 1; i <= 3; ++i) {
        const int inset = radius * i / 5;
        p.drawEllipse(QRect(disc.left() + inset, disc.top() + inset,
                            disc.width() - inset * 2, disc.height() - inset * 2));
    }

    /* 高光弧（左上，固定不随封面旋转） */
    {
        QPen hp(highlightColor(), qxMin(3, d / 60));
        p.setPen(hp);
        p.setBrush(Qt::NoBrush);
        const int inset = radius / 8;
        p.drawArc(QRect(disc.left() + inset, disc.top() + inset,
                        disc.width() - inset * 2, disc.height() - inset * 2),
                  110 * 16, 55 * 16);
    }

    /* 圆形封面（按角度旋转；缺图则画标牌色圆） */
    {
        const int cd = d * 58 / 100;
        const QRect coverRect(c.x() - cd / 2, c.y() - cd / 2, cd, cd);
        p.save();
        QRegion clip(coverRect, QRegion::Ellipse);
        p.setClipRegion(clip);
        p.translate(c.x(), c.y());
        p.rotate(angleDeg % 360);
        p.translate(-c.x(), -c.y());
        if (!cover.isNull()) {
            p.drawPixmap(coverRect, cover);
        } else {
            p.setPen(Qt::NoPen);
            p.setBrush(labelColor());
            p.drawEllipse(coverRect);
        }
        p.restore();
    }

    /* 中心标牌 + 轴孔 */
    {
        const int ld = d * 16 / 100;
        p.setPen(Qt::NoPen);
        p.setBrush(labelColor());
        p.drawEllipse(QRect(c.x() - ld / 2, c.y() - ld / 2, ld, ld));
        const int hd = qxMin(6, d / 26);
        p.setBrush(discColor());
        p.drawEllipse(QRect(c.x() - hd / 2, c.y() - hd / 2, hd, hd));
    }

    /* 唱臂（右上枢轴 → 盘缘） */
    if (drawTonearm) {
        const QPoint pivot(disc.right() - d / 12, disc.top() + d / 12);
        const QPoint tip(disc.right() - d / 5, disc.top() + d / 3);
        p.setPen(QPen(armColor(), qxMin(6, d / 26)));
        p.drawLine(pivot, tip);
        p.setPen(Qt::NoPen);
        p.setBrush(armColor());
        const int pd = qxMin(14, d / 14);
        p.drawEllipse(QRect(pivot.x() - pd / 2, pivot.y() - pd / 2, pd, pd));
    }
}

} // namespace qximage
