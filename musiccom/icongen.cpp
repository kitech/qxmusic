#include "icongen.h"

#include <qpainter.h>
#include <qfont.h>
#include <qbitmap.h>
#include <qcolor.h>
#include <qrect.h>
#if QT_VERSION < 0x040000
#include <qpointarray.h>
#else
#include <qpolygon.h>
#endif

#include <cmath>

namespace qximage {
namespace {

QString qxTrimmed(const QString& s)
{
#if QT_VERSION < 0x040000
    return s.stripWhiteSpace();
#else
    return s.trimmed();
#endif
}

unsigned int fnv1a(const QString& seed)
{
    const QByteArray bytes =
#ifdef QT3_BUILD
        seed.utf8();
#else
        seed.toUtf8();
#endif
    unsigned int h = 2166136261u;
    for (int i = 0; i < bytes.size(); ++i) {
        h ^= (unsigned char)bytes.at(i);
        h *= 16777619u;
    }
    return h;
}

void hueToRgb(int hue, int sat, int val, int& r, int& g, int& b)
{
    hue = hue % 360;
    const int c = (val * sat) / 255;
    const int hp = (hue / 60) % 6;
    const int x = c * (60 - (hue % 60)) / 60;
    int r1 = 0, g1 = 0, b1 = 0;
    switch (hp) {
    case 0: r1 = c; g1 = x; break;
    case 1: r1 = x; g1 = c; break;
    case 2: g1 = c; b1 = x; break;
    case 3: g1 = x; b1 = c; break;
    case 4: r1 = x; b1 = c; break;
    default: r1 = c; b1 = x; break;
    }
    const int m = val - c;
    r = r1 + m;
    g = g1 + m;
    b = b1 + m;
}

QColor colorForHash(unsigned int h)
{
    const int hue = 150 + (int)(h % 120);              // 青—蓝—紫区间
    const int sat = 120 + (int)((h >> 8) % 80);
    const int val = 150 + (int)((h >> 16) % 70);
    int r, g, b;
    hueToRgb(hue, sat, val, r, g, b);
    return QColor(r, g, b);
}

QString firstGlyph(const QString& name)
{
    const QString trimmed = qxTrimmed(name);
    if (trimmed.isEmpty())
        return QString::fromLatin1("?");
#if QT_VERSION < 0x040000
    return trimmed.left(1).upper();     // Qt3
#else
    return trimmed.left(1).toUpper();   // Qt4+
#endif
}

void applyCircleMask(QPixmap& pm, int size)
{
    QBitmap mask(size, size);
    mask.fill(Qt::color0);
    QPainter mp(&mask);
    mp.setBrush(Qt::color1);
    mp.setPen(Qt::NoPen);
    mp.drawEllipse(0, 0, size, size);
    mp.end();
    pm.setMask(mask);
}

} // namespace

QPixmap identicon(const QString& seed, int size)
{
    const unsigned int h = fnv1a(seed);
    const QColor fg = colorForHash(h);

    QPixmap pm(size, size);
    pm.fill(Qt::white);

    // 5×5 左右镜像图案，位来自哈希
    bool cells[5][3];
    for (int y = 0; y < 5; ++y) {
        for (int x = 0; x < 3; ++x) {
            const int idx = y * 3 + x;
            cells[y][x] = (((h >> (idx % 29)) ^ (h >> ((idx + 7) % 29))) & 1) != 0;
        }
    }

    QPainter p(&pm);
    const int cell = size / 5;
    const int rem = size % 5;
    for (int y = 0; y < 5; ++y) {
        for (int x = 0; x < 5; ++x) {
            const int sx = (x < 3) ? x : (4 - x);
            if (!cells[y][sx])
                continue;
            const int px = x * cell + (x < rem ? x : rem);
            const int py = y * cell + (y < rem ? y : rem);
            const int w = cell + (x < rem ? 1 : 0);
            const int hh = cell + (y < rem ? 1 : 0);
            p.fillRect(px, py, w, hh, fg);
        }
    }
    p.end();

    applyCircleMask(pm, size);
    return pm;
}

QPixmap letterAvatar(const QString& name, int size, bool circular)
{
    const unsigned int h = fnv1a(name);
    const QColor bg = colorForHash(h);

    QPixmap pm(size, size);
    pm.fill(bg);

    QPainter p(&pm);
    QFont f = p.font();
    f.setBold(true);
    f.setPixelSize(size * 48 / 100);
    p.setFont(f);
    p.setPen(Qt::white);
    p.drawText(QRect(0, 0, size, size), Qt::AlignCenter, firstGlyph(name));
    p.end();

    if (circular)
        applyCircleMask(pm, size);
    return pm;
}

QPixmap categoryIcon(const QString& name, int size)
{
    const unsigned int h = fnv1a(name);
    const QColor bg = colorForHash(h);
    const int radius = size / 4;

    QPixmap pm(size, size);
    pm.fill(Qt::white);

    QPainter p(&pm);
    p.setPen(Qt::NoPen);
    p.setBrush(bg);
#if QT_VERSION >= 0x060000
    p.drawRoundedRect(QRect(0, 0, size, size), radius, radius);
#else
    p.drawRoundRect(QRect(0, 0, size, size), radius, radius);
#endif
    QFont f = p.font();
    f.setBold(true);
    f.setPixelSize(size * 46 / 100);
    p.setFont(f);
    p.setPen(Qt::white);
    p.drawText(QRect(0, 0, size, size), Qt::AlignCenter, firstGlyph(name));
    p.end();
    return pm;
}

namespace {

/* Qt3 无 QRectF/QPolygonF/QLatin1String/qMin：全部用整数 QRect + QPolygon */
#if QT_VERSION < 0x040000
typedef QPointArray NavPoly;
#else
typedef QPolygon NavPoly;
#endif

QColor navColor(const QString& kind)
{
    if (kind == QString("recommend")) return QColor(0xe8, 0xa3, 0x3d);
    if (kind == QString("rank"))      return QColor(0x5b, 0x7c, 0xfa);
    if (kind == QString("mymusic"))   return QColor(0x3a, 0xa9, 0x81);
    if (kind == QString("local"))     return QColor(0x6b, 0x7a, 0x90);
    return QColor(0xe2, 0x68, 0x3c);            // hot（默认火焰）
}

/* 五角星（推荐） */
void drawStar(QPainter& p, const QRect& r)
{
    NavPoly poly;
    const QPoint c = r.center();
    const int side = r.width() < r.height() ? r.width() : r.height();
    const double R = side * 0.46;
    const double r2 = R * 0.45;
    for (int i = 0; i < 10; ++i) {
        const double a = -1.5707963 + i * 0.6283185;   // -90° 起步，每 36°
        const double rad = (i % 2 == 0) ? R : r2;
        poly.putPoints(i, 1, c.x() + (int)(rad * std::cos(a)),
                       c.y() + (int)(rad * std::sin(a)));
    }
    p.drawPolygon(poly);
}

/* 火焰（热门歌单） */
void drawFlame(QPainter& p, const QRect& r)
{
    const int w = r.width(), h = r.height();
    const int x = r.left(), y = r.top();
    NavPoly outer;
    const int pts[6][2] = {
        { x + w * 50 / 100, y + h * 10 / 100 },
        { x + w * 78 / 100, y + h * 45 / 100 },
        { x + w * 72 / 100, y + h * 80 / 100 },
        { x + w * 50 / 100, y + h * 92 / 100 },
        { x + w * 28 / 100, y + h * 80 / 100 },
        { x + w * 22 / 100, y + h * 48 / 100 },
    };
    for (int i = 0; i < 6; ++i)
        outer.putPoints(i, 1, pts[i][0], pts[i][1]);
    p.drawPolygon(outer);
}

/* 柱状条（排行榜） */
void drawBars(QPainter& p, const QRect& r)
{
    const int w = r.width(), h = r.height();
    const int bw = w * 18 / 100;
    const int gap = w * 12 / 100;
    const int base = r.top() + h * 86 / 100;
    const int hs[3] = { h * 38 / 100, h * 62 / 100, h * 50 / 100 };
    int x = r.left() + w * 16 / 100;
    for (int i = 0; i < 3; ++i) {
        p.drawRect(QRect(x, base - hs[i], bw, hs[i]));
        x += bw + gap;
    }
}

/* 人形（我的音乐） */
void drawPerson(QPainter& p, const QRect& r)
{
    const int w = r.width(), h = r.height();
    const int head = w * 30 / 100;
    p.drawEllipse(QRect(r.center().x() - head / 2, r.top() + h * 16 / 100,
                        head, head));
    p.drawChord(QRect(r.left() + w * 18 / 100, r.top() + h * 50 / 100,
                      w * 64 / 100, h * 52 / 100), 0, 180 * 16);
}

/* 文件夹 + 音符（本地音乐） */
void drawFolder(QPainter& p, const QRect& r)
{
    const int w = r.width(), h = r.height();
    const int x = r.left(), y = r.top();
    p.drawRect(QRect(x + w * 14 / 100, y + h * 40 / 100,
                     w * 44 / 100, h * 10 / 100));   // 文件夹页签
    p.drawRect(QRect(x + w * 14 / 100, y + h * 46 / 100,
                     w * 72 / 100, h * 38 / 100));   // 文件夹主体
    p.drawEllipse(QRect(x + w * 60 / 100, y + h * 30 / 100,
                        w * 16 / 100, h * 13 / 100));  // 音符头
    p.drawRect(QRect(x + w * 72 / 100, y + h * 16 / 100,
                     w * 6 / 100, h * 20 / 100));    // 音符杆
}

/* 电台（网络电台 / 站点电台）：中心圆点 + 两道波纹弧 */
void drawBroadcast(QPainter& p, const QRect& r)
{
    const int w = r.width(), h = r.height();
    const int x = r.left(), y = r.top();
    const int dot = w / 7;
    p.drawEllipse(QRect(x + w / 2 - dot / 2, y + h / 2 - dot / 2, dot, dot));
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(Qt::white, (w / 12 < 3) ? w / 12 : 3));
    p.drawArc(QRect(x + w / 4, y + h / 4, w / 2, h / 2), 45 * 16, 90 * 16);
    p.drawArc(QRect(x + w / 8, y + h / 8, w * 3 / 4, h * 3 / 4), 45 * 16, 90 * 16);
}

} // namespace

