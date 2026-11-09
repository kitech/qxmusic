#ifndef PLAYER_BAR_H
#define PLAYER_BAR_H

#include <QskLinearBox.h>

class QskPushButton;
class QskProgressBar;
class QskTextLabel;

/*
 * 全局唯一播放条（qlmusic 主窗体底部播放条的 qsk 版）：
 *   布局横排：prev / play / next / 进度条 / 时间 / 词 / 音量 / 队列。
 *   状态由 main.cpp 从 Playback 单例转发进来，页面不持有实例。
 */
class PlayerBar : public QskLinearBox
{
    Q_OBJECT
public:
    explicit PlayerBar(QQuickItem* parent = nullptr);

    void setProgressMs(int ms);
    void setDurationMs(int ms);
    void setPlaying(bool playing);

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
    QskProgressBar* m_progress = nullptr;
    QskTextLabel* m_timeLabel = nullptr;
    QskPushButton* m_lyricBtn = nullptr;
    QskPushButton* m_volumeBtn = nullptr;
    QskPushButton* m_queueBtn = nullptr;
    int m_progressMs = 0;
    int m_durationMs = 0;
    bool m_playing = false;
};

#endif
