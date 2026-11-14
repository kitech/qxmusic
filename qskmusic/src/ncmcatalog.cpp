#include "ncmcatalog.h"
#include <QStandardPaths>
#include <QDir>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDebug>
#include <thread>

namespace {

// 与 ncmapi.cpp 的 parseSong 同口径（不改 ncmapi，独立一份）
Song parseSong(const QJsonObject& o)
{
    Song s;
    s.id = o.value("id").toVariant().toLongLong();
    s.name = o.value("name").toString();
    QString artists;
    QList<qint64> ids;
    for (const auto& a : o.value("artists").toArray()) {
        if (!artists.isEmpty()) artists += QStringLiteral("/");
        artists += a.isObject() ? a.toObject().value("name").toString()
                                : a.toString();
    }
    for (const auto& v : o.value("artist_ids").toArray()) {
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

} // namespace

NcmCatalog& NcmCatalog::instance()
{
    static NcmCatalog s;
    return s;
}

NcmCatalog::NcmCatalog(QObject* parent)
    : QObject(parent)
    , m_client((QStandardPaths::writableLocation(
          QStandardPaths::AppLocalDataLocation)
          + QStringLiteral("/ncm")).toStdString())
{
}

void NcmCatalog::loadCategoriesAsync()
{
    std::thread([this]() {
        std::string json;
        const auto st = m_client.playlist_categories(json);
        QStringList hot;
        QList<CategoryGroup> groups;
        QString err;
        if (st != ncm::Status::Ok) {
            err = QString::fromStdString(std::string(ncm::to_string(st)));
        } else {
            const auto root = QJsonDocument::fromJson(
                QByteArray::fromStdString(json)).object();
            for (const auto& v : root.value("hot").toArray())
                hot << v.toString();
            for (const auto& gv : root.value("groups").toArray()) {
                const auto go = gv.toObject();
                CategoryGroup g;
                g.name = go.value("name").toString();
                for (const auto& cv : go.value("categories").toArray())
                    g.categories << cv.toString();
                if (!g.name.isEmpty() && !g.categories.isEmpty())
                    groups << g;
            }
        }
        qDebug() << "[qskmusic] ncm categories:" << hot.size()
                 << groups.size() << err;
        Q_EMIT categoriesDone(hot, groups, err);
    }).detach();
}

void NcmCatalog::loadTopPlaylistsAsync(const QString& cat,
    const QString& order, int limit, int offset)
{
    qDebug() << "[qsk] ncm req top_playlists cat=" << cat
             << "order=" << order << "limit=" << limit << "offset=" << offset;
    const std::string c = cat.toStdString();
    const std::string o = order.toStdString();
    std::thread([this, c, o, limit, offset]() {
        std::string json;
        const auto st = m_client.top_playlists(c, o, limit, offset, json);
        QList<PlaylistCard> cards;
        bool more = false;
        QString err;
        if (st != ncm::Status::Ok) {
            err = QString::fromStdString(std::string(ncm::to_string(st)));
        } else {
            const auto root = QJsonDocument::fromJson(
                QByteArray::fromStdString(json)).object();
            more = root.value("more").toBool(false);
            for (const auto& pv : root.value("playlists").toArray()) {
                const auto po = pv.toObject();
                PlaylistCard card;
                card.id = po.value("id").toVariant().toLongLong();
                card.name = po.value("name").toString();
                card.creator = po.value("creator").toString();
                card.cover = po.value("cover").toString();
                card.trackCount = po.value("track_count").toInt();
                card.playCount = po.value("play_count").toVariant().toLongLong();
                cards << card;
            }
        }
        qDebug() << "[qskmusic] ncm top playlists:" << cards.size()
                 << "more=" << more << err;
        Q_EMIT topPlaylistsDone(cards, more, err);
    }).detach();
}

void NcmCatalog::loadPlaylistTracksAsync(qint64 playlistId)
{
    qDebug() << "[qsk] ncm req playlist_tracks id=" << playlistId;
    std::thread([this, playlistId]() {
        std::string json;
        const auto st = m_client.playlist_tracks(playlistId, json);
        PlaylistInfo info;
        info.id = playlistId;
        QString err;
        if (st != ncm::Status::Ok) {
            err = QString::fromStdString(std::string(ncm::to_string(st)));
        } else {
            const auto root = QJsonDocument::fromJson(
                QByteArray::fromStdString(json)).object();
            const auto pl = root.value("playlist").toObject();
            info.name = pl.value("name").toString();
            info.creator = pl.value("creator").toString();
            info.cover = pl.value("cover").toString();
            info.trackCount = pl.value("track_count").toInt();
            for (const auto& tv : root.value("tracks").toArray())
                info.songs << parseSong(tv.toObject());
        }
        qDebug() << "[qskmusic] ncm playlist tracks:" << playlistId
                 << info.songs.size() << err;
        Q_EMIT playlistTracksDone(info, err);
    }).detach();
}

void NcmCatalog::loadDjHotAsync(int cateId, int limit, int offset)
{
    std::thread([this, cateId, limit, offset]() {
        std::string json;
        const auto st = m_client.dj_hot(cateId, limit, offset, json);
        QList<DjRadio> radios;
        QString err;
        if (st != ncm::Status::Ok) {
            err = QString::fromStdString(std::string(ncm::to_string(st)));
        } else {
            const auto o = QJsonDocument::fromJson(
                QByteArray::fromStdString(json)).object();
            for (const auto& v : o.value("radios").toArray()) {
                const auto r = v.toObject();
                DjRadio d;
                d.id = r.value("id").toVariant().toLongLong();
                d.name = r.value("name").toString();
                d.cover = r.value("cover").toString();
                d.desc = r.value("desc").toString();
                d.nickname = r.value("nickname").toString();
                d.programCount = r.value("program_count").toInt();
                radios << d;
            }
        }
        qDebug() << "[qskmusic] ncm dj hot:" << radios.size() << err;
        Q_EMIT djHotDone(radios, err);
    }).detach();
}

void NcmCatalog::loadDjProgramsAsync(qint64 rid, int limit, int offset)
{
    std::thread([this, rid, limit, offset]() {
        std::string json;
        const auto st = m_client.dj_programs(rid, limit, offset, 0, json);
        QList<DjProgram> programs;
        QString err;
        if (st != ncm::Status::Ok) {
            err = QString::fromStdString(std::string(ncm::to_string(st)));
        } else {
            const auto o = QJsonDocument::fromJson(
                QByteArray::fromStdString(json)).object();
            for (const auto& v : o.value("programs").toArray()) {
                const auto p = v.toObject();
                DjProgram d;
                d.id = p.value("id").toVariant().toLongLong();
                d.mainSongId = p.value("main_song_id").toVariant().toLongLong();
                d.name = p.value("name").toString();
                d.cover = p.value("cover").toString();
                d.durationMs = p.value("duration_ms").toInt();
                programs << d;
            }
        }
        qDebug() << "[qskmusic] ncm dj programs:" << rid << programs.size() << err;
        Q_EMIT djProgramsDone(rid, programs, err);
    }).detach();
}
