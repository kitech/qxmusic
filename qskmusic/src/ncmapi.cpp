#include "ncmapi.h"
#include <QStandardPaths>
#include <QDir>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDebug>
#include <thread>

namespace {

QString statusText(ncm::Status s)
{
    return QString::fromStdString(std::string(ncm::to_string(s)));
}

/* 失败出参错误 JSON 里的 errmsg 原文，零加工。 */
QString errMsgFromJson(const std::string& json)
{
    const auto o = QJsonDocument::fromJson(
        QByteArray::fromStdString(json)).object();
    return o.value(QStringLiteral("errmsg")).toString();
}

/* 分类文案原样保留，尾部追加 ": errmsg"（errmsg 为空则不拼）。 */
QString errText(ncm::Status st, const std::string& json)
{
    QString e = statusText(st);
    const QString m = errMsgFromJson(json);
    if (!m.isEmpty()) e += QStringLiteral(": ") + m;
    return e;
}

Song parseSong(const QJsonObject& o)
{
    Song s;
    s.id = o.value("id").toVariant().toLongLong();
    s.name = o.value("name").toString();
    QString artists;
    QList<qint64> ids;
    const auto arr = o.value("artists").toArray();
    for (const auto& a : arr) {
        if (!artists.isEmpty()) artists += QStringLiteral("/");
        artists += a.isObject() ? a.toObject().value("name").toString()
                                : a.toString();
    }
    const auto idArr = o.value("artist_ids").toArray();
    for (const auto& v : idArr) {
        const qint64 id = v.toVariant().toLongLong();
        if (id > 0) ids.append(id);
    }
    s.artists = artists;
    s.artistIds = ids;
    s.album = o.value("album").toString();
    s.cover = o.value("cover").toString();
    s.albumId = o.value("album_id").toVariant().toLongLong();
    s.durationMs = o.value("duration_ms").toInt();
    s.playable = o.value("playable").toBool(true);
    return s;
}

QList<Song> parseSongs(const QByteArray& json)
{
    const auto doc = QJsonDocument::fromJson(json);
    QJsonArray arr;
    if (doc.isObject())
        arr = doc.object().value("songs").toArray();
    else if (doc.isArray())
        arr = doc.array();
    QList<Song> out;
    for (const auto& v : arr)
        out.append(parseSong(v.toObject()));
    return out;
}

} // namespace

NcmApi& NcmApi::instance()
{
    static NcmApi s;
    return s;
}

NcmApi::NcmApi(QObject* parent)
    : QObject(parent)
    , m_client((QStandardPaths::writableLocation(
          QStandardPaths::AppLocalDataLocation)
          + QStringLiteral("/ncm")).toStdString())
{
    QDir().mkpath(QStandardPaths::writableLocation(
        QStandardPaths::AppLocalDataLocation) + QStringLiteral("/ncm"));

    qDebug() << "[qskmusic] ncm version:" << version();

    // 事件回调在 Go goroutine 触发 → 队列回主线程再发信号
    m_client.on_event([](void* ud, int32_t event, const char* json) {
        if (event != NCM_EVT_PLAYFAIL)
            return;
        auto* api = static_cast<NcmApi*>(ud);
        const auto o = QJsonDocument::fromJson(
            QByteArray(json ? json : "")).object();
        const qint64 songId = o.value("song_id").toVariant().toLongLong();
        const QString reason = o.value("reason").toString();
        QMetaObject::invokeMethod(api, [api, songId, reason]() {
            Q_EMIT api->playFailReceived(songId, reason);
        });
    }, this);
}

QString NcmApi::version() const
{
    return QString::fromStdString(ncm::Client::version());
}

void NcmApi::searchAsync(const QString& keywords, int limit)
{
    const std::string kw = keywords.toStdString();
    std::thread([this, kw, limit]() {
        std::string json;
        const auto st = (m_site == QStringLiteral("bili"))
            ? m_client.bili_search(kw, 1, json)
            : m_client.search(kw, limit, json);
        QList<Song> songs;
        QString err;
        if (st != ncm::Status::Ok) {
            err = errText(st, json);
        } else {
            songs = parseSongs(QByteArray::fromStdString(json));
        }
        qDebug() << "[qskmusic] ncm search done:" << songs.size() << err;
        Q_EMIT searchDone(songs, err);
    }).detach();
}

