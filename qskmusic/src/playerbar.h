#ifndef PLAYER_BAR_H
#define PLAYER_BAR_H

#include <QskLinearBox.h>

class QskPushButton;
class QskTextLabel;
class ProgressLineItem;

/*
 * 全局唯一播放条（qlmusic 主窗体底部播放条的 qsk 版）：
 *   两行高（56dp）单层横排五格：icon | 文本列(歌名/歌手) | 圆形播放键 | 时间列 | 队列
 *   —— 切歌：文本列左右横划（原 ⏮/⏭ 按钮功能保留，位置换手势）
 *   —— 歌词：队列键长按（原「词」按钮功能保留）
 *   —— 音量：仅桌面；Android 隐藏（设计明确要求）
 *   —— 下载进度：右侧瞬时显示（仅下载中出现，沿用原行为）
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
    void setDownloadProgress(int pct);   // 驱动下载进度线（-1 未定；0-100；100/-2 → 归零）
    void attachProgressLine(ProgressLineItem* line) { m_line = line; }
    void attachDownloadLine(ProgressLineItem* line) { m_dlLine = line; }

Q_SIGNALS:
    void playPauseRequested();
    void prevRequested();      // 由文本列左划触发
    void nextRequested();      // 由文本列右划触发
    void queueRequested();
    void volumeClicked();      // 仅桌面（Android 无该按钮）
    void lyricRequested();     // 队列键长按

private:
    static QString fmtMs(int ms);

    QskTextLabel* m_icon = nullptr;
    QskTextLabel* m_title = nullptr;
    QskTextLabel* m_artist = nullptr;
    QskPushButton* m_playBtn = nullptr;
    QskTextLabel* m_timeCur = nullptr;
    QskTextLabel* m_timeDur = nullptr;
    QskPushButton* m_lyricBtn = nullptr;    // 歌词入口（点击）
    QskPushButton* m_queueBtn = nullptr;
    QskPushButton* m_volumeBtn = nullptr;   // 仅桌面创建；Android 为 nullptr
    ProgressLineItem* m_line = nullptr;
    ProgressLineItem* m_dlLine = nullptr;   // 下载进度线（播放条下方，常显）
    int m_progressMs = 0;
    int m_durationMs = 0;
    bool m_playing = false;
};

#endif
