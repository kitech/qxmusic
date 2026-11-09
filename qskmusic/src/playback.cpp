#include "playback.h"
#include "audioplayer.h"
#include "logmodel.h"
#include <QUrl>

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
        Q_EMIT progressMsChanged(int(ms));
    });
    connect(ap, &AudioPlayer::durationChanged, this, [this](qint64 ms) {
        Q_EMIT durationMsChanged(int(ms));
    });

    auto& ncm = NcmApi::instance();
    connect(&ncm, &NcmApi::resolveDone, this,
        [this](qint64 songId, const QString& url, const QString& err) {
            m_resolving = false;
            if (songId != currentSongId()) return;   // 期间已切曲，丢弃
            if (!err.isEmpty() || url.isEmpty()) {
                Q_EMIT playError(err == QStringLiteral("NoResource")
                    ? QString::fromUtf8("灰色歌曲，无可播资源")
                    : QString::fromUtf8("获取播放链接失败：%1").arg(err));
                return;
            }
            AudioPlayer::instance()->playUrl(QString::number(songId), QUrl(url));
            m_playing = true;
            Q_EMIT playingChanged(true);
            NcmApi::instance().lyricAsync(songId);   // 歌词预取
        });
    connect(&ncm, &NcmApi::lyricDone, this,
        [this](qint64 songId, const QString& lrc, const QString&) {
            m_lrcSongId = songId;
            m_lrcText = lrc;
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
    m_queue = songs;
    playIndex(startIndex);
}

void Playback::playIndex(int index)
{
    if (index < 0 || index >= m_queue.size())
        return;   // 越界 = 队尾，自然停止
    const auto& s = m_queue.at(index);
    if (!s.playable) {
        Q_EMIT playError(QString::fromUtf8("灰色歌曲，无可播资源"));
        return;
    }
    if (m_resolving) return;
    m_current = index;
    m_lrcSongId = 0;
    m_lrcText.clear();
    m_resolving = true;
    m_playing = false;
    Q_EMIT songChanged();
    Q_EMIT progressMsChanged(0);
    Q_EMIT durationMsChanged(s.durationMs);
    LogModel::instance().append(LogModel::Info, "player",
        QString("play: %1 - %2").arg(s.name, s.artists));
    NcmApi::instance().resolveAsync(s.id, NCM_Q_EXHIGH);
}

void Playback::togglePause()
{
    auto* ap = AudioPlayer::instance();
    if (!ap->playingId().isEmpty()) {
        ap->pause();
        m_playing = false;
        Q_EMIT playingChanged(false);
    } else if (!ap->pausedId().isEmpty()) {
        ap->resume();
        m_playing = true;
        Q_EMIT playingChanged(true);
    } else if (m_current >= 0 && !m_resolving) {
        playIndex(m_current);   // 重播当前
    }
}

void Playback::next() { playIndex(m_current + 1); }
void Playback::prev() { playIndex(m_current - 1); }
