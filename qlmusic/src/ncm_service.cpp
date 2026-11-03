#include "ncm_service.h"

#include <qapplication.h>
#include <qthread.h>
#include <qmutex.h>
#include <qwaitcondition.h>
#include <qstring.h>

#include <deque>

#include <curl/curl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include <cstring>

#include "limelog.h"

namespace {

size_t curlWriteCb(char* ptr, size_t sz, size_t nm, void* ud)
{
    FILE* fp = (FILE*)ud;
    return fwrite(ptr, sz, nm, fp);
}

/* 缓存目录布局（业界 cache-first + sidecar 模式）：
 *  <song_id>.<ext>       实体音频（ext ∈ mp3/flac/m4a/aac）
 *  <song_id>.meta        sidecar {"level":"exhigh","size":N}（零网判命中据此）
 *  <song_id>.<ext>.part  下载中临时，完成后原子落位
 *  songs/<净化歌名>.<ext> 软链 → ../<song_id>.<ext>（人名可浏览）
 * 文件名不带比特率/音质后缀（切音质由 sidecar 比对判定） */

/* 软链文件名净化：非法/控制字符 → '_'；空名 fallback song_<id>；截 90 字符 */
static std::string sanitizeName(const std::string& raw, long long id)
{
    std::string s;
    s.reserve(raw.size());
    for (size_t i = 0; i < raw.size(); ++i) {
        unsigned char c = (unsigned char)raw[i];
        bool bad = (c > 0 && c < 0x20) || c == 0x7f ||
                   strchr("/\\:*?\"<>|", c) != 0;
        s.push_back(bad ? '_' : (char)c);
    }
    size_t b = s.find_first_not_of(" \t");
    size_t e = s.find_last_not_of(" \t");
    s = (b == std::string::npos) ? std::string() : s.substr(b, e - b + 1);
    if (s.empty()) {
        char f[64];
        snprintf(f, sizeof(f), "song_%lld", id);
        s = f;
    }
    if (s.size() > 90) s = s.substr(0, 90);
    return s;
}

/* ID3v2.4 writer：10 字节 tag 头 + TIT2/TPE1/TALB 文本帧（UTF-8，syncsafe）。
 * 元数据全空 → 空串（调用方 no-op） */
static std::string makeId3Tag(const std::string& title, const std::string& artist,
                              const std::string& album)
{
    static const char* ids[3] = { "TIT2", "TPE1", "TALB" };
    const std::string* vals[3] = { &title, &artist, &album };
    std::string body, out;
    char fr[10];
    unsigned cnt = 0;
    for (int i = 0; i < 3; ++i) {
        const std::string& v = *vals[i];
        if (v.empty()) continue;
        unsigned fsize = (unsigned)(v.size() + 1); /* 内容 = encoding byte + utf8 */
        memcpy(fr, ids[i], 4);
        fr[4] = 0;
        fr[5] = (char)((fsize >> 21) & 0x7f);
        fr[6] = (char)((fsize >> 14) & 0x7f);
        fr[7] = (char)((fsize >> 7) & 0x7f);
        fr[8] = (char)(fsize & 0x7f);
        fr[9] = 0;                                 /* flags */
        body.append(fr, 10);
        body.push_back((char)0x03);                /* encoding: UTF-8 */
        body.append(v);
        ++cnt;
    }
    if (cnt == 0) return std::string();
    unsigned tagSize = (unsigned)body.size();      /* 不含 10 字节头 */
    char hdr[10];
    memcpy(hdr, "ID3", 3);
    hdr[3] = 4; hdr[4] = 0;                        /* ver 2.4 rev 0 */
    hdr[5] = 0;                                    /* flags: 无 */
    hdr[6] = (char)((tagSize >> 21) & 0x7f);
    hdr[7] = (char)((tagSize >> 14) & 0x7f);
    hdr[8] = (char)((tagSize >> 7) & 0x7f);
    hdr[9] = (char)(tagSize & 0x7f);
    out.append(hdr, 10);
    out.append(body);
    return out;
}

/* 两段式原子落位：<final> = tag 头 + <part> 原字节，写入后调用方 remove(part)。
 * 源自带 "ID3" 魔数（或非 mp3）→ 原样拷贝不写 tag；失败删 <final> 返回 false */
static bool writeWithId3(const char* part, const char* finalPath,
                         const std::string& title, const std::string& artist,
                         const std::string& album, bool isMp3)
{
    std::string tag;
    if (isMp3) {
        FILE* rp0 = fopen(part, "rb");
        if (rp0) {
            char magic[3];
            if (fread(magic, 1, 3, rp0) == 3 && memcmp(magic, "ID3", 3) == 0)
                ALOG_INFO(qFromUtf8("NCM fetch keep source ID3 tag"));
            else
                tag = makeId3Tag(title, artist, album);
            fclose(rp0);
        }
    }
    FILE* rp = fopen(part, "rb");
    if (!rp) return false;
    FILE* wp = fopen(finalPath, "wb");
    if (!wp) { fclose(rp); return false; }
    bool ok = true;
    if (!tag.empty() && fwrite(tag.data(), 1, tag.size(), wp) != tag.size()) ok = false;
    char buf[65536];
    size_t n;
    while (ok && (n = fread(buf, 1, sizeof(buf), rp)) > 0)
        if (fwrite(buf, 1, n, wp) != n) ok = false;
    fclose(rp);
    if (fclose(wp) != 0) ok = false;
    if (!ok) remove(finalPath);
    return ok;
}

/* 歌名软链 songs/<名>.<ext> → ../<id>.<ext>（best-effort，失败静默）：
 * 链已存在且指向不同实体（同名不同歌）→ 尾缀 _<id>；每次刷新到最新实体 */
static void ensureTitleSymlink(const char* dir, long long id,
                               const std::string& title, const std::string& ext,
                               const char* file)
{
    std::string songs = std::string(dir) + "/songs";
    mkdir(songs.c_str(), 0755);
    std::string name = sanitizeName(title, id);
    std::string target = std::string("../") + (strrchr(file, '/') + 1);
    std::string linkPath = songs + "/" + name + "." + ext;
    struct stat lst, fst;
    if (lstat(linkPath.c_str(), &lst) == 0 && stat(linkPath.c_str(), &fst) == 0) {
        struct stat me;
        if (stat(file, &me) == 0 &&
            (me.st_dev != fst.st_dev || me.st_ino != fst.st_ino))
            linkPath = songs + "/" + name + "_" + std::to_string(id) + "." + ext;
    }
    unlink(linkPath.c_str());                      /* 存在则刷新指向 */
    if (symlink(target.c_str(), linkPath.c_str()) != 0) return;
    ALOG_INFO(qFromUtf8("NCM fetch symlink %1").arg(qFromUtf8(linkPath.c_str())));
}

/* sidecar：<id>.meta {"level":"...","size":N} —— UI cache-first 零网判命中依据 */
static void writeSidecar(const char* dir, long long id, const std::string& level,
                         long long size)
{
    if (level.empty()) return;
    char p[800];
    snprintf(p, sizeof(p), "%s/%lld.meta", dir, id);
    FILE* fp = fopen(p, "wb");
    if (!fp) return;
    fprintf(fp, "{\"level\":\"%s\",\"size\":%lld}", level.c_str(), size);
    fclose(fp);
}
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
    char part[1300];
    snprintf(part, sizeof(part), "%s.part", file);

