#include "ncm_service.h"

#include <qapplication.h>
#include <qthread.h>
#include <qmutex.h>
#include <qwaitcondition.h>
#include <qstring.h>

#include <deque>

#include <curl/curl.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>

#include "limelog.h"

namespace {

size_t curlWriteCb(char* ptr, size_t sz, size_t nm, void* ud)
{
    FILE* fp = (FILE*)ud;
    return fwrite(ptr, sz, nm, fp);
}

/* 远程 URL 落盘 ~/.cache/qlmusic/ncm/<song_id>.<ext>：
 *  - 同名非空缓存命中直接复用（重复播放零再下载）
 *  - curl 拉取（worker 线程串行，无并发）
 *  - 成功回 {"song_id":N,"path":"file://...","level":...}：
 *    无 ffmpeg/mpv dylib 的环境经此走本地解码链播
 *    （mp3=miniaudio 内置、m4a/aac=faad2 内嵌），mac 等零依赖可用 */
} /* namespace */

/* worker 线程：串出队列、阻塞调 ncm_*、postEvent 回投。
 * 必须在 ncm_free 前停止（ncm_free 取 apiMu）。 */
class NcmWorker : public QThread {
public:
    NcmWorker(NcmService* svc) : m_svc(svc), m_quit(false) {}
    ~NcmWorker() { stop(); wait(); }

    void stop()
    {
        m_mu.lock();
        m_quit = true;
        m_cv.wakeAll();
        m_mu.unlock();
    }

    void push(const NcmTask& t)
    {
        m_mu.lock();
        m_queue.push_back(t);
        m_cv.wakeAll();
        m_mu.unlock();
    }

protected:
    void run()
    {
        ALOG_INFO(qFromUtf8("NCM worker run start"));
        while (true) {
            m_mu.lock();
            while (m_queue.empty() && !m_quit)
                m_cv.wait(&m_mu);
            if (m_quit)
                break;
            NcmTask t = m_queue.front();
            m_queue.pop_front();
            m_mu.unlock();
            ALOG_INFO(qFromUtf8("NCM worker exec kind=%1 op=%2")
                          .arg(t.kind).arg(qFromUtf8(t.op)));
            m_svc->execTask(t);
            ALOG_INFO(qFromUtf8("NCM worker done kind=%1").arg(t.kind));
        }
        ALOG_INFO(qFromUtf8("NCM worker exit"));
        m_mu.unlock();
    }

private:
    NcmService* m_svc;
    std::deque<NcmTask> m_queue;
    QMutex m_mu;
    QWaitCondition m_cv;
    bool m_quit;
};

NcmService::NcmService(QObject* parent)
    : QObject(parent), m_client(0), m_worker(0), m_seq(0)
{
}

NcmService::~NcmService()
{
    shutdown();
    delete m_worker;
}

bool NcmService::init(const QString& configDir)
{
    if (m_client) return true;
#ifdef NCM_ENABLE
#ifdef QT3_BUILD
    m_client = ncm_new(configDir.isEmpty() ? 0 : configDir.utf8());
#else
    m_client = ncm_new(configDir.isEmpty() ? 0 : configDir.toUtf8().constData());
#endif
#endif
    if (m_client && !m_worker) {
        m_worker = new NcmWorker(this);
        m_worker->start();
    }
    return m_client != 0;
}

void NcmService::shutdown()
{
    if (m_worker) {
        m_worker->stop();   /* 消费完队列里已有任务并退出 run() */
        m_worker->wait();
    }
#ifdef NCM_ENABLE
    if (m_client) {
        ncm_free(m_client);
        m_client = 0;
    }
#endif
}

void NcmService::postResult(int kind, const std::string& op, int rc, int sel,
                            qint64 id, const std::string& json)
{
    NcmResultEvent* ev = new NcmResultEvent();
    ev->kind = kind;
    ev->op = op;
    ev->rc = rc;
    ev->sel = sel;
    ev->id = id;
    ev->json = json;
    QApplication::postEvent(this, ev);
}

void NcmService::postAuthRequired(const std::string& op)
{
    NcmResultEvent* ev = new NcmResultEvent();
    ev->kind = 1;
    ev->op = op;
    QApplication::postEvent(this, ev);
}

void NcmService::loadCategories()
{
    NcmTask t;
    t.kind = NcmTaskCategories;
    t.op = "playlist_categories";
    m_worker->push(t);
}

void NcmService::loadTopPlaylists(const QString& cat, const QString& order, int limit, int offset)
{
    NcmTask t;
    t.kind = NcmTaskTopPlaylists;
    t.op = "top_playlists";
#ifdef QT3_BUILD
    t.cat = cat.utf8();
    t.order = order.utf8();
#else
    t.cat = cat.toUtf8().constData();
    t.order = order.toUtf8().constData();
#endif
    t.limit = limit;
    t.offset = offset;
    m_worker->push(t);
}

