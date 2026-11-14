#include "chipframe.h"

#include <QskPaintedNode.h>
#include <QskPlacementPolicy.h>
#include <QPainter>
#include <QQuickWindow>
#include <QSGNode>

namespace {

/* 圆角描边 + 右上角对号（QPainter 通道，与 ProgressLine/IconItem 同款） */
class ChipNode : public QskPaintedNode
{
public:
    void setData(const QColor& accent)
    {
        m_accent = accent;
    }

    void refresh(QQuickWindow* window, const QRectF& rect, const QSizeF& size)
    {
        update(window, rect, size, nullptr);
    }

    void paint(QPainter* painter, const QSize& size, const void*) override
    {
        const qreal w = size.width();
        const qreal h = size.height();
        if (w <= 1 || h <= 1)
            return;

        painter->setRenderHint(QPainter::Antialiasing, true);

        /* 容器描边（内缩 0.5px，避免线宽被裁掉一半） */
        /* 细弱描边：accent 约 43% 透明，1px 圆角 10 */
        QColor stroke = m_accent;
        stroke.setAlpha(110);
        QPen pen(stroke);
        pen.setWidthF(1.0);
        painter->setPen(pen);
        painter->setBrush(Qt::NoBrush);
        painter->drawRoundedRect(QRectF(0.5, 0.5, w - 1.0, h - 1.0), 10, 10);
    }

    QskHashValue hash(const void*) const override
    {
        return QskHashValue(m_accent.rgb()) ^ QskHashValue(0x9e37);
    }

private:
    QColor m_accent{ 0x3d, 0x8b, 0xfd };
};

} // namespace

/* ── 实际绘制项（QQuickItem + QskPaintedNode）── */
class ChipPaintedItem : public QQuickItem
{
public:
    explicit ChipPaintedItem(QQuickItem* parent = nullptr)
        : QQuickItem(parent)
    {
        setFlag(QQuickItem::ItemHasContents, true);
        setAcceptedMouseButtons(Qt::NoButton);
    }

    void setAccent(const QColor& color)
    {
        if (m_accent == color)
            return;
        m_accent = color;
        m_dirty = true;
        update();
    }

protected:
    QSGNode* updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*) override
    {
        auto* node = static_cast<ChipNode*>(oldNode);
        if (!node)
            node = new ChipNode();

        static_cast<ChipNode*>(node)->setData(m_accent);
        m_dirty = false;
        node->refresh(window(), QRectF(0, 0, width(), height()),
                      QSizeF(width(), height()));
        return node;
    }

private:
    QColor m_accent{ 0x3d, 0x8b, 0xfd };
    bool m_dirty = true;
};

/* ── 容器：不进布局，内嵌绘制项填满自身 ── */
ChipFrameItem::ChipFrameItem(QQuickItem* parent)
    : QskControl(parent)
{
    /* chip 是 QskLinearBox（autoAddChildren）：必须 Ignore，否则被吸进布局 */
    setPlacementPolicy(QskPlacementPolicy::Ignore, QskPlacementPolicy::Ignore);
    setAcceptedMouseButtons(Qt::NoButton);

    m_paint = new ChipPaintedItem(this);
    m_paint->setSize(QSizeF(width(), height()));

    QObject::connect(this, &QQuickItem::widthChanged, this, [this]() {
        m_paint->setSize(QSizeF(width(), height()));
    });
    QObject::connect(this, &QQuickItem::heightChanged, this, [this]() {
        m_paint->setSize(QSizeF(width(), height()));
    });
}

void ChipFrameItem::setAccent(const QColor& color)
{
    m_paint->setAccent(color);
}
