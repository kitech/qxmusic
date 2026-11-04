#ifndef PLAYERBAR_H
#define PLAYERBAR_H

#include "compat34.h"
#include "musicmodel.h"
#include <qwidget.h>

class PlayerBar : public QWidget {
    Q_OBJECT
public:
    explicit PlayerBar(QWidget* parent = 0);
    ~PlayerBar();
    void setTrack(const Track& t);
    void setPlaying(bool on);
    bool isPlaying() const { return m_playing; }
    void setProgress(int ms);
    void setDuration(int ms);
    void setRepeatMode(int mode);     /* 0=off 1=all 2=one */
    void setAutoSkip(bool on);
    QString tipForPos(const QPoint& p) const;   /* tooltip 命中计算唯一点 */
    QRect tipRectFor(const QPoint& p) const;
    void retranslateUi();

signals:
    void playStateChanged(bool playing);
    void playPauseRequested();
    void prevRequested();
    void nextRequested();
    void seekRequested(int ms);
    void repeatClicked();
    void autoSkipClicked();

protected:
    void paintEvent(QPaintEvent* e);
    bool event(QEvent* e);          /* qt5 QEvent::ToolTip 派发（qt3 走 maybeTip 子类） */
    void mousePressEvent(QMouseEvent* e);
    void mouseMoveEvent(QMouseEvent* e);
    void mouseReleaseEvent(QMouseEvent* e);
    void leaveEvent(QEvent* e);

private:
    enum Hit { HitNone, HitPrev, HitPlay, HitNext, HitProgress, HitRepeat, HitAutoSkip };
    void setProgressFromX(int x);
    QRect playRect() const;
    QRect prevRect() const;
    QRect nextRect() const;
    QRect repeatRect() const;       /* 单曲重复/列表循环 按钮 */
    QRect autoSkipRect() const;     /* 播放失败自动跳过 开关 */
    QRect progressRect() const;

    Track m_track;
    bool m_hasTrack;
    bool m_playing;
    int m_progressMs;
    int m_durationMs;
    int m_repeatMode;
    bool m_autoSkip;
    QString m_tipRepeat;
    QString m_tipAutoSkip;
    void*   m_tip;                  /* qt3 专用：maybeTip 子类实例（cpp 内定义） */
    Hit m_hover;
    bool m_dragging;
};

#endif // PLAYERBAR_H
