#ifndef MAIN_PAGE_H
#define MAIN_PAGE_H

#include "page.h"
#include "ncmapi.h"
#include <QPointF>

class CategoryView;
class NavBar;
class LyricPopup;
class PlayerBar;
class RecView;
class QskLinearBox;
class QskScrollArea;
class QskStackBox;
class QskTextField;
class QueuePopup;

/*
 * 音乐主页初版（qlmusic mainwindow 的 qsk 化骨架，桩数据）：
 *   顶栏（标题 + ☰ 菜单：日志/设置/关于）
 *   主体（宽屏 nav rail / 窄屏底部导航，5 个 tab 切换）
 *   底部播放条（PlayerBar）
 */
class MainPage : public Page
{
    Q_OBJECT
public:
    MainPage(QQuickItem* parent = nullptr);

protected:
    void onCreate(const QVariantMap& launchArgs,
                  const QVariantMap& savedState) override;

private:
    void showOptionsMenu(const QPointF& origin);
    void buildStubList(QskLinearBox* rows);
    void openQueue();
    void switchTab(int index);
    void playSong(int index);
    void openLyric();
    qint64 currentSongId() const;
    QString currentTitle() const;

    QskStackBox* m_contentStack = nullptr;
    NavBar* m_navBottom = nullptr;
    QList<QQuickItem*> m_tabViews;   // tab0=推荐，tab1=分类，tab2=我的（滚动视图）
    RecView* m_recView = nullptr;
    CategoryView* m_categoryView = nullptr;
    PlayerBar* m_playerBar = nullptr;
    QueuePopup* m_queuePopup = nullptr;
    LyricPopup* m_lyricPopup = nullptr;
    QList<Song> m_songs;
    QStringList m_titles;
    int m_currentTrack = -1;
    bool m_playing = false;
    bool m_resolving = false;
    qint64 m_lrcSongId = 0;
    QString m_lrcText;
};

#endif
