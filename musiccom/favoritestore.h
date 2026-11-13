// musiccom/favoritestore：qlmusic / qskmusic 共享的本地收藏存储
// favorites.json 完整元数据快照 → 未登录/离线也能显示收藏列表。
// Qt 无关（std + cJSON），Qt3/4/6 两端均可链接；cJSON 取 qlcomp/cJSON.{c,h}。
#ifndef MUSICCOM_FAVORITESTORE_H
#define MUSICCOM_FAVORITESTORE_H

#include <string>
#include <vector>

namespace musiccom {

struct FavItem {
    long long ncmId;
    std::string name;
    std::string artists;      // "a/b" 串（两端现有格式）
    std::string album;
    std::string coverUrl;
    long long durationMs;
    int playable;             // 0/1
    long long addedAt;        // unix 秒
    int fromAccount;          // 1 = 由账号同步并入（界面徽标用）

    FavItem()
        : ncmId(0), durationMs(0), playable(1), addedAt(0), fromAccount(0) {}
};

class FavoriteStore {
public:
    // dir：ql=~/.local/share/qlmusic，qsk=AppLocalData；文件名固定 favorites.json
    explicit FavoriteStore(const std::string& dir);

    void load();              // 启动调用；文件缺失/损坏 → 空表（不炸）
    bool save() const;        // .part + rename 原子写

    bool add(const FavItem& item);   // 已存在（ncmId 相同）→ false 且不改动
    bool remove(long long ncmId);
    bool contains(long long ncmId) const;
    const FavItem* find(long long ncmId) const;
    void setFromAccount(long long ncmId, bool fromAccount);

    void clear() { m_items.clear(); }
    const std::vector<FavItem>& all() const { return m_items; }

private:
    std::string m_path;
    std::vector<FavItem> m_items;
};

} // namespace musiccom

#endif // MUSICCOM_FAVORITESTORE_H
