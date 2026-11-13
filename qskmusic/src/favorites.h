#ifndef FAVORITES_H
#define FAVORITES_H

#include <QObject>
#include <QList>
#include <QString>

#include "ncmapi.h"
#include "favoritestore.h"

/*
 * 本地收藏 + 账号收藏统一门面（qskmusic）。
 * - 本地：musiccom::FavoriteStore（favorites.json，未登录/离线可用）
 * - 账号：libncm ncm_like_list / ncm_like（需登录 cookie）
 * 写入语义：本地永远写；登录时双写。启动登录态则双向合并：
 *   账号有本地无 → 拉歌曲详情并入本地（fromAccount=1）
 *   本地有账号无 → ncm_like(1) 推送
 */
class Favorites : public QObject
{
    Q_OBJECT
public:
    static Favorites& instance();

    void init(const QString& dir);   // main.cpp 启动调用一次（加载 + 登录同步）

    bool contains(qint64 ncmId) const;
    // 曲目菜单"收藏/取消收藏"入口：本地切换 + 登录时双写
    void toggle(const Song& song);

    // 合并单页数据（本地快照顺序；fromAccount 项带标记）
    QList<Song> songs() const;
    bool isFromAccount(qint64 ncmId) const;

Q_SIGNALS:
    void changed();

private:
    explicit Favorites(QObject* parent = nullptr);

    void syncAccount();                       // 登录态双向合并
    void mergeAccountIds(const QList<qint64>& ids);
    static musiccom::FavItem toItem(const Song& s);
    static Song toSong(const musiccom::FavItem& f);

    musiccom::FavoriteStore* m_store = nullptr;
    bool m_ready = false;
};

#endif
