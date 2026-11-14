#ifndef VINYL_WIDGET_H
#define VINYL_WIDGET_H

#include <qwidget.h>
#include <qpixmap.h>
#include <qtimer.h>
#include <qdatetime.h>

/*
 * 播放唱片控件（QWidget 版；仅编译，暂不接入界面）。
 * - 33⅓ RPM 默认转速；暂停即停转（保留角度），stop() 复位 0°
 * - 按真实 elapsed 累加角度，掉帧不会"加速"
 * - 不可见/隐藏时停表省电；showEvent 恢复
 */
class VinylWidget : public QWidget {
    Q_OBJECT
public:
    explicit VinylWidget(QWidget* parent = 0);

    void setCover(const QPixmap& cover);   // 封面（缺图时画标牌色圆）
    void setPlaying(bool playing);         // 起停旋转
    void stop();                           // 停转并复位到 0°
    void setRpm(int rpm);                  // 默认 33（33⅓）
    void setShowTonearm(bool on);

    int angle() const { return m_angle; }

protected:
    void paintEvent(QPaintEvent* e);
    void showEvent(QShowEvent* e);
    void hideEvent(QHideEvent* e);

private:
    QTimer m_timer;
    QTime m_clock;
    int m_intervalMs;
    QPixmap m_cover;
    double m_angle;
    double m_rpm;
    bool m_playing;
    bool m_showTonearm;
};

#endif // VINYL_WIDGET_H
