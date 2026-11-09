#ifndef REC_VIEW_H
#define REC_VIEW_H

#include <QskLinearBox.h>

#include "ncmcatalog.h"

class QskTextLabel;
class QskPushButton;

/*
 * 「推荐」tab（qlmusic 启动 requestTopPlaylists("__hot__",0) 的移植）：
 *   构造即拉热门歌单（cat="全部", order="hot", 30/页），
 *   卡片列表 + 「加载更多」；点歌单发 playlistSelected 由宿主 push 曲目页。
 */
class RecView : public QskLinearBox
{
    Q_OBJECT
public:
    explicit RecView(QQuickItem* parent = nullptr);

    void reload();

Q_SIGNALS:
    void playlistSelected(qint64 playlistId, const QString& name);

private:
    void requestPage(int offset);
    void rebuildCards();

    bool m_loading = false;
    bool m_more = false;
    QList<PlaylistCard> m_cards;
    QskLinearBox* m_cardRows = nullptr;
    QskTextLabel* m_status = nullptr;
    QskPushButton* m_moreBtn = nullptr;
};

#endif
