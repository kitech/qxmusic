#include "playback.h"
#include "audioplayer.h"
#include "downloadservice.h"
#include "keepalive.h"
#include "logmodel.h"
#include <QUrl>
#include <QDebug>

Playback& Playback::instance()
{
    static Playback s;
    return s;
}

Playback::Playback(QObject* parent)
    : QObject(parent)
{
    auto* ap = AudioPlayer::instance();
    connect(ap, &AudioPlayer::finished, this, [this](const QString& id) {
        m_playing = false;
        Q_EMIT playingChanged(false);
        if (m_current < 0 || m_queue.isEmpty())
            return;
        const qint64 endedId = id.toLongLong();
        const qint64 curId = currentSongId();
        // 自然播完且就是当前曲：自动下一首；被打断的 finished 不推进
        if (endedId == curId || curId == 0)
            playIndex(m_current + 1);
    });
    connect(ap, &AudioPlayer::positionChanged, this, [this](qint64 ms) {
        m_positionMs = int(ms);
        Q_EMIT progressMsChanged(int(ms));
    });
    connect(ap, &AudioPlayer::durationChanged, this, [this](qint64 ms) {
        Q_EMIT durationMsChanged(int(ms));
    });
    /* 直链播放失败（版权 403 / 格式拒绝 / 断网）：此前无任何处理 →
     * 静默停住需手点；与 resolve 失败同策略：计数 + 自动下一首 */
    connect(ap, &AudioPlayer::playError, this, [this](const QString& id) {
        if (id.toLongLong() != currentSongId()) return;
        onStreamFailure(tr("Playback failed"), ErrPlayback);
    });

    auto& ncm = NcmApi::instance();
    connect(&ncm, &NcmApi::resolveDone, this,
        [this](qint64 songId, const QString& url, const QString& err) {
            m_resolving = false;
            Q_EMIT resolvingChanged(false);
            qDebug() << "[DIAG] resolveDone id=" << songId << "err=" << err;
            if (songId != currentSongId()) return;   // 期间已切曲，丢弃
            if (!err.isEmpty() || url.isEmpty()) {
                /* 统一失败出口（版权曲 NoResource/GEO）：计数 + 跳下一首 */
                onStreamFailure(err == QStringLiteral("NoResource")
                    ? tr("Unavailable track, no playable source")
                    : tr("Failed to resolve play URL: %1").arg(err), ErrResolve);
                return;
            }
            QString suffix = QUrl(url).path().section(QLatin1Char('.'), -1);
            if (suffix.isEmpty() || suffix.size() > 5)
                suffix = QStringLiteral("mp3");
            const auto headers = NcmApi::instance().streamHeaders(songId);
            if (!headers.isEmpty()) {
                /* 带 Referer 的直链（B站）：QMediaPlayer 无法加头 →
                 * DownloadService 落盘后播 file://（与 ql 同语义） */
                m_resolving = false;
                Q_EMIT resolvingChanged(false);
                if (DownloadService::instance().isCached(songId, suffix)) {
                    startPlayback(QUrl::fromLocalFile(
                        DownloadService::instance().filePath(songId, suffix)));
                } else {
                    m_waitDownloadId = songId;
                    m_waitSuffix = suffix;
                    DownloadService::instance().enqueue(songId, url, suffix, headers);
                }
                return;
            }
            startPlayback(QUrl(url));
            /* 自动缓存（qlmusic cache-first）：本地无文件则后台下载（空头） */
            DownloadService::instance().enqueue(songId, url, suffix, headers);
        });
    /* headers 下载完成 → 播本地文件；失败 → 跳下一首 */
    connect(&DownloadService::instance(), &DownloadService::progress, this,
        [this](qint64 id, int pct) {
            if (m_waitDownloadId != id || id != currentSongId()) return;
            if (pct == 100) {
                m_waitDownloadId = 0;
                startPlayback(QUrl::fromLocalFile(
                    DownloadService::instance().filePath(id, m_waitSuffix)));
            } else if (pct == -2) {
                m_waitDownloadId = 0;
                onStreamFailure(tr("Download failed"), ErrDownload);
            }
        });
    connect(&ncm, &NcmApi::lyricDone, this,
        [this](qint64 songId, const QString& lrc, const QString&) {
            m_lrcSongId = songId;
            m_lrcText = lrc;
            Q_EMIT lyricReady(songId, lrc);   // 弹层据此实时刷新
        });
    connect(&ncm, &NcmApi::playFailReceived, this, [this](qint64, const QString& reason) {
        Q_EMIT playError(tr("Playback failed: %1, skipping").arg(reason), ErrPlayback);
        playIndex(m_current + 1);
    });
}

