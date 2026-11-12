#include "icongen.h"

#include <qpainter.h>
#include <qfont.h>
#include <qbitmap.h>
#include <qcolor.h>
#include <qrect.h>

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
#if QT_VERSION >= 0x060000
    return trimmed.left(1).toUpper();
#else
    return trimmed.left(1).upper();
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

} // namespace qximage
