#ifndef PLAYER_BAR_H
#define PLAYER_BAR_H

#include <QskLinearBox.h>

class QskPushButton;
class QskProgressBar;
class QskTextLabel;

/*
 * 底部播放条初版（qsk 化：qlmusic playerbar 的 QSkinny 组合控件版，桩状态）。
 * 布局横排：prev / play / next / 进度条 / 时间 / 音量 / 队列。
 * 数据接线下轮（cxaudio FFI）；本轮仅本地桩状态与信号。
 */
class PlayerBar : public QskLinearBox
{
    Q_OBJECT
public:
    explicit PlayerBar(QQuickItem* parent = nullptr);

    void setProgressMs(int ms);
    void setDurationMs(int ms);
    void setPlaying(bool playing);
    void updateCompact(bool compact);

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
    QskPushButton* m_volumeBtn = nullptr;
    QskPushButton* m_queueBtn = nullptr;
    QskPushButton* m_lyricBtn = nullptr;
    int m_progressMs = 0;
    int m_durationMs = 0;
    bool m_playing = false;
};

#endif
