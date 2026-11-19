#ifndef SONGDETAILPAGE_H
#define SONGDETAILPAGE_H

#include "page.h"
#include "ncmapi.h"

class QskLinearBox;
class QskTextLabel;
class QskPushButton;
class IconItem;

/*
 * 歌曲详情页（qlmusic SongDetailMode 的 qsk 等价物）：
 *   launchArgs: {"songId": qint64}
 *   内容：大封面、歌名、歌手（可点 → artist 页）、专辑/时长、歌词；
 *   返回：页头 ←（finish() → PageManager::back）
 */
class SongDetailPage : public Page
{
    Q_OBJECT
public:
    explicit SongDetailPage(QQuickItem* parent = nullptr);

protected:
    void onCreate(const QVariantMap& launchArgs,
                  const QVariantMap& savedState) override;
    void onDestroy() override;

private:
    void applySong(const Song& s);
    void updateStats();

    qint64 m_songId = 0;
    qint64 m_artistId = 0;
    QString m_artistName;
    IconItem* m_cover = nullptr;
    QskTextLabel* m_title = nullptr;
    QskPushButton* m_artistBtn = nullptr;
    QskTextLabel* m_meta = nullptr;
    QskTextLabel* m_lyric = nullptr;
    QskTextLabel* m_stats = nullptr;      /* ▷ -- + 真实评论数 */
    QskPushButton* m_commentBtn = nullptr;/* 评论区 → comments 页 */
    int m_commentCount = -1;              /* -1=未加载，>=0=已回填 */
    bool m_destroyed = false;
};

#endif // SONGDETAILPAGE_H