const Song* Playback::currentSong() const
{
    if (m_current < 0 || m_current >= m_queue.size())
        return nullptr;
    return &m_queue.at(m_current);
}

qint64 Playback::currentSongId() const
{
    const auto* s = currentSong();
    return s ? s->id : 0;
}

QString Playback::currentTitle() const
{
    const auto* s = currentSong();
    if (!s)
        return QStringLiteral("qskmusic");
    return s->artists.isEmpty() ? s->name
                                : QString("%1 - %2").arg(s->name, s->artists);
}

void Playback::setQueue(const QList<Song>& songs, int startIndex)
{
    qDebug() << "[DIAG] setQueue size=" << songs.size() << "start=" << startIndex;
    m_queue = songs;
    playIndex(startIndex);
}

void Playback::startPlayback(const QUrl& url)
{
    if (!KeepAlive::requestAudioFocus()) {
        m_playing = false;
        Q_EMIT playingChanged(false);
        Q_EMIT playError(QString::fromUtf8("其他应用正在使用音频，请稍后重试"),
                         ErrAudioFocus);
        return;
    }
    AudioPlayer::instance()->playUrl(QString::number(currentSongId()), url);
    m_playing = true;
    m_positionMs = 0;
    m_errSkipRun = 0;      /* 起播成功：断"连续失败"链 */
    m_failedSongId = 0;
    Q_EMIT playingChanged(true);
    NcmApi::instance().lyricAsync(currentSongId());   /* 歌词预取 */
}

void Playback::onStreamFailure(const QString& why, int code)
{
    const qint64 id = currentSongId();
    /* 上下文前缀（歌名）：错误内容保持引擎/解析层原文，不改写 */
    const QString full = currentTitle().isEmpty()
        ? why : (currentTitle() + QStringLiteral(": ") + why);
    if (id != 0 && m_failedSongId == id)   /* 同曲只跳一次（playError+resolve 双通道去重） */
        return;
    m_failedSongId = id;
    m_resolving = false;
    Q_EMIT resolvingChanged(false);
    Q_EMIT playError(full, code);
    if (++m_errSkipRun >= 99) {
        m_errSkipRun = 0;
        m_playing = false;
        KeepAlive::abandonAudioFocus();
        Q_EMIT playingChanged(false);
        Q_EMIT playError(QString::fromUtf8("连续 99 首播放失败，已停止连播"),
                         ErrSkipStop);
        return;
    }
    playIndex(m_current + 1);
}

void Playback::playIndex(int index)
{
    qDebug() << "[DIAG] playIndex req=" << index
             << "size=" << m_queue.size()
             << "resolving=" << m_resolving;
    if (index < 0 || index >= m_queue.size()) {
        KeepAlive::abandonAudioFocus();
        Q_EMIT playError(tr("End of queue"), ErrNoNext);   /* 便于 UI 区分自然停止 */
        return;   // 越界 = 队尾，自然停止
    }

    // 灰歌：保留原 toast 提示；随后预跳到下一可播曲目（qlmusic 跳过语义）
    if (!m_queue.at(index).playable) {
        Q_EMIT playError(tr("Unavailable track, no playable source"), ErrResolve);
        const int usable = firstPlayableAtOrAfter(index + 1);
        if (usable < 0) {
            m_playing = false;
            KeepAlive::abandonAudioFocus();
            Q_EMIT playingChanged(false);
            Q_EMIT playError(tr("No playable track left"), ErrNoNext);
            return;
        }
        index = usable;
    }
    const auto& s = m_queue.at(index);
    if (m_resolving) return;
    m_current = index;
    m_lrcSongId = 0;
    m_lrcText.clear();
    m_resolving = true;
    m_playing = false;
    Q_EMIT songChanged();
    Q_EMIT resolvingChanged(true);
    Q_EMIT progressMsChanged(0);
    m_positionMs = 0;
    Q_EMIT durationMsChanged(s.durationMs);
    LogModel::instance().append(LogModel::Info, "player",
        QString("play: %1 - %2").arg(s.name, s.artists));
    qDebug() << "[DIAG] resolveAsync id=" << s.id;
    NcmApi::instance().resolveAsync(s.id, NCM_Q_EXHIGH);
}

