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

    auto& ncm = NcmApi::instance();
    connect(&ncm, &NcmApi::resolveDone, this,
        [this](qint64 songId, const QString& url, const QString& err) {
            m_resolving = false;
            qDebug() << "[DIAG] resolveDone id=" << songId << "err=" << err;
            if (songId != currentSongId()) return;   // 期间已切曲，丢弃
            if (!err.isEmpty() || url.isEmpty()) {
                // 对号只由列表数据（服务端 playable）决定；播放失败不改列表标记
                Q_EMIT playError(err == QStringLiteral("NoResource")
                    ? QString::fromUtf8("灰色歌曲，无可播资源")
                    : QString::fromUtf8("获取播放链接失败：%1").arg(err));
                // qlmusic MainWindow::skipOnError（:1153-1174）：
                // 连续失败计数；未达阈值 → 自动跳下一首；达 99 → 停止连播
                if (++m_errSkipRun >= 99) {
                    m_errSkipRun = 0;
                    m_playing = false;
                    KeepAlive::abandonAudioFocus();
                    Q_EMIT playingChanged(false);
                    Q_EMIT playError(QString::fromUtf8(
                        "连续 99 首播放失败，已停止连播"));
                    return;
                }
                playIndex(m_current + 1);   // 自动下一首（内部会预跳灰歌）
                return;
            }
            // 阶段2a：起播前申请音频焦点（Android 15+ 需前台服务已在运行，本应用满足）
            if (!KeepAlive::requestAudioFocus()) {
                m_playing = false;
                Q_EMIT playingChanged(false);
                Q_EMIT playError(QString::fromUtf8(
                    "其他应用正在使用音频，请稍后重试"));
                return;
            }
            AudioPlayer::instance()->playUrl(QString::number(songId), QUrl(url));
            m_playing = true;
            m_positionMs = 0;
            m_errSkipRun = 0;   // 起播成功即中断"连续失败"链
            Q_EMIT playingChanged(true);
            NcmApi::instance().lyricAsync(songId);   // 歌词预取

            // 自动缓存（qlmusic cache-first）：本地无文件则后台下载
            QString suffix = QUrl(url).path().section(QLatin1Char('.'), -1);
            if (suffix.isEmpty() || suffix.size() > 5)
                suffix = QStringLiteral("mp3");
            DownloadService::instance().enqueue(songId, url, suffix);
        });
    connect(&ncm, &NcmApi::lyricDone, this,
        [this](qint64 songId, const QString& lrc, const QString&) {
            m_lrcSongId = songId;
            m_lrcText = lrc;
            Q_EMIT lyricReady(songId, lrc);   // 弹层据此实时刷新
        });
    connect(&ncm, &NcmApi::playFailReceived, this, [this](qint64, const QString& reason) {
        Q_EMIT playError(QString::fromUtf8("播放失败：%1，切下一首").arg(reason));
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

void Playback::playIndex(int index)
{
    qDebug() << "[DIAG] playIndex req=" << index
             << "size=" << m_queue.size()
             << "resolving=" << m_resolving;
    if (index < 0 || index >= m_queue.size()) {
        KeepAlive::abandonAudioFocus();
        return;   // 越界 = 队尾，自然停止
    }

    // 灰歌：保留原 toast 提示；随后预跳到下一可播曲目（qlmusic 跳过语义）
    if (!m_queue.at(index).playable) {
        Q_EMIT playError(QString::fromUtf8("灰色歌曲，无可播资源"));
        const int usable = firstPlayableAtOrAfter(index + 1);
        if (usable < 0) {
            m_playing = false;
            KeepAlive::abandonAudioFocus();
            Q_EMIT playingChanged(false);
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
            Q_EMIT playError(QString::fromUtf8("音频焦点被占用，无法恢复播放"));
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
