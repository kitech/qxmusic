#ifndef NCM_API_H
#define NCM_API_H

#include <QObject>
#include <QString>
#include <QList>
#include <QMap>
#include <QHash>
#include <QMutex>

#include "ncm.hpp"

// 播放音质（NCM_QUALITY_EXHIGH）
constexpr int NCM_Q_EXHIGH = 320000;

struct Song
{
    qint64 id = 0;
    QString name;
    QString artists;
    QList<qint64> artistIds;   // 与 artists 同序（ar[].id）
    QString album;
    QString cover;      // 专辑封面 URL（libncm 歌曲 JSON 的 cover）
    qint64 albumId = 0; // 专辑 id（兜底图标 seed / 去重预留）
    int durationMs = 0;
    bool playable = true;
    qint64 publishMs = 0;   // NCM 专辑上架时间 al.publishTime（0=无）
};

/* YouTube Music 曲目：id 是 videoId 字符串（无法映射 qint64，独立形状） */
struct YtmSong
{
    QString vid;         // videoId（取流用）
    QString name;
    QString artists;
    int durationMs = 0;
};

/*
 * libncm 的 Qt 薄封装（单例）。
 * - 网络调用阻塞（Go 内部 HTTP），一律 std::thread 工作线程执行后直接
 *   Q_EMIT（Auto 连接跨线程自动排队，Qt6 自动注册参数元类型）。
 * - ABI：单客户端内部串行化；事件回调在 Go goroutine 触发，
 *   经 QMetaObject::invokeMethod 队列回主线程再发信号。
 */
class NcmApi : public QObject
{
    Q_OBJECT
public:
    static NcmApi& instance();

    QString version() const;

    void searchAsync(const QString& keywords, int limit);
    void resolveAsync(qint64 songId, int quality);
    void resolveForDownloadAsync(qint64 songId);   // 仅取 URL（不触发播放）
    void lyricAsync(qint64 songId);
    void artistAsync(qint64 artistId);   // 艺人详情（头像 URL）
    void artistSongsAsync(qint64 artistId, int limit = 50, int offset = 0,
                          const QString& order = QStringLiteral("hot"));   // 歌手歌曲
    void likeListAsync();                // 账号"喜欢的音乐" id 列表（需登录）
    void likeAsync(qint64 songId, bool on);   // 收藏/取消收藏（需登录）
    void songDetailAsync(const QString& ids); // 批量歌曲详情（逗号分隔 id）

    /* ---- 站点切换："ncm"(默认) / "bili"：解析/搜索按站点分流到对应 libncm 接口 ---- */
    void setSite(const QString& site) { m_site = site; }
    QString site() const { return m_site; }
    /* 下载头（resolve 出参 headers；Referer/UA/Cookie…，NCM 与 B站同契约） */
    QMap<QString, QString> streamHeaders(qint64 songId) const {
        QMutexLocker lk(const_cast<QMutex*>(&m_hm));
        return m_headers.value(songId);
    }

    /* ---- YouTube Music（外部 yt-dlp + 代理，默认 http://127.0.0.1:10809）---- */
    static bool ytmAvailable();              // yt-dlp 在 PATH？
    void ytmSearchAsync(const QString& keywords, int limit);
    void ytmStreamAsync(const QString& videoId);

Q_SIGNALS:
    void searchDone(const QList<Song>& songs, const QString& err);
    void resolveDone(qint64 songId, const QString& url, const QString& err);
    void downloadUrlDone(qint64 songId, const QString& url, const QString& err);
    void lyricDone(qint64 songId, const QString& lrc, const QString& err);
    void artistDone(qint64 artistId, const QString& picUrl, const QString& err);
    void artistSongsDone(qint64 artistId, QList<Song> songs, const QString& err);   // 歌手歌曲
    void likeListDone(const QList<qint64>& ids, const QString& err);
    void likeDone(qint64 songId, bool on, const QString& err);
    void songDetailDone(const QList<Song>& songs, const QString& err);
    void playFailReceived(qint64 songId, const QString& reason);

    void ytmSearchDone(const QList<YtmSong>& songs, const QString& err);
    void ytmStreamDone(const QString& videoId, const QString& url, const QString& err);

private:
    explicit NcmApi(QObject* parent = nullptr);

    ncm::Client m_client;
    QString m_site = QStringLiteral("ncm");
    mutable QMutex m_hm;
    QHash<qint64, QMap<QString, QString>> m_headers;
};

#endif
