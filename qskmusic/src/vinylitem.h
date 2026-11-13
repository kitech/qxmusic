#ifndef VINYL_ITEM_H
#define VINYL_ITEM_H

#include <QQuickItem>
#include <QPixmap>
#include <QImage>
#include <QTimer>
#include <QElapsedTimer>

class QSGTexture;

/*
 * 播放唱片（QSkinny/Qt Quick 版；仅编译，暂不接入页面）。
 * 性能要点：光盘只预渲染一次成纹理，旋转只改节点矩阵，
 * 每帧不重画（与 QSkinny QskPaintedNode 思路一致）。
 */
class VinylItem : public QQuickItem
{
    Q_OBJECT
    Q_PROPERTY(qreal angle READ angle WRITE setAngle NOTIFY angleChanged)
    Q_PROPERTY(bool playing READ isPlaying WRITE setPlaying NOTIFY playingChanged)
    Q_PROPERTY(int rpm READ rpm WRITE setRpm)

public:
    explicit VinylItem(QQuickItem* parent = nullptr);

    void setCover(const QPixmap& cover);

    qreal angle() const { return m_angle; }
    void setAngle(qreal a);

    bool isPlaying() const { return m_playing; }
    void setPlaying(bool playing);

    int rpm() const { return m_rpm; }
    void setRpm(int rpm) { m_rpm = rpm > 0 ? rpm : 33; }

Q_SIGNALS:
    void angleChanged();
    void playingChanged();

protected:
    QSGNode* updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*) override;
    void itemChange(ItemChange change, const ItemChangeData& value) override;

private:
    void regenerateImage();

    QTimer m_timer;
    QElapsedTimer m_clock;
    QPixmap m_cover;
    QImage m_image;          // 预渲染光盘（角度 0）
    QSGTexture* m_texture = nullptr;
    qreal m_angle = 0;
    int m_rpm = 33;
    bool m_playing = false;
    bool m_dirty = true;
};

#endif // VINYL_ITEM_H
