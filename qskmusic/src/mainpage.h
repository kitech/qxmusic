#ifndef MAIN_PAGE_H
#define MAIN_PAGE_H

#include "page.h"
#include "ncmcatalog.h"

class QskLinearBox;
class QskTextLabel;
class QskPushButton;

/*
 * 首页（anystik 主页骨架 + qlmusic 启动节奏）：
 *   顶栏 = 标题 + ☰（QskMenu：分类/搜索/日志/设置/关于）；
 *   内容 = 热门歌单列表（构造即拉 = qlmusic requestTopPlaylists(__hot__,0)）
 *          + 加载更多（offset 分页）；
 *   点歌单 push 曲目页。
 */
class HomePage : public Page
{
    Q_OBJECT
public:
    explicit HomePage(QQuickItem* parent = nullptr);

protected:
    void onCreate(const QVariantMap& launchArgs,
                  const QVariantMap& savedState) override;

private:
    void showOptionsMenu(const QPointF& origin);
    void requestPage(int offset);
    void rebuildCards();

    QskLinearBox* m_cardRows = nullptr;
    QskTextLabel* m_status = nullptr;
    QskPushButton* m_moreBtn = nullptr;
    QList<PlaylistCard> m_cards;
    bool m_loading = false;
    bool m_more = false;
};

#endif
