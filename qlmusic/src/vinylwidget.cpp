#include "vinylwidget.h"
#include "vinylpaint.h"

#include <qpainter.h>

VinylWidget::VinylWidget(QWidget* parent)
    : QWidget(parent)
    , m_intervalMs(33)
    , m_angle(0)
    , m_rpm(33)
    , m_playing(false)
    , m_showTonearm(true)
{
    setMinimumSize(120, 120);
    connect(&m_timer, SIGNAL(timeout()), this, SLOT(update()));
#if QT_VERSION < 0x040000
    m_timer.changeInterval(m_intervalMs);   // Qt3：无 setInterval
#else
    m_timer.setInterval(m_intervalMs);      // ≈30fps，够顺滑且省电
#endif
}

void VinylWidget::setCover(const QPixmap& cover)
{
    m_cover = cover;
    update();
}

void VinylWidget::setPlaying(bool playing)
{
    if (m_playing == playing)
        return;
    m_playing = playing;
    if (playing && isVisible()) {
        m_clock.restart();
        m_timer.start(m_intervalMs);
    } else {
        m_timer.stop();
    }
}

void VinylWidget::stop()
{
    m_timer.stop();
    m_playing = false;
    m_angle = 0;
    update();
}

void VinylWidget::setRpm(int rpm)
{
    m_rpm = rpm > 0 ? rpm : 33;
}

void VinylWidget::setShowTonearm(bool on)
{
    m_showTonearm = on;
    update();
}

void VinylWidget::paintEvent(QPaintEvent*)
{
    /* 角度按真实 elapsed 推进（33⅓ RPM = 200°/s） */
    if (m_playing) {
        const int ms = m_clock.restart();
        m_angle += m_rpm * 360.0 * ms / 60000.0;
        while (m_angle >= 360.0)
            m_angle -= 360.0;
    }

    QPainter p(this);
    qximage::paintVinyl(p, rect(), (int)m_angle, m_cover, m_showTonearm);
}

void VinylWidget::showEvent(QShowEvent* e)
{
    QWidget::showEvent(e);
    if (m_playing) {
        m_clock.restart();
        m_timer.start(m_intervalMs);
    }
}

void VinylWidget::hideEvent(QHideEvent* e)
{
    m_timer.stop();
    QWidget::hideEvent(e);
}
