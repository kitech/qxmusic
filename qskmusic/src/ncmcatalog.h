#ifndef NCM_CATALOG_H
#define NCM_CATALOG_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QList>

#include "ncmapi.h"   // Song（仅复用类型，不改动 ncmapi）
#include "ncm.hpp"

struct CategoryGroup
{
    QString name;
    QStringList categories;
};

struct PlaylistCard
{
    qint64 id = 0;
    QString name;
    QString creator;
    QString cover;
    int trackCount = 0;
    qint64 playCount = 0;
};

struct PlaylistInfo
{
    qint64 id = 0;
    QString name;
    QString creator;
    QString cover;
    int trackCount = 0;
    QList<Song> songs;
};

/* 站点电台（djradio）：电台 + 节目 */
struct DjRadio
{
    qint64 id = 0;
    QString name;
    QString cover;
    QString desc;
    QString nickname;
    int programCount = 0;
};

struct DjProgram
{
    qint64 id = 0;
    qint64 mainSongId = 0;   // 交给 resolve_source 取流（mp3Url 已废弃）
    QString name;
    QString cover;
    int durationMs = 0;
};

/*
 * 分类/歌单目录客户端（qlmusic NcmService 的 qskmusic 移植版）：
 * 直接用 libncm 的 C++ RAII 封装（ncm.hpp）调
 *   ncm_playlist_categories / ncm_top_playlists / ncm_playlist_tracks
 * 独立 ncm client（与 NcmApi 同 config 目录，共享登录态）；
 * 工作线程 + 跨线程信号（与 NcmApi 同模式）。
 */
class NcmCatalog : public QObject
{
    Q_OBJECT
public:
    static NcmCatalog& instance();

    void loadCategoriesAsync();
    void loadTopPlaylistsAsync(const QString& cat, const QString& order,
                               int limit, int offset);
    void loadPlaylistTracksAsync(qint64 playlistId);
    /* 站点电台：热门电台 + 节目列表 */
    void loadDjHotAsync(int cateId, int limit, int offset);
    void loadDjProgramsAsync(qint64 rid, int limit, int offset);

Q_SIGNALS:
    void categoriesDone(const QStringList& hot,
                        const QList<CategoryGroup>& groups, const QString& err);
    void topPlaylistsDone(const QList<PlaylistCard>& cards,
                          bool more, const QString& err);
    void playlistTracksDone(const PlaylistInfo& playlist, const QString& err);
    void djHotDone(const QList<DjRadio>& radios, const QString& err);
    void djProgramsDone(qint64 rid, const QList<DjProgram>& programs,
                        const QString& err);

private:
    explicit NcmCatalog(QObject* parent = nullptr);

    ncm::Client m_client;
};

#endif
