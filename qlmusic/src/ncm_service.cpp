#include "ncm_service.h"

#include <qapplication.h>
#include <qthread.h>
#include <qmutex.h>
#include <qwaitcondition.h>
#include <qstring.h>

#include <deque>

#include "limelog.h"

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
    if (op == qFromUtf8("song_detail")) {
        emit songDetailLoaded(e->rc, json, e->sel);
        return;
    }
}

