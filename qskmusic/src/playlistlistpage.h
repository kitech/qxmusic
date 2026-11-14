#ifndef PLAYLIST_LIST_PAGE_H
#define PLAYLIST_LIST_PAGE_H

#include "page.h"
#include "ncmcatalog.h"

class PlaylistListView;
class QskLinearBox;
class StatusLine;
class QskPushButton;

/*
 * 某分类下的歌单列表页（qlmusic ContentView::showPlaylistList 移植）：
 *   launchArgs: {"cat": 分类名(空=热门), "title": 页标题}
 *   offset 分页（30/次）+ more 标志 + 加载更多按钮；点歌单 push 曲目页。
 */
class PlaylistListPage : public Page
{
    Q_OBJECT
public:
    explicit PlaylistListPage(QQuickItem* parent = nullptr);

protected:
    void onCreate(const QVariantMap& launchArgs,
                  const QVariantMap& savedState) override;

private:
    void requestPage(int offset);
    void rebuildCards();
    int cardRowAt(const QPointF& localPos) const;   // 非卡片行=-1

    QString m_cat;
    QString m_title;
    bool m_radio = false;    /* launchArgs.radio：站点电台模式（数据源 dj_hot） */
    bool m_more = false;
    bool m_loading = false;
    QList<PlaylistCard> m_cards;
    QskLinearBox* m_cardRows = nullptr;
    StatusLine* m_status = nullptr;
    QskPushButton* m_moreBtn = nullptr;
};

#endif
