#include "favorites.h"

#include <QStandardPaths>
#include <QDir>
#include <QDebug>
#include <QSet>

Favorites& Favorites::instance()
{
    static Favorites s;
    return s;
}

Favorites::Favorites(QObject* parent)
    : QObject(parent)
{
}

void Favorites::init(const QString& dir)
{
    if (m_ready)
        return;
    m_ready = true;
    QDir().mkpath(dir);
    m_store = new musiccom::FavoriteStore(dir.toStdString());
    m_store->load();
    qDebug() << "[FAV] loaded" << m_store->all().size() << "from" << dir;

    // 登录态同步：失败（未登录/网络）静默跳过，本地功能不受影响
    auto& api = NcmApi::instance();
    connect(&api, &NcmApi::likeListDone, this,
        [this](const QList<qint64>& ids, const QString& err) {
            if (!err.isEmpty()) {
                qDebug() << "[FAV] like_list skip:" << err;
                return;
            }
            mergeAccountIds(ids);
        });
    // 账号有、本地无 → 拉详情补快照
    connect(&api, &NcmApi::songDetailDone, this,
        [this](const QList<Song>& songs, const QString& err) {
            if (!err.isEmpty()) {
                qDebug() << "[FAV] song_detail fail:" << err;
                return;
            }
            bool dirty = false;
            for (const auto& s : songs) {
                if (m_store->contains(s.id))
                    continue;
                musiccom::FavItem v = toItem(s);
                v.fromAccount = 1;
                if (m_store->add(v))
                    dirty = true;
            }
            if (dirty) {
                m_store->save();
                Q_EMIT changed();
            }
        });
    // 本地有、账号无 → 推送到账号（失败保留本地，下次启动重试）
    connect(&api, &NcmApi::likeDone, this,
        [this](qint64 songId, bool on, const QString& err) {
            if (!err.isEmpty())
                qDebug() << "[FAV] like push fail:" << songId << err;
        });

    api.likeListAsync();
}

void Favorites::mergeAccountIds(const QList<qint64>& ids)
{
    QSet<qint64> account;
    for (auto id : ids)
        account.insert(id);

    bool dirty = false;
    // 本地 → 账号：只推送"用户主动收藏过"的项（fromAccount=0）
    for (const auto& f : m_store->all()) {
        if (account.contains(f.ncmId))
            continue;
        if (f.fromAccount)
            continue;   // 账号同步来的项不回推，避免已取消的复活
        NcmApi::instance().likeAsync(f.ncmId, true);
    }
    // 账号 → 本地：已存在则标记来源；缺快照的批量拉详情
    QStringList missing;
    for (auto id : ids) {
        if (m_store->contains(id)) {
            m_store->setFromAccount(id, true);
            dirty = true;
        } else {
            missing << QString::number(id);
        }
    }
    if (dirty)
        m_store->save();
    if (!missing.isEmpty())
        NcmApi::instance().songDetailAsync(missing.join(QLatin1Char(',')));
    else
        Q_EMIT changed();
}

bool Favorites::contains(qint64 ncmId) const
{
    return m_store && m_store->contains(ncmId);
}

void Favorites::toggle(const Song& song)
{
    if (!m_store || song.id <= 0)
        return;
    if (m_store->contains(song.id)) {
        m_store->remove(song.id);
        m_store->save();
        NcmApi::instance().likeAsync(song.id, false);
        Q_EMIT changed();
        return;
    }
    m_store->add(toItem(song));
    m_store->save();
    NcmApi::instance().likeAsync(song.id, true);
    Q_EMIT changed();
}

QList<Song> Favorites::songs() const
{
    QList<Song> out;
    if (!m_store)
        return out;
    for (const auto& f : m_store->all())
        out.append(toSong(f));
    return out;
}

bool Favorites::isFromAccount(qint64 ncmId) const
{
    if (!m_store)
        return false;
    const musiccom::FavItem* f = m_store->find(ncmId);
    return f && f->fromAccount;
}

musiccom::FavItem Favorites::toItem(const Song& s)
{
    musiccom::FavItem v;
    v.ncmId = s.id;
    v.name = s.name.toStdString();
    v.artists = s.artists.toStdString();
    v.album = s.album.toStdString();
    v.coverUrl = s.cover.toStdString();
    v.durationMs = s.durationMs;
    v.playable = s.playable ? 1 : 0;
    return v;
}

Song Favorites::toSong(const musiccom::FavItem& f)
{
    Song s;
    s.id = f.ncmId;
    s.name = QString::fromStdString(f.name);
    s.artists = QString::fromStdString(f.artists);
    s.album = QString::fromStdString(f.album);
    s.cover = QString::fromStdString(f.coverUrl);
    s.durationMs = (int)f.durationMs;
    s.playable = f.playable != 0;
    return s;
}
