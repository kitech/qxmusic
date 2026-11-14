#ifndef MUSICCOM_VINYLPAINT_H
#define MUSICCOM_VINYLPAINT_H

/* 唱片绘制核心（qlmusic/qskmusic 共享，Qt3/4/6 通用）：
 * 纯整数 QRect + QPainter，规避 Qt3 缺失的 QRectF/QPolygonF/QLatin1String。
 * 只做"画"，不带定时器/控件生命周期 —— 两端各自提供壳。 */

#include <qpainter.h>
#include <qpixmap.h>
#include <qrect.h>

namespace qximage {

/* 在 rect 内绘制一张唱片（居中，直径 = min(w,h) 的 86%）：
 *   盘体 → 同心沟纹 → 高光弧 → 圆形封面(按 angleDeg 旋转)
 *   → 中心标牌 → 轴孔；drawTonearm 为 true 时右上角画唱臂。 */
void paintVinyl(QPainter& p, const QRect& rect, int angleDeg,
                const QPixmap& cover, bool drawTonearm);

/* 唱片实际直径（供壳层做尺寸换算/命中测试） */
int vinylDiameter(const QRect& rect);

} // namespace qximage

#endif // MUSICCOM_VINYLPAINT_H
