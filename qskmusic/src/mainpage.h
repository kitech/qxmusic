#ifndef MAIN_PAGE_H
#define MAIN_PAGE_H

#include "page.h"

class PlayerBar;
class QskLinearBox;
class QskScrollArea;
class QueuePopup;

/*
 * 音乐主页初版（qlmusic mainwindow 的 qsk 化骨架，桩数据）：
 *   顶栏（标题 + 日志/设置/关于入口）
 *   主体（左侧导航桩 + 右侧歌曲列表桩）
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
    void buildSongList();
    void openQueue();

    QskLinearBox* m_songRows = nullptr;
    PlayerBar* m_playerBar = nullptr;
    QueuePopup* m_queuePopup = nullptr;
    QStringList m_titles;
    int m_currentTrack = -1;
    bool m_playing = false;
};

#endif