int Playback::firstPlayableAtOrAfter(int from) const
{
    for (int i = from; i < m_queue.size(); ++i)
        if (m_queue.at(i).playable) return i;
    return -1;
}

void Playback::togglePause()
{
    // 媒体键可能同时经 Qt 与 MediaSession 到达（部分 ROM 双发）：300ms 去重
    if (m_toggleDebounce.isValid() && m_toggleDebounce.elapsed() < 300)
        return;
    m_toggleDebounce.start();

    auto* ap = AudioPlayer::instance();
    if (!ap->playingId().isEmpty()) {
        pause();
    } else if (!ap->pausedId().isEmpty()) {
        resume();
    } else if (m_current >= 0 && !m_resolving) {
        playIndex(m_current);   // 重播当前
    }
}

void Playback::pause()
{
    auto* ap = AudioPlayer::instance();
    if (!m_playing && ap->playingId().isEmpty())
        return;
    ap->pause();
    m_playing = false;
    m_resumeOnFocusGain = false;
    Q_EMIT playingChanged(false);
    qDebug() << "[ENV] playback paused";
}

void Playback::resume()
{
    auto* ap = AudioPlayer::instance();
    if (m_playing)
        return;
    if (!ap->pausedId().isEmpty()) {
        if (!KeepAlive::requestAudioFocus()) {
            Q_EMIT playError(tr("Audio focus lost, cannot resume"), ErrAudioFocus);
            return;
        }
        ap->resume();
        m_playing = true;
        m_resumeOnFocusGain = false;
        Q_EMIT playingChanged(true);
        qDebug() << "[ENV] playback resumed";
    } else if (m_current >= 0 && !m_resolving) {
        playIndex(m_current);
    }
}

void Playback::handleFocusChange(int change)
{
    qDebug() << "[ENV] focus change=" << change
             << "resumeFlag=" << m_resumeOnFocusGain;
    switch (change) {
    case 1:                                  // AUDIOFOCUS_GAIN
        if (m_resumeOnFocusGain) {
            m_resumeOnFocusGain = false;
            resume();
        }
        break;
    case -2: {                               // AUDIOFOCUS_LOSS_TRANSIENT
        const bool wasPlaying = m_playing;
        pause();
        m_resumeOnFocusGain = wasPlaying;
        LogModel::instance().append(LogModel::Info, "play",
            QString::fromUtf8("焦点瞬时丢失，暂停（%1）")
                .arg(wasPlaying ? QString::fromUtf8("结束后恢复")
                                : QString::fromUtf8("保持暂停")));
        break;
    }
    case -1:                                 // AUDIOFOCUS_LOSS
        m_resumeOnFocusGain = false;
        pause();
        LogModel::instance().append(LogModel::Info, "play",
            QString::fromUtf8("焦点永久丢失，已暂停"));
        break;
    case -3:                                 // CAN_DUCK：系统自动降音量，不处理
        break;
    default:
        break;
    }
}

void Playback::handleMediaCommand(int cmd)
{
    qDebug() << "[ENV] media command=" << cmd;
    switch (cmd) {
    case 1: resume(); break;
    case 2: pause(); break;
    case 3: next(); break;
    case 4: prev(); break;
    case 5: togglePause(); break;
    default: break;
    }
}

void Playback::next() { playIndex(m_current + 1); }
void Playback::prev() { playIndex(m_current - 1); }
