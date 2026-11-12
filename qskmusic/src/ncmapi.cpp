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
        const auto st = m_client.search(kw, limit, json);
        QList<Song> songs;
        QString err;
        if (st != ncm::Status::Ok) {
            err = statusText(st);
        } else {
            songs = parseSongs(QByteArray::fromStdString(json));
        }
        qDebug() << "[qskmusic] ncm search done:" << songs.size() << err;
        Q_EMIT searchDone(songs, err);
    }).detach();
}

void NcmApi::resolveAsync(qint64 songId, int quality)
{
    std::thread([this, songId, quality]() {
        std::string json;
        const auto st = m_client.resolve_source(songId,
            static_cast<ncm::Quality>(quality), json);
        QString url, err;
        if (st != ncm::Status::Ok) {
            err = statusText(st);
        } else {
            const auto o = QJsonDocument::fromJson(
                QByteArray::fromStdString(json)).object();
            url = o.value("url").toString();
            if (url.isEmpty())
                err = QStringLiteral("no url");
        }
        qDebug() << "[qskmusic] ncm resolve:" << songId
                 << (err.isEmpty() ? QStringLiteral("ok") : err);
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
            err = statusText(st);
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
            err = statusText(st);
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

void NcmApi::lyricAsync(qint64 songId)
{
    std::thread([this, songId]() {
        std::string json;
        const auto st = m_client.lyric(songId, json);
        QString lrc, err;
        if (st != ncm::Status::Ok) {
            err = statusText(st);
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
