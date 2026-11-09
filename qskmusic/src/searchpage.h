#ifndef SEARCH_PAGE_H
#define SEARCH_PAGE_H

#include "page.h"
#include "ncmapi.h"

class PlayerBar;
class QskLinearBox;
class QskTextLabel;
class QskTextField;

/*
 * 搜索页（从主界面 tab 移出，qlmusic 侧栏搜索的次要入口）：
 *   顶栏返回 + 搜索行 + 结果列表 + 播放条；
 *   打开时自动搜默认词（保留原搜索能力，不占首屏）；
 *   点曲 → NcmApi::resolveAsync → AudioPlayer 播放，队列 = 搜索结果。
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
    void playSong(int index);
    qint64 currentSongId() const;

    QskTextField* m_field = nullptr;
    QskTextLabel* m_status = nullptr;
    QskLinearBox* m_rows = nullptr;
    PlayerBar* m_playerBar = nullptr;
    QList<Song> m_songs;
    int m_current = -1;
    bool m_resolving = false;
};

#endif
