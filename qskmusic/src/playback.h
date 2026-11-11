#ifndef PLAYBACK_H
#define PLAYBACK_H

#include <QObject>
#include <QUrl>
#include <QList>

#include "ncmapi.h"

/*
 * 全局播放控制器（qlmusic MainWindow 集中处理播放的等价物）：
 *   队列 = QList<Song>；resolve → AudioPlayer::playUrl；
 *   页面只向它投队列/切曲；全局 PlayerBar/歌词/队列弹层全部对接它。
 * 自然播完自动下一首（队列内推进，无跨页抢播问题）。
 */
class Playback : public QObject
{
    Q_OBJECT
public:
    static Playback& instance();

    void setQueue(const QList<Song>& songs, int startIndex);
    void playIndex(int index);
    void togglePause();
    void next();
    void prev();

    const QList<Song>& queue() const { return m_queue; }
    int currentIndex() const { return m_current; }
    const Song* currentSong() const;
    qint64 currentSongId() const;   // 0 = 无在播
    QString currentTitle() const;

    // 歌词缓存（播放成功即预取；LyricPopup 直接取）
    bool lyricCached(qint64 songId) const { return m_lrcSongId == songId && !m_lrcText.isEmpty(); }
    const QString& lrcText() const { return m_lrcText; }

Q_SIGNALS:
    void songChanged();
    void playingChanged(bool playing);
    void progressMsChanged(int ms);
    void durationMsChanged(int ms);
    void playError(const QString& message);
    // resolve 实锤不可播（灰歌/仅试听/地区限制）→ 列表摘对号（qlmusic 同口径）
    void playableChanged(qint64 songId);

private:
    explicit Playback(QObject* parent = nullptr);

    QList<Song> m_queue;
    int m_current = -1;
    bool m_resolving = false;
    bool m_playing = false;
    qint64 m_lrcSongId = 0;
    QString m_lrcText;
};

#endif
