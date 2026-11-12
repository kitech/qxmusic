#ifndef ICON_ITEM_H
#define ICON_ITEM_H

#include <QskControl.h>
#include <QPixmap>
#include <QString>

class IconPaintedItem;

/*
 * 固定尺寸图标容器（封面/兜底）：
 *   QskControl 负责布局尺寸（setPreferredSize 等），内部 QQuickItem 自绘
 *   （QskControl::updatePaintNode 为 final，不能直接重写）。
 * 不改行高、不参与宽度自适应；无图时画 fallback（identicon/首字母）。
 */
class IconItem : public QskControl
{
    Q_OBJECT
public:
    enum Shape { Rounded, Circle };

    explicit IconItem(QQuickItem* parent = nullptr);

    void setSource(const QString& url, int px);
    void setFallback(const QPixmap& pixmap);
    void setShape(Shape shape);
    void setCornerRadius(qreal radius);

private:
    IconPaintedItem* m_paint = nullptr;
};

#endif // ICON_ITEM_H