void NcmService::loadPlaylistDetail(qint64 ncmId)
{
    NcmTask t;
    t.kind = NcmTaskPlaylistDetail;
    t.op = "playlist_detail";
    t.id = ncmId;
    m_worker->push(t);
}

void NcmService::loadPlaylistTracks(qint64 ncmId, int localId)
{
    NcmTask t;
    t.kind = NcmTaskPlaylistTracks;
    t.op = "playlist_tracks";
    t.id = ncmId;
    t.localId = localId;
    m_worker->push(t);
}

void NcmService::resolveSource(qint64 songId, int quality)
{
    NcmTask t;
    t.kind = NcmTaskResolve;
    t.op = "resolve_source";
    t.id = songId;
    t.quality = quality;
    m_worker->push(t);
}

int fetchRemoteTask(const NcmTask& t, std::string& json)
{
    const char* home = std::getenv("HOME");
    if (!home || !*home) return NCM_ERR_INTERNAL;

    /* 扩展名从 URL 路径段提取（截断 query），缺省 mp3 */
    std::string ext = "mp3";
    std::string path = t.url;
    size_t q = path.find('?');
    if (q != std::string::npos) path = path.substr(0, q);
    size_t dot = path.find_last_of('.');
    size_t sep = path.find_last_of('/');
    if (dot != std::string::npos && (sep == std::string::npos || dot > sep))
        ext = path.substr(dot + 1);

    char dir[768];
    snprintf(dir, sizeof(dir), "%s/.cache/qlmusic/ncm", home);
    char peer[768];
    snprintf(peer, sizeof(peer), "%s/.cache/qlmusic", home);
    mkdir(peer, 0755);
    mkdir(dir, 0755);
    char file[1280];
    snprintf(file, sizeof(file), "%s/%lld.%s", dir,
             (long long)t.id, ext.c_str());

    struct stat st;
    if (stat(file, &st) == 0 && st.st_size > 0) {
        char js[1400];
        snprintf(js, sizeof(js), "{\"song_id\":%lld,\"path\":\"file://%s\",\"level\":\"%s\"}",
                 (long long)t.id, file, t.extra.c_str());
        ALOG_INFO(qFromUtf8("NCM fetch cache hit %1").arg(qFromUtf8(file)));
        json = js;
        return NCM_OK;
    }

    FILE* fp = fopen(file, "wb");
    if (!fp) return NCM_ERR_INTERNAL;
    CURL* h = curl_easy_init();
    if (!h) { fclose(fp); remove(file); return NCM_ERR_INTERNAL; }
    curl_easy_setopt(h, CURLOPT_URL, t.url.c_str());
    curl_easy_setopt(h, CURLOPT_WRITEFUNCTION, curlWriteCb);
    curl_easy_setopt(h, CURLOPT_WRITEDATA, fp);
    curl_easy_setopt(h, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(h, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(h, CURLOPT_NOSIGNAL, 1L);
    CURLcode cr = curl_easy_perform(h);
    curl_easy_cleanup(h);
    long nbyte = ftell(fp);
    fclose(fp);
    if (cr != CURLE_OK || nbyte <= 0) {
        remove(file);
        ALOG_WARN(qFromUtf8("NCM fetch failed rc=%1 byte=%2")
                      .arg((int)cr).arg((int)nbyte));
        return NCM_ERR_NETWORK;
    }
    char js[1400];
    snprintf(js, sizeof(js), "{\"song_id\":%lld,\"path\":\"file://%s\",\"level\":\"%s\"}",
             (long long)t.id, file, t.extra.c_str());
    ALOG_INFO(qFromUtf8("NCM fetch ok %1 byte=%2")
                  .arg(qFromUtf8(file)).arg((int)nbyte));
    json = js;
    return NCM_OK;
}

void NcmService::fetchRemote(qint64 songId, const QString& url, const QString& level)
{
    if (!m_worker) return;
    NcmTask t;
    t.kind = NcmTaskFetch;
    t.op = "fetch";
    t.id = songId;
#ifdef QT3_BUILD
    t.url = url.utf8();
    t.extra = level.utf8();
#else
    t.url = url.toUtf8().constData();
    t.extra = level.toUtf8().constData();
#endif
    m_worker->push(t);
}

void NcmService::loadSongDetail(const QString& ids)
{
    NcmTask t;
    t.kind = NcmTaskSongDetail;
    t.op = "song_detail";
#ifdef QT3_BUILD
    t.ids = ids.utf8();
#else
    t.ids = ids.toUtf8().constData();
#endif
    m_worker->push(t);
}

void NcmService::execTask(const NcmTask& t)
{
    if (!m_client) return;
#ifdef NCM_ENABLE
    int rc = NCM_ERR_INTERNAL;
    std::string json;
    /* playlist_tracks 的回投 id 用 UI 本地歌单句柄，
     * 其余任务回投 ncm id（resolve_source=song id） */
    qint64 id = (t.kind == NcmTaskPlaylistTracks) ? (qint64)t.localId : t.id;

    switch (t.kind) {
    case NcmTaskCategories: {
        char* out = 0;
        rc = ncm_playlist_categories(m_client, &out);
        if (rc == NCM_OK && out) json = out;
        if (out) ncm_free_string(out);
        break;
    }
    case NcmTaskTopPlaylists: {
        char* out = 0;
        rc = ncm_top_playlists(m_client, t.cat.c_str(), t.order.c_str(),
                               t.limit, t.offset, &out);
        if (rc == NCM_OK && out) json = out;
        if (out) ncm_free_string(out);
        break;
    }
    case NcmTaskPlaylistDetail: {
        char* out = 0;
        rc = ncm_playlist_detail(m_client, t.id, &out);
        if (rc == NCM_OK && out) json = out;
        if (out) ncm_free_string(out);
        break;
    }
    case NcmTaskPlaylistTracks: {
        char* out = 0;
        rc = ncm_playlist_tracks(m_client, t.id, &out);
        if (rc == NCM_OK && out) json = out;
        if (out) ncm_free_string(out);
        break;
    }
    case NcmTaskResolve: {
        char* out = 0;
        rc = ncm_resolve_source(m_client, t.id, t.quality, &out);
        if (rc == NCM_OK && out) json = out;
        if (out) ncm_free_string(out);
        break;
    }
    case NcmTaskSongDetail: {
        char* out = 0;
        rc = ncm_song_detail(m_client, t.ids.c_str(), &out);
        if (rc == NCM_OK && out) json = out;
        if (out) ncm_free_string(out);
        break;
    }
    case NcmTaskFetch: {
        /* 下载不依赖 ncm client（fetchRemoteTask 内自管 curl） */
        rc = fetchRemoteTask(t, json);
        break;
    }
    default:
        return;
    }

    if (rc == NCM_ERR_UNAUTHORIZED) {
        ALOG_WARN(qFromUtf8("NCM exec op=%1 rc=%2 needAuth").arg(qFromUtf8(t.op)).arg(rc));
        postAuthRequired(t.op);
        postResult(0, t.op, rc, t.sel, id, std::string());
    } else {
        ALOG_INFO(qFromUtf8("NCM exec op=%1 rc=%2 result=%3")
                      .arg(qFromUtf8(t.op)).arg(rc)
                      .arg(qFromUtf8(json).left(400)));
        if (rc != NCM_OK)
            ALOG_WARN(qFromUtf8("NCM exec op=%1 rc=%2 len=%3")
                          .arg(qFromUtf8(t.op)).arg(rc).arg((int)json.size()));
        postResult(0, t.op, rc, t.sel, id, json);
    }
#else
    Q_UNUSED(t);
#endif
}

void NcmService::customEvent(CustomEventBase* event)
{
    if (event->type() == NcmResultType)
        handleResult(static_cast<NcmResultEvent*>(event));
    else
        QObject::customEvent(event);
}

void NcmService::handleResult(NcmResultEvent* e)
{
    ALOG_INFO(qFromUtf8("NCM result op=%1 rc=%2 len=%3")
                  .arg(qFromUtf8(e->op)).arg(e->rc).arg((int)e->json.size()));
    if (e->kind == 1) {
        emit authRequired(qFromUtf8(e->op));
        return;
    }
    QString json = qFromUtf8(e->json);
    const QString op = qFromUtf8(e->op);
    if (op == qFromUtf8("playlist_categories")) {
        emit categoriesLoaded(e->rc, json, e->sel);
        return;
    }
    if (op == qFromUtf8("top_playlists")) {
        emit topPlaylistsLoaded(e->rc, json, e->sel);
        return;
    }
    if (op == qFromUtf8("playlist_detail")) {
        emit playlistDetailLoaded(e->rc, json, e->id);
        return;
    }
    if (op == qFromUtf8("playlist_tracks")) {
        emit playlistTracksLoaded(e->rc, json, (int)e->id);
        return;
    }
    if (op == qFromUtf8("resolve_source")) {
        emit sourceResolved(e->rc, json, e->id);
        return;
    }
    if (op == qFromUtf8("fetch")) {
        emit fetched(e->rc, json, e->id);
        return;
    }
    if (op == qFromUtf8("song_detail")) {
        emit songDetailLoaded(e->rc, json, e->sel);
        return;
    }
}

