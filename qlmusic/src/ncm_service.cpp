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

#ifdef HAVE_LIBID3TAG
#include <id3tag.h>   /* 系统 libid3tag 0.16.x（pkg-config 探测链接） */
#endif

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

#ifdef HAVE_LIBID3TAG

/* 提取一个文本帧（UTF-8 出参；缺帧/空帧 → false） */
static bool readId3TextField(struct id3_tag* tag, const char* fid, std::string& out)
{
    struct id3_frame const* fr = id3_tag_findframe(tag, fid, 0);
    if (!fr) return false;
    union id3_field const* fld = id3_frame_field(fr, 1);
    if (!fld || id3_field_getnstrings(fld) == 0) return false;
    id3_ucs4_t const* u4 = id3_field_getstrings(fld, 0);
    if (!u4) return false;
    id3_utf8_t* u8 = id3_ucs4_utf8duplicate(u4);
    if (!u8) return false;
    out.assign((const char*)u8);
    free(u8);
    return true;
}

/* 新文本帧：fid + UTF-8 值；enc 按 tag 版本分支（v2.3 → UTF_16 合法） */
static bool attachTextFrame(struct id3_tag* tag, const char* fid,
                            const std::string& val, enum id3_field_textencoding enc)
{
    struct id3_frame* nf = id3_frame_new(fid);
    if (!nf) return false;
    id3_ucs4_t* u4 = id3_utf8_ucs4duplicate((id3_utf8_t const*)val.c_str());
    union id3_field* fld = id3_frame_field(nf, 1);
    bool ok = (u4 && fld && id3_field_addstring(fld, u4) == 0 &&
               id3_field_settextencoding(id3_frame_field(nf, 0), enc) == 0);
    if (u4) free(u4);
    if (ok) {
        id3_tag_attachframe(tag, nf);
    } else {
        id3_frame_delete(nf);
    }
    return ok;
}

/* ID3 tag 处理（系统 libid3tag）：
 * ① 先提取 TIT2/TPE1/TALB 现值并日志（可能为空）
 * ② 不全则只补缺失字段（已有保留）：编码版本分支 v2.4→UTF_8 /
 *    v2.3→UTF_16（库 render 自动转换）/ v2.2 老 tag 仅日志跳写
 * ③ 有补写 → id3_file_update；全齐 → 跳写
 * 返回 true=流程完成（含 skip）；false=文件级失败（调用方删 part） */
static bool updateNcmId3(const char* path, const std::string& title,
                         const std::string& artist, const std::string& album)
{
    static const char* ids[3]  = { "TIT2", "TPE1", "TALB" };
    const std::string* mine[3] = { &title, &artist, &album };
    std::string cur[3];
    bool wrote = false, created = false;
    int nframes = 0;

    struct id3_file* f = id3_file_open(path, ID3_FILE_MODE_READWRITE);
    struct id3_tag* tag = f ? id3_file_tag(f) : 0;
    if (tag)
        for (int i = 0; i < 3; ++i)
            if (!readId3TextField(tag, ids[i], cur[i])) cur[i].clear();
    ALOG_INFO(qFromUtf8("NCM tag extracted title=%1 artist=%2 album=%3")
                  .arg(qFromUtf8(cur[0]))
                  .arg(qFromUtf8(cur[1]))
                  .arg(qFromUtf8(cur[2])));

    unsigned ver = 0;
    if (tag) ver = id3_tag_version(tag) / 256;   /* 编码值 0x0400 → major=4 */
    if (tag && ver >= 3) {
        for (int i = 0; i < 3; ++i) {
            if (cur[i].empty() && !mine[i]->empty()) {
                enum id3_field_textencoding enc =
                    (ver >= 4) ? ID3_FIELD_TEXTENCODING_UTF_8
                               : ID3_FIELD_TEXTENCODING_UTF_16;
                if (attachTextFrame(tag, ids[i], *mine[i], enc)) wrote = true;
            }
        }
        if (wrote && id3_file_update(f) != 0) {
            ALOG_WARN(qFromUtf8("NCM tag update write failed"));
            wrote = false;
        }
    }
    if (f) id3_file_close(f);

    if (!tag) {
        /* 无既有 tag（源净流）：全新渲染 + tag+原字节手拼前缀。
         * 帧编码 UTF_16（v2.3/v2.4 双合法，库 render 自动转换） */
        struct id3_tag* nt = id3_tag_new();
        for (int i = 0; nt && i < 3; ++i)
            if (!mine[i]->empty() &&
                attachTextFrame(nt, ids[i], *mine[i],
                                ID3_FIELD_TEXTENCODING_UTF_16))
                ++nframes;
        if (nt && nframes > 0) {
            char buf[2048];
            id3_length_t len =
                id3_tag_render(nt, (id3_byte_t*)buf);
            if (len > 0 && len < sizeof(buf)) {
                std::string tmp = std::string(path) + ".id3tmp";
                FILE* wp = fopen(tmp.c_str(), "wb");
                FILE* rp = fopen(path, "rb");
                if (wp && rp) {
                    bool ok = fwrite(buf, 1, len, wp) == len;
                    char cp[65536];
                    size_t n;
                    while (ok && (n = fread(cp, 1, sizeof(cp), rp)) > 0)
                        if (fwrite(cp, 1, n, wp) != n) ok = false;
                    fclose(rp);
                    if (fclose(wp) != 0) ok = false;
                    if (ok && rename(tmp.c_str(), path) == 0) {
                        created = true;
                    } else {
                        remove(tmp.c_str());
                    }
                } else {
                    if (wp) fclose(wp);
                    if (rp) fclose(rp);
                }
            } else if (len == 0) {
                ALOG_WARN(qFromUtf8("NCM tag render empty, skip create"));
            } else {
                ALOG_WARN(qFromUtf8("NCM tag render too large, skip"));
            }
        }
        if (nt) id3_tag_delete(nt);
    }

    if (created) {
        ALOG_INFO(qFromUtf8("NCM tag created frames=%1").arg(nframes));
    } else if (wrote) {
        ALOG_INFO(qFromUtf8("NCM tag updated"));
    } else if (tag) {
        ALOG_INFO(qFromUtf8("NCM tag complete, skip update"));
    } else {
        ALOG_INFO(qFromUtf8("NCM tag absent, create skipped"));
    }
    return true;
}

#else /* !HAVE_LIBID3TAG：无系统库（mac 等）tag 段降级跳写 */

static bool updateNcmId3(const char*, const std::string&,
                         const std::string&, const std::string&)
{
    ALOG_INFO(qFromUtf8("NCM tag libid3tag not linked, skip update"));
    return true;
}

#endif /* HAVE_LIBID3TAG */

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

    long long fsize = 0;
    if (ext == "mp3") {
        /* 先提取+日志、不全则补缺（系统 libid3tag；无库时内部 skip） */
        if (!updateNcmId3(part, t.title, t.artist, t.album)) {
            remove(part);
            ALOG_WARN(qFromUtf8("NCM fetch id3 failed"));
            return NCM_ERR_INTERNAL;
        }
    } else {
        ALOG_INFO(qFromUtf8("NCM tag skip (not mp3: %1)").arg(qFromUtf8(ext)));
    }
    if (rename(part, file) != 0) {
        remove(part);
        ALOG_WARN(qFromUtf8("NCM fetch rename failed"));
        return NCM_ERR_INTERNAL;
    }
    struct stat st3;
    fsize = (stat(file, &st3) == 0) ? (long long)st3.st_size : 0;
    writeSidecar(dir, t.id, t.extra, fsize);   /* 含 tag 的最终 size（bug2 修复） */
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

