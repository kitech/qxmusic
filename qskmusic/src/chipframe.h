#ifndef CHIP_FRAME_H
#define CHIP_FRAME_H

#include <QskControl.h>
#include <QColor>

class ChipPaintedItem;

/*
 * 分类 chip 的视觉包裹层（纯装饰，无任何状态/交互逻辑）：
 *   细弱 accent 圆角描边（对号由 chip 内联文本子项承担）。
 * 作为 chip（QskLinearBox）子项时必须 layoutPolicy=Ignore，否则会被
 * autoAddChildren 吸进布局；尺寸由调用方跟随 chip 宽高同步。
 */
class ChipFrameItem : public QskControl
{
    Q_OBJECT
public:
    explicit ChipFrameItem(QQuickItem* parent = nullptr);

    void setAccent(const QColor& color);   // 描边与对号颜色

private:
    ChipPaintedItem* m_paint = nullptr;
};

#endif // CHIP_FRAME_H
