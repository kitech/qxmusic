#include "favoritestore.h"

#include "cJSON.h"

#include <cstdio>
#include <cstring>
#include <ctime>

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

FavoriteStore::FavoriteStore(const std::string& dir)
    : m_path(dir + "/favorites.json")
{
}

void FavoriteStore::load()
{
    m_items.clear();
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
    if (!root || root->type != cJSON_Array) {
        cJSON_Delete(root);
        return;
    }
    const cJSON* it = 0;
    cJSON_ArrayForEach(it, root) {
        FavItem v;
        v.ncmId = numOf(it, "ncmId");
        if (v.ncmId <= 0)
            continue;   // 脏数据跳过
        v.name = strOf(it, "name");
        v.artists = strOf(it, "artists");
        v.album = strOf(it, "album");
        v.coverUrl = strOf(it, "coverUrl");
        v.durationMs = numOf(it, "durationMs");
        v.playable = numOf(it, "playable") != 0 ? 1 : 0;
        v.addedAt = numOf(it, "addedAt");
        v.fromAccount = numOf(it, "fromAccount") != 0 ? 1 : 0;
        m_items.push_back(v);
    }
    cJSON_Delete(root);
}

bool FavoriteStore::save() const
{
    cJSON* root = cJSON_CreateArray();
    if (!root)
        return false;
    for (size_t i = 0; i < m_items.size(); ++i) {
        const FavItem& v = m_items[i];
        cJSON* o = cJSON_CreateObject();
        if (!o)
            continue;
        cJSON_AddNumberToObject(o, "ncmId", (double)v.ncmId);
        cJSON_AddStringToObject(o, "name", v.name.c_str());
        cJSON_AddStringToObject(o, "artists", v.artists.c_str());
        cJSON_AddStringToObject(o, "album", v.album.c_str());
        cJSON_AddStringToObject(o, "coverUrl", v.coverUrl.c_str());
        cJSON_AddNumberToObject(o, "durationMs", (double)v.durationMs);
        cJSON_AddNumberToObject(o, "playable", v.playable);
        cJSON_AddNumberToObject(o, "addedAt", (double)v.addedAt);
        cJSON_AddNumberToObject(o, "fromAccount", v.fromAccount);
        cJSON_AddItemToArray(root, o);
    }
    char* s = cJSON_Print(root);
    cJSON_Delete(root);
    if (!s)
        return false;

    /* .part + rename：与 iconcachecore 同范式（断电不留半截文件） */
    const std::string part = m_path + ".part";
    FILE* f = fopen(part.c_str(), "wb");
    if (!f) {
        cJSON_free(s);
        return false;
    }
    const size_t len = std::strlen(s);
    const bool ok = fwrite(s, 1, len, f) == len;
    fclose(f);
    cJSON_free(s);
    if (!ok) {
        ::remove(part.c_str());
        return false;
    }
    return rename(part.c_str(), m_path.c_str()) == 0;
}

bool FavoriteStore::add(const FavItem& item)
{
    if (item.ncmId <= 0 || contains(item.ncmId))
        return false;
    FavItem v = item;
    if (v.addedAt == 0)
        v.addedAt = (long long)time(0);
    m_items.push_back(v);
    return true;
}

bool FavoriteStore::remove(long long ncmId)
{
    for (std::vector<FavItem>::iterator it = m_items.begin();
         it != m_items.end(); ++it) {
        if (it->ncmId == ncmId) {
            m_items.erase(it);
            return true;
        }
    }
    return false;
}

const FavItem* FavoriteStore::find(long long ncmId) const
{
    for (size_t i = 0; i < m_items.size(); ++i)
        if (m_items[i].ncmId == ncmId)
            return &m_items[i];
    return 0;
}

bool FavoriteStore::contains(long long ncmId) const
{
    return find(ncmId) != 0;
}

void FavoriteStore::setFromAccount(long long ncmId, bool fromAccount)
{
    for (size_t i = 0; i < m_items.size(); ++i) {
        if (m_items[i].ncmId == ncmId) {
            m_items[i].fromAccount = fromAccount ? 1 : 0;
            return;
        }
    }
}

} // namespace musiccom
