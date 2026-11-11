#include "progressline.h"
#include <QskPaintedNode.h>
#include <QPainter>
#include <QQuickWindow>
#include <QSGNode>

namespace {

// QskPaintedNode 机制（本机头文件核对）：
//   刷新 = protected update(window, rect, size, nodeData)（无 triggerUpdate，
//   anystik 的 triggerUpdate 是自定义包装）；hash()=0 强制重绘，否则复用纹理。
class ProgressLineNode : public QskPaintedNode
{
public:
    void setData(qreal ratio, const QColor& track, const QColor& fill)
    {
        m_ratio = ratio;
        m_track = track;
        m_fill = fill;
    }

    void refresh(QQuickWindow* window, const QRectF& rect, const QSizeF& size)
    {
        update(window, rect, size, nullptr);
    }

    void paint(QPainter* painter, const QSize& size, const void*) override
    {
        const qreal w = size.width();
        const qreal h = size.height();
        painter->setRenderHint(QPainter::Antialiasing, true);

        // 底槽：恒绘（空进度时也与背景可区分）
        painter->fillRect(QRectF(0, 0, w, h), m_track);

        const qreal filled = qBound<qreal>(0.0, m_ratio, 1.0) * w;
        if (filled > 0)
            painter->fillRect(QRectF(0, 0, filled, h), m_fill);
    }

    QskHashValue hash(const void*) const override
    {
        const quint32 r = quint32(qBound<qreal>(0.0, m_ratio, 1.0) * 10000);
        return r ^ (quint32(m_track.rgb()) << 8)
                 ^ (quint32(m_fill.rgb()) << 16);
    }

private:
    qreal m_ratio = 0.0;
    QColor m_track;
    QColor m_fill;
};

} // namespace

ProgressLineItem::ProgressLineItem(QQuickItem* parent)
    : QQuickItem(parent)
{
    setFlag(QQuickItem::ItemHasContents, true);
    setAcceptedMouseButtons(Qt::NoButton);
}

void ProgressLineItem::setRatio(qreal ratio)
{
    const qreal r = qBound<qreal>(0.0, ratio, 1.0);
    if (qFuzzyCompare(r, m_ratio))
        return;
    m_ratio = r;
    m_dirty = true;
    update();
}

void ProgressLineItem::setColors(const QColor& track, const QColor& fill)
{
    m_trackColor = track;
    m_fillColor = fill;
    m_dirty = true;
    update();
}

QSGNode* ProgressLineItem::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*)
{
    auto* node = static_cast<ProgressLineNode*>(oldNode);
    if (!node)
        node = new ProgressLineNode();

    if (m_dirty) {
        node->setData(m_ratio, m_trackColor, m_fillColor);
        m_dirty = false;
    }
    node->refresh(window(),
        QRectF(QPointF(0, 0), QSizeF(width(), height())),
        QSizeF(width(), height()));
    return node;
}
