#ifndef PLAYERBAR_H
#define PLAYERBAR_H

#include "compat34.h"
#include "musicmodel.h"
#include <qwidget.h>

class QSlider;

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
    void setVolume(int v);           /* 0-100；0=静音态（喇叭画斜杠） */
    void setDownloadState(qint64 songId, int pct);   /* 0-100 进度；-1=未定；-2=失败；100=完成即清 */
    void nudgeAutoSkipTip();         /* 点击反馈：主动重弹 tooltip（B2/B3 点击零反馈修） */
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
    void volumeChanged(int v);

protected:
    void paintEvent(QPaintEvent* e);
    bool event(QEvent* e);          /* qt5 QEvent::ToolTip 派发（qt3 走 maybeTip 子类） */
    void mousePressEvent(QMouseEvent* e);
    void mouseMoveEvent(QMouseEvent* e);
    void mouseReleaseEvent(QMouseEvent* e);
    void leaveEvent(QEvent* e);
    void wheelEvent(QWheelEvent* e);   /* 播放条滚轮=音量 ±5（VLC/Spotify 桌面惯例） */

private:
    enum Hit { HitNone, HitPrev, HitPlay, HitNext, HitProgress, HitRepeat,
               HitAutoSkip, HitVolume, HitDownload };
    void setProgressFromX(int x);
    QRect playRect() const;
    QRect prevRect() const;
    QRect nextRect() const;
    QRect repeatRect() const;       /* 单曲重复/列表循环 按钮 */
    QRect autoSkipRect() const;     /* 播放失败自动跳过 开关 */
    QRect volumeRect() const;       /* 喇叭（点击弹滑条） */
    QRect downloadRect() const;     /* NCM 下载进度 pie 圆环 */
    QRect progressRect() const;
    void toggleVolumePopup();
    void feedbackTip(const QRect& r, const QString& text);   /* 点击即时重弹（qt3/qt5 双路径） */
    void startSpinIfIndeterminate();
    void stopSpin();
    void setDownloadHiddenLater();

private slots:
    void onDlHide();
    void onSpinTick();

private:
    Track m_track;
    bool m_hasTrack;
    bool m_playing;
    int m_progressMs;
    int m_durationMs;
    int m_repeatMode;
    bool m_autoSkip;
    int m_volume;                   /* 0-100；0=静音态 */
    QString m_tipRepeat;
    QString m_tipAutoSkip;
    void*   m_tip;                  /* qt3 专用：maybeTip 子类实例（cpp 内定义） */
    Hit m_hover;
    bool m_dragging;

    /* NCM 下载指示（Material pie 圆环：一图标表达状态+进度） */
    qint64 m_dlSongId;              /* >0 才显示 */
    int m_dlPct;                    /* 0-100；-1=未定（无 Content-Length 旋转）；-2=失败红斜杠 */
    int m_spinTick;                 /* 未定态旋转角（m_spinTimer 驱动） */
    void* m_spinTimer;              /* QTimer*（cpp 内定义，规避头文件依赖） */
    void* m_volumePopup;            /* qt3 WType_Popup / qt5 Qt::Popup（cpp 内定义） */
    void* m_volumeSlider;           /* QSlider*（cpp 内定义） */
};

#endif // PLAYERBAR_H
