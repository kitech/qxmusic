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
    for (const auto& a : o.value("artists").toArray()) {
        if (!artists.isEmpty()) artists += QStringLiteral("/");
        artists += a.isObject() ? a.toObject().value("name").toString()
                                : a.toString();
    }
    s.artists = artists;
    s.album = o.value("album").toString();
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
