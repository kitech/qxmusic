#include "queuestore.h"

#include "cJSON.h"
#include "storeutil.h"

#include <cstdio>
#include <cstring>

namespace musiccom {

namespace {

std::string strOf(const cJSON* o, const char* key)
{
    const cJSON* v = cJSON_GetObjectItemCaseSensitive(o, key);
    if (v && v->type == cJSON_String && v->valuestring)
        return std::string(v->valuestring);
    return std::string();
}

long long numOf(const cJSON* o, const char* key)
{
    const cJSON* v = cJSON_GetObjectItemCaseSensitive(o, key);
    if (v && v->type == cJSON_Number)
        return (long long)v->valuedouble;
    return 0;
}

} // namespace

QueueStore::QueueStore(const std::string& dir)
    : m_path(dir + "/queue.json")
    , m_current(-1)
{
}

void QueueStore::load()
{
    m_items.clear();
    m_current = -1;
    ::remove((m_path + ".part").c_str());   /* 历史固定名 .part 残留清理 */
    FILE* f = fopen(m_path.c_str(), "rb");
    if (!f)
        return;
    std::string buf;
    char tmp[4096];
    size_t n;
    while ((n = fread(tmp, 1, sizeof(tmp), f)) > 0)
        buf.append(tmp, n);
    fclose(f);

    cJSON* root = cJSON_Parse(buf.c_str());
    if (!root || root->type != cJSON_Object) {
        cJSON_Delete(root);
        return;
    }
    m_current = (int)numOf(root, "current");
    const cJSON* arr = cJSON_GetObjectItemCaseSensitive(root, "queue");
    const cJSON* it = 0;
    cJSON_ArrayForEach(it, arr) {
        QueueItem v;
        v.id = numOf(it, "id");
        v.url = strOf(it, "url");
        if (v.id <= 0 && v.url.empty())
            continue;   // 脏数据跳过
        v.name = strOf(it, "name");
        v.artists = strOf(it, "artists");
        v.album = strOf(it, "album");
        v.coverUrl = strOf(it, "coverUrl");
        v.durationMs = numOf(it, "durationMs");
        v.albumId = numOf(it, "albumId");
        v.playable = numOf(it, "playable") != 0 ? 1 : 0;
        m_items.push_back(v);
    }
    cJSON_Delete(root);

    // 防御：current 越界（文件被改/队列缩短）→ 摆队列但不指当前
    if (m_current >= (int)m_items.size())
        m_current = m_items.empty() ? -1 : (int)m_items.size() - 1;
}

bool QueueStore::save() const
{
    cJSON* root = cJSON_CreateObject();
    if (!root)
        return false;
    cJSON_AddNumberToObject(root, "version", 1);
    cJSON_AddNumberToObject(root, "savedAt", (double)time(0));
    cJSON_AddNumberToObject(root, "current", m_current);
    cJSON* arr = cJSON_CreateArray();
    for (size_t i = 0; i < m_items.size(); ++i) {
        const QueueItem& v = m_items[i];
        cJSON* o = cJSON_CreateObject();
        if (!o)
            continue;
        cJSON_AddNumberToObject(o, "id", (double)v.id);
        cJSON_AddStringToObject(o, "name", v.name.c_str());
        cJSON_AddStringToObject(o, "artists", v.artists.c_str());
        cJSON_AddStringToObject(o, "album", v.album.c_str());
        cJSON_AddStringToObject(o, "coverUrl", v.coverUrl.c_str());
        cJSON_AddNumberToObject(o, "durationMs", (double)v.durationMs);
        cJSON_AddNumberToObject(o, "albumId", (double)v.albumId);
        cJSON_AddNumberToObject(o, "playable", v.playable);
        if (!v.url.empty())
            cJSON_AddStringToObject(o, "url", v.url.c_str());
        cJSON_AddItemToArray(arr, o);
    }
    cJSON_AddItemToObject(root, "queue", arr);
    char* s = cJSON_Print(root);
    cJSON_Delete(root);
    if (!s)
        return false;

    /* .part.<pid> + rename：多实例并发保存互不踩（进程独占临时名） */
    const std::string part = storeutil::uniquePartPath(m_path);
    FILE* f = fopen(part.c_str(), "wb");
    if (!f) {
        storeutil::logSaveError("queue-store", "fopen(.part)", part);
        cJSON_free(s);
        return false;
    }
    const size_t len = std::strlen(s);
    const bool ok = fwrite(s, 1, len, f) == len;
    fclose(f);
    cJSON_free(s);
    if (!ok) {
        storeutil::logSaveError("queue-store", "fwrite", part);
        ::remove(part.c_str());
        return false;
    }
    if (rename(part.c_str(), m_path.c_str()) != 0) {
        storeutil::logSaveError("queue-store", "rename", part + " -> " + m_path);
        return false;
    }
    return true;
}

} // namespace musiccom
