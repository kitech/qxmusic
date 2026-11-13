#ifndef FAVORITES_PAGE_H
#define FAVORITES_PAGE_H

#include "page.h"
#include "ncmapi.h"
#include <QPair>

class QskLinearBox;
class QskTextLabel;
class StatusLine;

/*
 * 我的收藏页（本地 favorites.json 快照 + 账号同步并入项）：
 *   点曲 → Playback::setQueue(songs, i)；播放全部；
 *   长按 → 取消收藏（本地移除 + 登录时 ncm_like(0)）。
 * 数据由 Favorites 单例提供，changed 信号驱动重建。
 */
class FavoritesPage : public Page
{
    Q_OBJECT
public:
    explicit FavoritesPage(QQuickItem* parent = nullptr);

protected:
    void onCreate(const QVariantMap& launchArgs,
                  const QVariantMap& savedState) override;

private:
    void reload();
    void buildRows();
    int songRowAt(const QPointF& localPos) const;
    void updatePlayingMarker();

    QList<Song> m_songs;
    QList<QPair<qint64, QskTextLabel*>> m_playingMarkers;
    QskLinearBox* m_rows = nullptr;
    QskTextLabel* m_count = nullptr;
    StatusLine* m_status = nullptr;
};

#endif
