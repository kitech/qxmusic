#include "vinylitem.h"
#include "vinylpaint.h"

#include <QPainter>
#include <QQuickWindow>
#include <QSGSimpleTextureNode>
#include <QSGTransformNode>
#include <QSGTexture>
#include <QMatrix4x4>
#include <QTransform>

VinylItem::VinylItem(QQuickItem* parent)
    : QQuickItem(parent)
{
    setFlag(QQuickItem::ItemHasContents, true);
    m_timer.setInterval(33);   // ≈30fps
    connect(&m_timer, &QTimer::timeout, this, [this]() {
        const qint64 ms = m_clock.restart();
        setAngle(m_angle + m_rpm * 360.0 * ms / 60000.0);
    });
}

void VinylItem::setCover(const QPixmap& cover)
{
    m_cover = cover;
    m_dirty = true;
    update();
}

void VinylItem::setAngle(qreal a)
{
    while (a >= 360.0) a -= 360.0;
    while (a < 0.0) a += 360.0;
    if (qFuzzyCompare(a + 1.0, m_angle + 1.0))
        return;
    m_angle = a;
    Q_EMIT angleChanged();
    update();
}

void VinylItem::setPlaying(bool playing)
{
    if (m_playing == playing)
        return;
    m_playing = playing;
    if (playing && isVisible()) {
        m_clock.restart();
        m_timer.start();
    } else {
        m_timer.stop();
    }
    Q_EMIT playingChanged();
}

void VinylItem::itemChange(ItemChange change, const ItemChangeData& value)
{
    QQuickItem::itemChange(change, value);
    if (change == ItemVisibleHasChanged) {
        if (m_playing && value.boolValue) {
            m_clock.restart();
            m_timer.start();
        } else if (!value.boolValue) {
            m_timer.stop();
        }
    }
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    else if (change == ItemDevicePixelRatioHasChanged) {
        m_dirty = true;
        update();
    }
#endif
}

void VinylItem::regenerateImage()
{
    const int d = qMax(1, int(qMin(width(), height())));
    const qreal dpr = window() ? window()->effectiveDevicePixelRatio() : 1.0;
    const int px = qMax(1, int(d * dpr));
    m_image = QImage(px, px, QImage::Format_ARGB32_Premultiplied);
    m_image.fill(Qt::transparent);
    QPainter p(&m_image);
    p.scale(dpr, dpr);
    qximage::paintVinyl(p, QRect(0, 0, d, d), 0, m_cover, true);
    p.end();
    m_dirty = false;
}

QSGNode* VinylItem::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*)
{
    /* 结构：QSGTransformNode（旋转矩阵）→ QSGSimpleTextureNode（预渲染纹理）
     * QSGSimpleTextureNode 无 setMatrix，必须用父节点做变换 */
    QSGTransformNode* root = static_cast<QSGTransformNode*>(oldNode);
    QSGSimpleTextureNode* node = nullptr;
    if (!root) {
        root = new QSGTransformNode();
        node = new QSGSimpleTextureNode();
        root->appendChildNode(node);
    } else {
        node = static_cast<QSGSimpleTextureNode*>(root->firstChild());
        if (!node) {
            node = new QSGSimpleTextureNode();
            root->appendChildNode(node);
        }
    }

    if (m_dirty || !m_texture) {
        regenerateImage();
        delete m_texture;
        m_texture = window()
            ? window()->createTextureFromImage(
                  m_image, QQuickWindow::TextureHasAlphaChannel)
            : nullptr;
        node->setTexture(m_texture);
        node->setOwnsTexture(false);   // 纹理由本类管理（dirty 时手动重建）
    }

    node->setRect(0, 0, width(), height());
    node->setFiltering(QSGTexture::Linear);

    /* 旋转：仅改变换矩阵，纹理不重画 */
    const QPointF c(width() / 2.0, height() / 2.0);
    QTransform t;
    t.translate(c.x(), c.y());
    t.rotate(m_angle);
    t.translate(-c.x(), -c.y());
    root->setMatrix(QMatrix4x4(t));

    return root;
}
