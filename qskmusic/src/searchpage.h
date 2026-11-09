#ifndef SEARCH_PAGE_H
#define SEARCH_PAGE_H

#include "page.h"
#include "ncmapi.h"

class QskLinearBox;
class QskTextLabel;
class QskTextField;

/*
 * 搜索页（qlmusic 侧栏搜索的次要入口）：
 *   顶栏返回 + 搜索行 + 结果列表；
 *   打开时自动搜默认词（保留原搜索能力，不占首屏）；
 *   点曲 → Playback::setQueue(m_songs, i)（播放由全局 Playback + PlayerBar）。
 */
class SearchPage : public Page
{
    Q_OBJECT
public:
    explicit SearchPage(QQuickItem* parent = nullptr);

protected:
    void onCreate(const QVariantMap& launchArgs,
                  const QVariantMap& savedState) override;

private:
    void rebuildList();

    QskTextField* m_field = nullptr;
    QskTextLabel* m_status = nullptr;
    QskLinearBox* m_rows = nullptr;
    QList<Song> m_songs;
};

#endif
