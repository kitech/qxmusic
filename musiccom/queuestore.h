// musiccom/queuestore：qlmusic / qskmusic 共享的播放队列持久化
// queue.json 完整队列快照 + 当前播放下标 → 重启恢复（摆队列不自动播）。
// Qt 无关（std + cJSON），Qt3/4/6 两端均可链接；cJSON 取 qlcomp/cJSON.{c,h}。
#ifndef MUSICCOM_QUEUESTORE_H
#define MUSICCOM_QUEUESTORE_H

#include <string>
#include <vector>
#include <ctime>

namespace musiccom {

struct QueueItem {
    long long id;             // 取链键：ql Track.ncmId / qsk Song.id（B站=aid 等）
    std::string name;
    std::string artists;      // "a/b" 串（两端现有格式）
    std::string album;
    std::string coverUrl;
    long long durationMs;
    long long albumId;        // 兜底图标 seed / 去重预留
    int playable;             // 0/1
    std::string url;          // 非数值取链键曲目备用（ql ytm:"<videoId>" / 本地路径）

    QueueItem()
        : id(0), durationMs(0), albumId(0), playable(1) {}
};

class QueueStore {
public:
    // dir：ql=~/.local/share/qlmusic，qsk=收藏同目录；文件名固定 queue.json
    explicit QueueStore(const std::string& dir);

    void load();              // 启动调用；文件缺失/损坏 → 空表（不炸）
    bool save() const;        // .part + rename 原子写（与 FavoriteStore 同范式）

    void set(const std::vector<QueueItem>& items, int current)
    {
        m_items = items;
        m_current = current;
    }

    const std::vector<QueueItem>& items() const { return m_items; }
    int current() const { return m_current; }   // -1 = 无正在播放（未开始）

    void clear() { m_items.clear(); m_current = -1; }

private:
    std::string m_path;
    std::vector<QueueItem> m_items;
    int m_current;
};

} // namespace musiccom

#endif
