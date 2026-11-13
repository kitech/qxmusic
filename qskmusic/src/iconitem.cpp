#include "iconitem.h"
#include "iconloader.h"

#include <QskPaintedNode.h>
#include <QskSizePolicy.h>
#include <QGuiApplication>
#include <QScreen>
#include <QPainter>
#include <QPainterPath>
#include <QQuickWindow>
#include <QSGNode>
#include <QColor>

namespace {

class IconNode : public QskPaintedNode
{
public:
    void setData(const QPixmap& pixmap, int shape, qreal radius)
    {
        m_pixmap = pixmap;
        m_shape = shape;
        m_radius = radius;
    }

    void refresh(QQuickWindow* window, const QRectF& rect, const QSizeF& size)
    {
        update(window, rect, size, nullptr);
    }

    void paint(QPainter* painter, const QSize& size, const void*) override
    {
        const QRectF rect(0, 0, size.width(), size.height());
        painter->setRenderHint(QPainter::Antialiasing, true);

        QPainterPath path;
        if (m_shape == 1)
            path.addEllipse(rect);
        else
            path.addRoundedRect(rect, m_radius, m_radius);
        painter->setClipPath(path);

        painter->fillRect(rect, QColor(0x2a, 0x2a, 0x2a));

        if (!m_pixmap.isNull()) {
            const QPixmap scaled = m_pixmap.scaled(
                size, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
            const int x = (scaled.width() - size.width()) / 2;
            const int y = (scaled.height() - size.height()) / 2;
            painter->drawPixmap(QPoint(0, 0), scaled,
                                QRect(x, y, size.width(), size.height()));
        }
    }

    QskHashValue hash(const void*) const override
    {
        return QskHashValue(m_pixmap.cacheKey())
            ^ QskHashValue(m_shape) ^ QskHashValue(int(m_radius * 10));
    }

private:
    QPixmap m_pixmap;
    int m_shape = 0;
    qreal m_radius = 8;
};

} // namespace

/* ── 实际绘制项（QQuickItem + QskPaintedNode）── */
class IconPaintedItem : public QQuickItem
{
public:
    explicit IconPaintedItem(QQuickItem* parent = nullptr)
        : QQuickItem(parent)
    {
        setFlag(QQuickItem::ItemHasContents, true);
        setAcceptedMouseButtons(Qt::NoButton);
    }

    void setSource(const QString& url, int px)
    {
        if (!m_subscribed) {
            QObject::connect(&IconLoader::instance(), &IconLoader::iconReady,
                             this, [this](const QString& u, int p) {
                                 onIconReady(u, p);
                             });
            m_subscribed = true;
        }

        if (m_url == url && m_px == px)
            return;

        m_url = url;
        m_px = px;
        /* lazy：这里只查内存/磁盘缓存；网络请求推迟到首帧绘制
         * （updatePaintNode），离屏行永远不会发起下载 */
        m_pixmap = url.isEmpty() ? QPixmap()
                                 : IconLoader::instance().cached(url, px);
        if (!m_pixmap.isNull() || url.isEmpty())
            m_requested = true;      // 已有图/无 URL：不再懒请求

        m_dirty = true;
        update();
    }

    void setFallback(const QPixmap& pixmap)
    {
        m_fallback = pixmap;
        m_dirty = true;
        update();
    }

    void setShape(int shape)
    {
        if (m_shape == shape)
            return;
        m_shape = shape;
        m_dirty = true;
        update();
    }

    void setRadius(qreal radius)
    {
        if (qFuzzyCompare(m_radius, radius))
            return;
        m_radius = radius;
        m_dirty = true;
        update();
    }

protected:
    QSGNode* updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*) override
    {
        auto* node = static_cast<IconNode*>(oldNode);
        if (!node)
            node = new IconNode();

        /* 需要绘制 = 此刻真的要画这个 item（scene graph 只对可视 item 调
         * updatePaintNode，离屏行自然被跳过）→ 首次绘制才发起网络请求 */
        if (m_pixmap.isNull() && !m_url.isEmpty() && !m_requested) {
            m_requested = true;
            IconLoader::instance().request(m_url, m_px);
        }

        const QPixmap& shown = m_pixmap.isNull() ? m_fallback : m_pixmap;
        if (m_dirty) {
            node->setData(shown, m_shape, m_radius);
            m_dirty = false;
        }
        node->refresh(window(),
            QRectF(QPointF(0, 0), QSizeF(width(), height())),
            QSizeF(width(), height()));
        return node;
    }

private:
    void onIconReady(const QString& url, int px)
    {
        if (url != m_url || px != m_px)
            return;
        m_pixmap = IconLoader::instance().cached(url, px);
        m_dirty = true;
        update();
    }

    QString m_url;
    int m_px = 0;
    QPixmap m_pixmap;
    QPixmap m_fallback;
    int m_shape = 0;
    qreal m_radius = 8;
    bool m_dirty = true;
    bool m_subscribed = false;
    bool m_requested = false;      // 懒请求已发出（一次性）
};

/* ── 容器：负责首选尺寸，内嵌绘制项填满自身 ── */
IconItem::IconItem(QQuickItem* parent)
    : QskControl(parent)
{
    // 固定尺寸：不参与布局伸缩（行高/宽度稳定，安卓不留重排风险）
    setSizePolicy(QskSizePolicy::Fixed, QskSizePolicy::Fixed);

    m_paint = new IconPaintedItem(this);
    m_paint->setSize(QSizeF(width(), height()));

    QObject::connect(this, &QQuickItem::widthChanged, this, [this]() {
        m_paint->setSize(QSizeF(width(), height()));
    });
    QObject::connect(this, &QQuickItem::heightChanged, this, [this]() {
        m_paint->setSize(QSizeF(width(), height()));
    });
}

int IconItem::pxForDp(int dp)
{
    qreal dpr = 1.0;
    if (QGuiApplication::primaryScreen())
        dpr = QGuiApplication::primaryScreen()->devicePixelRatio();
    return qBound(1, qRound(dp * dpr), 256);
}

void IconItem::setSource(const QString& url, int dp)
{
    m_paint->setSource(url, pxForDp(dp));
}

void IconItem::setFallback(const QPixmap& pixmap)
{
    m_paint->setFallback(pixmap);
}

void IconItem::setShape(Shape shape)
{
    m_paint->setShape(int(shape));
}

void IconItem::setCornerRadius(qreal radius)
{
    m_paint->setRadius(radius);
}
