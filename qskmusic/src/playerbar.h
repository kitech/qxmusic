#ifndef PLAYER_BAR_H
#define PLAYER_BAR_H

#include <QskLinearBox.h>

class QskPushButton;
class QskProgressBar;
class QskTextLabel;
class ProgressLineItem;
class QTimer;

/*
 * 全局唯一播放条（qlmusic 主窗体底部播放条的 qsk 版）：
 *   布局横排：prev / play / next / 歌名歌手 / 进度条 / 时间 / 词 / 音量 / 队列
 *   右侧图标区附带下载进度（qlmusic setDownloadState 同款位置）。
 *   状态由 main.cpp 从 Playback/DownloadService 转发进来，页面不持有实例。
 */
class PlayerBar : public QskLinearBox
{
    Q_OBJECT
public:
    explicit PlayerBar(QQuickItem* parent = nullptr);

    void setSongInfo(const QString& title, const QString& artist);
    void setProgressMs(int ms);
    void setDurationMs(int ms);
    void setPlaying(bool playing);
    void setDownloadProgress(int pct);   // -1 未定；0-100；100 完成 1.2s 隐藏；-2 失败
    void attachProgressLine(ProgressLineItem* line) { m_line = line; }

Q_SIGNALS:
    void playPauseRequested();
    void prevRequested();
    void nextRequested();
    void queueRequested();
    void volumeClicked();
    void lyricRequested();

private:
    static QString fmtMs(int ms);

    QskPushButton* m_prevBtn = nullptr;
    QskPushButton* m_playBtn = nullptr;
    QskPushButton* m_nextBtn = nullptr;
    QskTextLabel* m_title = nullptr;
    QskTextLabel* m_artist = nullptr;
    QskTextLabel* m_timeLabel = nullptr;
    QskPushButton* m_lyricBtn = nullptr;
    QskPushButton* m_volumeBtn = nullptr;
    QskPushButton* m_queueBtn = nullptr;
    QskProgressBar* m_dlBar = nullptr;
    QskTextLabel* m_dlPct = nullptr;
    ProgressLineItem* m_line = nullptr;
    QTimer* m_dlHideTimer = nullptr;
    int m_progressMs = 0;
    int m_durationMs = 0;
    bool m_playing = false;
};

#endif