QPixmap navIcon(const QString& kind, int size)
{
    QPixmap pm(size, size);
    const int radius = size / 4;
#if QT_VERSION < 0x040000
    /* Qt3 无 Qt::transparent 填充：用位图遮罩裁圆角 */
    pm.fill(Qt::white);
    QBitmap mask(size, size);
    mask.fill(Qt::color0);
    {
        QPainter mp(&mask);
        mp.setPen(Qt::NoPen);
        mp.setBrush(Qt::color1);
        mp.drawRoundRect(QRect(0, 0, size, size), radius, radius);
    }
    pm.setMask(mask);
#else
    pm.fill(Qt::transparent);
#endif
    QPainter p(&pm);
#if QT_VERSION >= 0x040000
    p.setRenderHint(QPainter::Antialiasing, true);
#endif
    p.setPen(Qt::NoPen);
    p.setBrush(navColor(kind));
#if QT_VERSION >= 0x060000
    p.drawRoundedRect(QRect(0, 0, size, size), radius, radius);
#else
    p.drawRoundRect(QRect(0, 0, size, size), radius, radius);
#endif
    p.setBrush(Qt::white);
    const QRect inner(size * 18 / 100, size * 18 / 100,
                      size * 64 / 100, size * 64 / 100);
    if (kind == QString("recommend"))      drawStar(p, inner);
    else if (kind == QString("rank"))      drawBars(p, inner);
    else if (kind == QString("mymusic"))   drawPerson(p, inner);
    else if (kind == QString("local"))     drawFolder(p, inner);
    else if (kind == QString("radio"))     drawBroadcast(p, inner);
    else                                   drawFlame(p, inner);
    p.end();
    return pm;
}

} // namespace qximage