void NcmApi::resolveAsync(qint64 songId, int quality)
{
    const QString site = m_site;
    std::thread([this, songId, quality, site]() {
        std::string json;
        const auto st = (site == QStringLiteral("bili"))
            ? m_client.bili_stream(songId, json)          /* aid → 直链+headers */
            : m_client.resolve_source(songId,
                  static_cast<ncm::Quality>(quality), json);
        QString url, err;
        QMap<QString, QString> headers;
        if (st != ncm::Status::Ok) {
            err = errText(st, json);
        } else {
            const auto o = QJsonDocument::fromJson(
                QByteArray::fromStdString(json)).object();
            url = o.value("url").toString();
            const auto hs = o.value("headers").toObject();
            for (auto it = hs.begin(); it != hs.end(); ++it)
                headers.insert(it.key(), it.value().toString());
            if (url.isEmpty())
                err = QStringLiteral("no url");
        }
        {
            QMutexLocker lk(&m_hm);
            m_headers.insert(songId, headers);
        }
        qDebug() << "[qskmusic] ncm resolve:" << songId << site
                 << (err.isEmpty() ? QStringLiteral("ok") : err)
                 << "headers=" << headers.size();
        Q_EMIT resolveDone(songId, url, err);
    }).detach();
}

void NcmApi::resolveForDownloadAsync(qint64 songId)
{
    std::thread([this, songId]() {
        std::string json;
        const auto st = m_client.resolve_source(songId,
            static_cast<ncm::Quality>(NCM_Q_EXHIGH), json);
        QString url, err;
        if (st != ncm::Status::Ok) {
            err = errText(st, json);
        } else {
            const auto o = QJsonDocument::fromJson(
                QByteArray::fromStdString(json)).object();
            url = o.value("url").toString();
            if (url.isEmpty())
                err = QStringLiteral("no url");
        }
        qDebug() << "[qskmusic] ncm download url:" << songId
                 << (err.isEmpty() ? QStringLiteral("ok") : err);
        Q_EMIT downloadUrlDone(songId, url, err);
    }).detach();
}

void NcmApi::artistAsync(qint64 artistId)
{
    if (artistId <= 0)
        return;
    std::thread([this, artistId]() {
        std::string json;
        const auto st = m_client.artist_detail(artistId, json);
        QString pic, err;
        if (st != ncm::Status::Ok) {
            err = errText(st, json);
        } else {
            const auto o = QJsonDocument::fromJson(
                QByteArray::fromStdString(json)).object();
            pic = o.value("pic_url").toString();
            if (pic.isEmpty())
                err = QStringLiteral("no pic");
        }
        qDebug() << "[qskmusic] ncm artist:" << artistId
                 << (err.isEmpty() ? QStringLiteral("ok") : err);
        Q_EMIT artistDone(artistId, pic, err);
    }).detach();
}

/* 歌手歌曲（热门 50 首）：worker 线程取 → artistSongsDone(QList<Song>, err) */
void NcmApi::artistSongsAsync(qint64 artistId, int limit, int offset,
                              const QString& order)
{
    if (artistId <= 0)
        return;
    const std::string ord = order.toStdString();
    std::thread([this, artistId, limit, offset, ord]() {
        std::string json;
        const auto st = m_client.artist_songs(artistId, limit, offset, ord, json);
        QList<Song> songs;
        QString err;
        if (st != ncm::Status::Ok) {
            err = errText(st, json);
        } else {
            songs = parseSongs(QByteArray::fromStdString(json));
        }
        qDebug() << "[qskmusic] ncm artist songs:" << artistId
                 << "n=" << songs.size()
                 << (err.isEmpty() ? QStringLiteral("ok") : err);
        Q_EMIT artistSongsDone(artistId, songs, err);
    }).detach();
}

void NcmApi::likeListAsync()
{
    std::thread([this]() {
        std::string json;
        const auto st = m_client.like_list(json);
        QList<qint64> ids;
        QString err;
        if (st != ncm::Status::Ok) {
            err = errText(st, json);
        } else {
            const auto arr = QJsonDocument::fromJson(
                QByteArray::fromStdString(json)).object()
                .value(QStringLiteral("ids")).toArray();
            for (const auto& v : arr) {
                const qint64 id = v.toVariant().toLongLong();
                if (id > 0) ids.append(id);
            }
        }
        qDebug() << "[qskmusic] ncm like_list:" << ids.size() << err;
        Q_EMIT likeListDone(ids, err);
    }).detach();
}

void NcmApi::likeAsync(qint64 songId, bool on)
{
    if (songId <= 0)
        return;
    std::thread([this, songId, on]() {
        std::string json;
        const auto st = m_client.like(songId, on, json);
        QString err = st == ncm::Status::Ok ? QString() : errText(st, json);
        qDebug() << "[qskmusic] ncm like" << songId << on
                 << (err.isEmpty() ? QStringLiteral("ok") : err);
        Q_EMIT likeDone(songId, on, err);
    }).detach();
}

void NcmApi::songDetailAsync(const QString& ids)
{
    const std::string idStr = ids.toStdString();
    std::thread([this, idStr]() {
        std::string json;
        const auto st = m_client.song_detail(idStr, json);
        QList<Song> songs;
        QString err;
        if (st != ncm::Status::Ok)
            err = errText(st, json);
        else
            songs = parseSongs(QByteArray::fromStdString(json));
        qDebug() << "[qskmusic] ncm song_detail:" << songs.size() << err;
        Q_EMIT songDetailDone(songs, err);
    }).detach();
}

