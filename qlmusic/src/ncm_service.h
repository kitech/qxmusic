#ifndef NCM_SERVICE_H
#define NCM_SERVICE_H

#include "compat34.h"

#include <qevent.h>
#include <qstring.h>
#include <qobject.h>

/* 普通类型定义，避开 eventpoller 的 QEvent::User+100..113 */
const EventType34 NcmResultType = toEventType34(QEvent::User + 150);

#include <string>
#include <vector>

#ifdef NCM_ENABLE
#include "ncm.h"   /* ncm_client / NCM_* 常量 */
#else
/* libncm 不可用时占位：仅指针成员，ncm_new/free 只出现在实现内 */
struct ncm_client;
typedef struct ncm_client ncm_client;
#endif

class NcmWorker;

/* libncm 结果事件：worker 线程阻塞调用 ncm_* 后经 postEvent 回投
 * UI 线程（对齐 Api::onHttpDone 模式）。UI 绝不能直接调 ncm_*。
 * 事件类型避开 eventpoller 的 QEvent::User+100..113。 */
class NcmResultEvent : public CustomEventBase {
public:
    int kind;          /* 0=普通结果 1=need-auth */
    std::string op;    /* "top_playlists" 等，用于日志/提示 */
    int rc;            /* NCM_* 返回码 */
    int sel;           /* 请求序号（区分新/旧响应） */
    qint64 id;         /* song/playlist id 或本地 playlist id */
    std::string json;  /* 成功时的出参 JSON */
    NcmResultEvent()
        : CustomEventBase(NcmResultType), kind(0), rc(0), sel(0), id(0) {}
};

/* 任务种类 */
enum {
    NcmTaskCategories = 0,
    NcmTaskTopPlaylists,
    NcmTaskPlaylistDetail,
    NcmTaskPlaylistTracks,
    NcmTaskResolve,
    NcmTaskSongDetail,
    NcmTaskFetch,      /* 远程 URL 本地化：curl 拉缓存 → file:// 回投 */
    NcmTaskLyric,      /* 歌词：ncm_lyric（go 侧 stub 期返回 NotImplemented → UI 落空态） */
    NcmTaskArtist,     /* 艺人详情：ncm_artist_detail（头像 URL） */
    NcmTaskLikeList,   /* 账号"喜欢的音乐" id 列表（需登录） */
    NcmTaskLike,       /* 收藏/取消收藏 ncm_like */
    NcmTaskDjHot,      /* 热门电台列表（站点电台） */
    NcmTaskDjDetail,   /* 电台详情 */
    NcmTaskDjPrograms, /* 电台节目列表 */
    NcmTaskQuit,       /* 哨兵：worker 收到即退出线程 */
};

/* worker 队列元素。任务提交后字段不可变。 */
struct NcmTask {
    int kind;
    int sel;
    std::string op;
    std::string cat;
    std::string order;
    std::string ids;
    int limit;
    int offset;
    qint64 id;         /* ncm 歌单/歌曲 id */
    int localId;       /* UI 本地歌单句柄 */
    int quality;
    std::string url;   /* NcmTaskFetch：远程 URL */
    std::string extra; /* NcmTaskFetch：level 展示字段（透传 UI 状态栏） */
    std::string title;   /* NcmTaskFetch：Track.title → ID3 TIT2 + 歌名软链 */
    std::string artist;  /* NcmTaskFetch：Track.artist → ID3 TPE1 */
    std::string album;   /* NcmTaskFetch：Track.album → ID3 TALB */
    NcmTask() : kind(0), sel(0), limit(50), offset(0), id(0), localId(-1), quality(320000) {}
};

class NcmService : public QObject {
    Q_OBJECT
public:
    explicit NcmService(QObject* parent = 0);
    ~NcmService();

    /* UI 线程调用：ncm_new 是纯本地操作（无网络） */
    bool init(const QString& configDir);
    bool isAvailable() const { return m_client != 0; }

    /* 停 worker（等在途请求返回）后再 ncm_free */
    void shutdown();

    /* 投递任务；结果经 MainWindow::customEvent -> handleResult -> signals */
    void loadCategories();
    void loadTopPlaylists(const QString& cat, const QString& order, int limit, int offset);
    void loadPlaylistDetail(qint64 ncmId);
    void loadPlaylistTracks(qint64 ncmId, int localId);
    void resolveSource(qint64 songId, int quality);
    /* 纯下载：curl → .part → ID3v2.4 前置 → rename → <id>.meta sidecar
     * → 歌名软链。命中/音质判定全在 UI 层（cache-first），worker 不检测 */
    void fetchRemote(qint64 songId, const QString& url, const QString& level,
                     const QString& title, const QString& artist, const QString& album);
    void loadSongDetail(const QString& ids);
    void loadLyric(qint64 songId);   /* 歌词兜底（本地 .lrc 优先后的网络侧） */
    void loadArtist(qint64 artistId); /* 艺人详情（头像 URL） */
    void likeList();                  /* 账号"喜欢的音乐" id 列表（需登录） */
    void like(qint64 songId, bool on); /* 收藏/取消收藏（需登录） */
    void loadDjHot(int cateId, int limit, int offset);          /* 站点电台：热门电台 */
    void loadDjPrograms(qint64 rid, int limit, int offset, int asc); /* 电台节目 */

    /* MainWindow::customEvent 转发进来 */
    void handleResult(NcmResultEvent* e);

protected:
    /* postResult 投给自身，worker 结果在 UI 线程的过这里 */
    void customEvent(CustomEventBase* event);

signals:
    void categoriesLoaded(int rc, const QString& json, int sel);
    void topPlaylistsLoaded(int rc, const QString& json, int sel);
    void playlistDetailLoaded(int rc, const QString& json, qint64 ncmId);
    void playlistTracksLoaded(int rc, const QString& json, int localId);
    void sourceResolved(int rc, const QString& json, qint64 songId);
    void fetched(int rc, const QString& json, qint64 songId);
    void songDetailLoaded(int rc, const QString& json, int sel);
    void lyricLoaded(qint64 songId, const QString& json);   /* raw json；UI 侧解析 */
    void artistLoaded(qint64 artistId, int rc, const QString& json);
    void likeListLoaded(int rc, const QString& json);
    void likeDone(qint64 songId, bool on, int rc);
    void djHotLoaded(int rc, const QString& json);
    void djProgramsLoaded(int rc, const QString& json, qint64 rid);
    void authRequired(const QString& op);
    /* 下载进度（worker curl 回调节流 200ms 投递；pct 0-100） */
    void downloadProgress(qint64 songId, int pct);

private:
    void execTask(const NcmTask& t);
    void postResult(int kind, const std::string& op, int rc, int sel, qint64 id,
                    const std::string& json);
    void postAuthRequired(const std::string& op);
    void postProgress(qint64 songId, int pct);

    friend class NcmWorker;
    friend int dlProgressCb(void*, double, double, double, double);
    friend int fetchRemoteTask(const struct NcmTask& t, NcmService* svc, std::string& json);
    ncm_client* m_client;
    NcmWorker* m_worker;
    int m_seq;
};

#endif /* NCM_SERVICE_H */
