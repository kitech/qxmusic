#ifndef PLAYER_BAR_H
#define PLAYER_BAR_H

#include <QskLinearBox.h>

class QskPushButton;
class QskTextLabel;
class IconItem;
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
    void setCover(const QString& url, const QString& seed);   // 专辑封面（无图用 identicon）
    void setArtistAvatar(const QString& url, const QString& seed);  // 当前歌手头像（圆形）
    void setProgressMs(int ms);
    void setDurationMs(int ms);
    void setPlaying(bool playing);
    void setDownloadProgress(int pct);   // 驱动下载进度线（-1 未定；0-100；100/-2 → 归零）
    void setShuffleOn(bool on);          // Playback::shuffleChanged 反向同步按钮态
    void attachProgressLine(ProgressLineItem* line) { m_line = line; }
    void attachDownloadLine(ProgressLineItem* line) { m_dlLine = line; }
    /* 场景坐标命中：1=封面/歌名（歌曲详情），2=歌手头像/名字（歌手详情），0=无 */
    int clickTargetAt(const QPointF& scenePos) const;

protected:
    /* 播放条空白/文本区点击：封面+歌名 → 歌曲详情；歌手头像+名字 → 歌手详情
     * （子控件 Play/队列等自带按钮优先消费，不经过这里） */
    void mousePressEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    bool eventFilter(QObject* obj, QEvent* ev) override;

Q_SIGNALS:
    void playPauseRequested();
    void prevRequested();      // 由文本列左划触发
    void nextRequested();      // 由文本列右划触发
    void queueRequested();
    void shuffleRequested();
    void songInfoRequested();   // 封面/歌名 → 歌曲详情
    void artistRequested();     // 歌手头像/歌手名 → 歌手详情
    void volumeClicked();      // 仅桌面（Android 无该按钮）
    void lyricRequested();     // 队列键长按

private:
    QRectF itemRect(QQuickItem* it) const;   /* 子项在本条坐标系下的矩形 */
    QPointF m_pressPos;
    bool m_pressValid = false;
    static QString fmtMs(int ms);

    IconItem* m_icon = nullptr;
    IconItem* m_artistAvatar = nullptr;
    QskTextLabel* m_title = nullptr;
    QskTextLabel* m_artist = nullptr;
    QskPushButton* m_playBtn = nullptr;
    QskTextLabel* m_timeCur = nullptr;
    QskTextLabel* m_timeDur = nullptr;
    QskPushButton* m_lyricBtn = nullptr;    // 歌词入口（点击）
    QskPushButton* m_queueBtn = nullptr;
    QskPushButton* m_shuffleBtn = nullptr;  // 随机乱序 开关（checkable）
    QskPushButton* m_volumeBtn = nullptr;   // 仅桌面创建；Android 为 nullptr
    ProgressLineItem* m_line = nullptr;
    ProgressLineItem* m_dlLine = nullptr;   // 下载进度线（播放条下方，常显）
    QString m_lastArtist;                   // resolve 完成后恢复歌手行显示
    int m_progressMs = 0;
    int m_durationMs = 0;
    bool m_playing = false;
};

#endif