/* 评论：按当前站点分流（ncm_comment / ncm_bili_comment），出参同形 */
void NcmApi::commentsAsync(qint64 id, int limit, int offset)
{
    if (id <= 0)
        return;
    const QString site = m_site;
    std::thread([this, id, limit, offset, site]() {
        std::string json;
        const auto st = (site == QStringLiteral("bili"))
            ? m_client.bili_comment(id, limit, json)
            : m_client.comment(id, limit, offset, json);
        QList<Comment> comments;
        int total = 0;
        QString err;
        if (st != ncm::Status::Ok) {
            err = errText(st, json);
        } else {
            const auto root = QJsonDocument::fromJson(
                QByteArray::fromStdString(json)).object();
            total = root.value(QStringLiteral("total")).toInt();
            const auto arr = root.value(QStringLiteral("comments")).toArray();
            for (const auto& v : arr) {
                const auto o = v.toObject();
                Comment c;
                c.id = o.value(QStringLiteral("id")).toVariant().toLongLong();
                c.content = o.value(QStringLiteral("content")).toString();
                c.likedCount = o.value(QStringLiteral("liked_count")).toVariant().toLongLong();
                c.timeMs = o.value(QStringLiteral("time_ms")).toVariant().toLongLong();
                c.nickname = o.value(QStringLiteral("nickname")).toString();
                c.avatar = o.value(QStringLiteral("avatar")).toString();
                c.replyCount = o.value(QStringLiteral("reply_count")).toVariant().toLongLong();
                if (!c.content.isEmpty() || !c.nickname.isEmpty()) comments << c;
            }
        }
        qDebug() << "[qskmusic] ncm comments:" << id << site
                 << "total=" << total << "n=" << comments.size()
                 << (err.isEmpty() ? QStringLiteral("ok") : err);
        Q_EMIT commentsDone(id, comments, total, err);
    }).detach();
}

void NcmApi::lyricAsync(qint64 songId)
{
    std::thread([this, songId]() {
        std::string json;
        const auto st = m_client.lyric(songId, json);
        QString lrc, err;
        if (st != ncm::Status::Ok) {
            err = errText(st, json);
        } else {
            const auto o = QJsonDocument::fromJson(
                QByteArray::fromStdString(json)).object();
            const auto lv = o.value("lrc");
            if (lv.isObject())
                lrc = lv.toObject().value("lyric").toString();
            else if (lv.isString())
                lrc = lv.toString();
            if (lrc.isEmpty())
                lrc = o.value("lyric").toString();
        }
        qDebug() << "[qskmusic] ncm lyric:" << songId
                 << (err.isEmpty() ? QStringLiteral("ok") : err);
        Q_EMIT lyricDone(songId, lrc, err);
    }).detach();
}

/* ---- YouTube Music（外部 yt-dlp；详见 go163-music-core/internal/ytm） ---- */

bool NcmApi::ytmAvailable()
{
    return ncm::Client::ytm_available();
}

void NcmApi::ytmSearchAsync(const QString& keywords, int limit)
{
    const std::string kw = keywords.toStdString();
    std::thread([this, kw, limit]() {
        std::string json;
        const auto st = m_client.ytm_search(kw, limit, json);
        QList<YtmSong> songs;
        QString err;
        if (st != ncm::Status::Ok) {
            err = errText(st, json);
        } else {
            const auto root = QJsonDocument::fromJson(
                QByteArray::fromStdString(json)).object();
            for (const auto& v : root.value("songs").toArray()) {
                const auto o = v.toObject();
                YtmSong s;
                s.vid = o.value("id").toString();
                s.name = o.value("name").toString();
                const auto arr = o.value("artists").toArray();
                QStringList al;
                for (const auto& a : arr) al << (a.isObject()
                    ? a.toObject().value("name").toString() : a.toString());
                s.artists = al.join(QStringLiteral("/"));
                s.durationMs = o.value("duration_ms").toInt();
                if (!s.vid.isEmpty()) songs << s;
            }
        }
        qDebug() << "[qskmusic] ytm search done:" << songs.size() << err;
        Q_EMIT ytmSearchDone(songs, err);
    }).detach();
}

void NcmApi::ytmStreamAsync(const QString& videoId)
{
    const std::string vid = videoId.toStdString();
    std::thread([this, vid, videoId]() {
        std::string json;
        const auto st = m_client.ytm_stream(vid, json);
        QString url, err;
        if (st != ncm::Status::Ok) {
            err = errText(st, json);
        } else {
            const auto o = QJsonDocument::fromJson(
                QByteArray::fromStdString(json)).object();
            url = o.value("url").toString();
            if (url.isEmpty()) err = QStringLiteral("empty url");
        }
        qDebug() << "[qskmusic] ytm stream:" << videoId << err;
        Q_EMIT ytmStreamDone(videoId, url, err);
    }).detach();
}