    /* 命中/音质判定已上移 UI 层（cache-first，sidecar 比对）；worker 纯下载 */
    FILE* fp = fopen(part, "wb");
    if (!fp) return NCM_ERR_INTERNAL;
    CURL* h = curl_easy_init();
    if (!h) { fclose(fp); remove(part); return NCM_ERR_INTERNAL; }
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
        remove(part);
        ALOG_WARN(qFromUtf8("NCM fetch failed rc=%1 byte=%2")
                      .arg((int)cr).arg((int)nbyte));
        return NCM_ERR_NETWORK;
    }

    struct stat st2;
    long long fsize = (stat(part, &st2) == 0) ? (long long)st2.st_size : nbyte;
    if (!writeWithId3(part, file, t.title, t.artist, t.album, ext == "mp3")) {
        remove(part);
        ALOG_WARN(qFromUtf8("NCM fetch finalize failed"));
        return NCM_ERR_INTERNAL;
    }
    remove(part);
    writeSidecar(dir, t.id, t.extra, fsize);
    ensureTitleSymlink(dir, t.id, t.title, ext, file);

    char js[1400];
    snprintf(js, sizeof(js), "{\"song_id\":%lld,\"path\":\"file://%s\",\"level\":\"%s\"}",
             (long long)t.id, file, t.extra.c_str());
    ALOG_INFO(qFromUtf8("NCM fetch ok %1 byte=%2")
                  .arg(qFromUtf8(file)).arg((int)fsize));
    json = js;
    return NCM_OK;
}

void NcmService::fetchRemote(qint64 songId, const QString& url, const QString& level,
                             const QString& title, const QString& artist, const QString& album)
{
    if (!m_worker) return;
    NcmTask t;
    t.kind = NcmTaskFetch;
    t.op = "fetch";
    t.id = songId;
#ifdef QT3_BUILD
    t.url = url.utf8();
    t.extra = level.utf8();
    t.title = title.utf8();
    t.artist = artist.utf8();
    t.album = album.utf8();
#else
    t.url = url.toUtf8().constData();
    t.extra = level.toUtf8().constData();
    t.title = title.toUtf8().constData();
    t.artist = artist.toUtf8().constData();
    t.album = album.toUtf8().constData();
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

