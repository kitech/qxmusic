#ifndef PROGRESS_LINE_H
#define PROGRESS_LINE_H

#include <QQuickItem>
#include <QColor>

class QSGNode;

/*
 * 全宽细线播放进度（贴播放条上沿，业界 mini-player 惯例）：
 *   QQuickItem + QskPaintedNode 自绘（QSkinny 官方自绘通道）；
 *   底槽恒绘（空进度也可见，不与面板同色）；填充主题绿。
 */
class ProgressLineItem : public QQuickItem
{
public:
    explicit ProgressLineItem(QQuickItem* parent = nullptr);

    void setRatio(qreal ratio);          // 0..1
    void setColors(const QColor& track, const QColor& fill);

protected:
    QSGNode* updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*) override;

private:
    qreal m_ratio = 0.0;
    QColor m_trackColor{ 0x4a, 0x4a, 0x4a };   // 底槽：比面板亮，空态可辨
    QColor m_fillColor{ 0x4c, 0xaf, 0x50 };    // 填充：主题绿
    bool m_dirty = true;
};

#endif
