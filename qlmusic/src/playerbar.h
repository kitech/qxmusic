#ifndef PLAYERBAR_H
#define PLAYERBAR_H

#include "compat34.h"
#include "musicmodel.h"
#include <qwidget.h>
#include <qpixmap.h>

class LyricsStrip;

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
    void setArtistAvatar(const QString& url, const QString& seed);  /* 当前歌手圆形头像（可空） */
    void nudgeAutoSkipTip();         /* 点击反馈：主动重弹 tooltip（B2/B3 点击零反馈修） */
    QString tipForPos(const QPoint& p) const;   /* tooltip 命中计算唯一点 */
    QRect tipRectFor(const QPoint& p) const;
    void retranslateUi();
    void toggleVolumePopup();
    void onPopupVolume(int v);      /* 自绘 panel 值回调 → volumeChanged 信号（无 moc 桥） */
    /* 三行滚动歌词（playbar 右侧；本地 .lrc 优先 + NCM 兜底，见 plan-20261104-playbar-lyrics） */
    void setLyricsText(qint64 songTag, const QString& lrcText);
    void setLyricsPosition(int ms); /* positionChanged 驱动；内部二分、行变才重绘 */
    void clearLyrics();

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
    void resizeEvent(QResizeEvent* e);   /* LyricsStrip 子组件几何同步 */
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
    QRect volumeRect() const;       /* 喇叭（歌词区左邻；点击弹滑条） */
    QRect downloadRect() const;     /* NCM 下载进度 pie 圆环（喇叭左侧） */
    QRect lyricsRect() const;       /* 右侧三行滚动歌词区（空组件占位；最右） */
    int lyricLeft() const;          /* 歌词区左缘（动态缩宽保护 progress/时间） */
    QRect progressRect() const;
    void feedbackTip(const QRect& r, const QString& text);   /* 点击即时重弹（qt3/qt5 双路径） */
    void startSpinIfIndeterminate();
    void stopSpin();
    void setDownloadHiddenLater();

private slots:
    void refreshCover();
    void onDlHide();
    void onSpinTick();
    void onMuteToggled(bool on);    /* 静音 checkbox：on=音量0；off=回静音前值 */

private:
    Track m_track;
    bool m_hasTrack;
    bool m_playing;
    int m_progressMs;
    int m_durationMs;
    int m_repeatMode;
    bool m_autoSkip;
    int m_volume;                   /* 0-100；0=静音态 */
    int m_lastVolume;               /* 静音记忆（wheel/滑条拖回声位） */
    QString m_tipRepeat;
    QString m_tipAutoSkip;
    void*   m_tip;                  /* qt3 专用：maybeTip 子类实例（cpp 内定义） */
    Hit m_hover;
    bool m_dragging;

    QPixmap m_coverPixmap;          /* identicon 缓存（拖动高频重绘不再每帧生成） */
    QPixmap m_artistAvatar;         /* 当前歌手圆形头像（可空） */
    QString m_artistAvatarUrl;
    QString m_artistAvatarSeed;

    /* NCM 下载指示（Material pie 圆环：一图标表达状态+进度） */
    qint64 m_dlSongId;              /* >0 才显示 */
    int m_dlPct;                    /* 0-100；-1=未定（无 Content-Length 旋转）；-2=失败红斜杠 */
    int m_spinTick;                 /* 未定态旋转角（m_spinTimer 驱动） */
    void* m_spinTimer;              /* QTimer*（cpp 内定义，规避头文件依赖） */
    void* m_volumePopup;            /* VolumePopupFrame*（cpp 内定义，WType_Popup） */
    void* m_volumeSlider;           /* VolumePanelSlider*（cpp 内定义，自绘滑条） */
    void* m_muteCheck;              /* QCheckBox*（弹出条右侧静音 checkbox，防误触） */
    LyricsStrip* m_lyrics;          /* 三行滚动歌词（cpp 内定义完整类） */
};

#endif // PLAYERBAR_H
