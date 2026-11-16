#ifndef PLAYBACK_H
#define PLAYBACK_H

#include <QObject>
#include <QUrl>
#include <QList>
#include <QElapsedTimer>

#include "ncmapi.h"
#include "queuestore.h"

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
    /* 播放错误码：接收端可据此细分处理（Toast/换源/重试/后端提示…） */
    enum ErrorCode {
        ErrResolve    = 1,   /* 取链失败（NCM rc / B站 stream / 版权） */
        ErrPlayback   = 2,   /* AudioPlayer/QMediaPlayer 解码/网络播放失败 */
        ErrDownload   = 3,   /* DownloadService 失败（带 headers 直链下载） */
        ErrAudioFocus = 4,   /* 系统音频焦点被占用 */
        ErrSkipStop   = 5,   /* 连续失败达到止损阈值（99） */
        ErrNoNext     = 6,   /* 队列到头 / 无下一首 */
    };
    Q_ENUM(ErrorCode)

    static Playback& instance();

    void setQueue(const QList<Song>& songs, int startIndex);
    void playIndex(int index);
    void togglePause();
    void next();
    void prev();

    // 阶段2：幂等播控（锁屏/焦点回调共用）
    bool isPlaying() const { return m_playing; }
    int positionMs() const { return m_positionMs; }
    void pause();
    void resume();
    bool shuffle() const { return m_shuffle; }
    void setShuffle(bool on);            // 乱序开关（持久化 QSettings("shuffle")）
    void handleFocusChange(int change);      // 1 GAIN / -1 LOSS / -2 TRANSIENT / -3 DUCK
    void handleMediaCommand(int cmd);        // 1 play 2 pause 3 next 4 prev 5 toggle

    const QList<Song>& queue() const { return m_queue; }
    int currentIndex() const { return m_current; }
    const Song* currentSong() const;
    qint64 currentSongId() const;   // 0 = 无在播
    QString currentTitle() const;

    // 播放队列持久化（musiccom::QueueStore，queue.json 原子写）
    void initQueueStore(const QString& dir);              // main.cpp 启动调用一次
    void restoreLoaded();                                 // 启动恢复（摆队列不自动播）
    void saveState();                                     // current<0 不写（保留旧快照）

    // 歌词缓存（播放成功即预取；LyricPopup 直接取）
    bool lyricCached(qint64 songId) const { return m_lrcSongId == songId && !m_lrcText.isEmpty(); }
    const QString& lrcText() const { return m_lrcText; }

Q_SIGNALS:
    void songChanged();
    void playingChanged(bool playing);
    // resolve 链接期间 true（慢网可达数秒）→ PlayerBar 显示"获取中"
    void resolvingChanged(bool resolving);
    void progressMsChanged(int ms);
    void durationMsChanged(int ms);
    void playError(const QString& message, int code);
    // 歌词到达/失败（空文本=无歌词）→ 弹层实时刷新（修复"永远加载中"）
    void lyricReady(qint64 songId, const QString& text);
    void shuffleChanged(bool on);   // 乱序开关变化（PlayerBar 按钮同步）

private:
    explicit Playback(QObject* parent = nullptr);

    int firstPlayableAtOrAfter(int from) const;   // 预跳过灰歌；无则 -1
    /* 版权/解码/下载失败统一出口：计数 + 跳下一首（同曲去重） */
    void onStreamFailure(const QString& why, int code = ErrResolve);
    /* 起播统一入口（成功归零失败计数、歌词预取） */
    void startPlayback(const QUrl& url);
    /* 乱序推进：shuffle && size>1 → 随机(≠当前)；否则顺序 +1（prev 不走此） */
    int advanceIndex() const;

    QList<Song> m_queue;
    int m_current = -1;
    bool m_resolving = false;
    bool m_playing = false;
    qint64 m_failedSongId = 0;    // 已因失败跳过的曲目（同曲只跳一次）
    qint64 m_waitDownloadId = 0;  // headers 非空：等待下载完成后播本地文件
    QString m_waitSuffix;
    int m_positionMs = 0;
    bool m_resumeOnFocusGain = false;        // 瞬时丢失后是否恢复
    QElapsedTimer m_toggleDebounce;          // Qt 媒体键 + MediaSession 双通道去重
    int m_errSkipRun = 0;   // 连续播放失败计数（qlmusic skipOnError 同义，阈值 99）
    bool m_shuffle = false; // 随机乱序（切曲随机、prev 保持顺序；QSettings 持久化）
    qint64 m_lrcSongId = 0;
    QString m_lrcText;
    musiccom::QueueStore* m_store = nullptr;   // 队列持久化（initQueueStore 创建）
};

#endif
