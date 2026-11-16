#ifndef ARTISTPAGE_H
#define ARTISTPAGE_H

#include "page.h"
#include "ncmapi.h"

class QskLinearBox;
class QskTextLabel;
class IconItem;

/*
 * 歌手详情页（qlmusic ArtistMode 的 qsk 等价物）：
 *   launchArgs: {"artistId": qint64, "name": QString}
 *   数据：NcmApi::artistAsync（头像）+ artistSongsAsync（热门 50 首）；
 *   点行 → Playback::setQueue(整个列表, 行号)；
 *   返回：页头 ←（finish()）。
 */
class ArtistPage : public Page
{
    Q_OBJECT
public:
    explicit ArtistPage(QQuickItem* parent = nullptr);

protected:
    void onCreate(const QVariantMap& launchArgs,
                  const QVariantMap& savedState) override;
    void onDestroy() override;

private:
    void buildRows();
    void showLoading(bool on, const QString& err = QString());

    qint64 m_artistId = 0;
    QString m_name;
    QList<Song> m_songs;
    QskLinearBox* m_rows = nullptr;
    QskTextLabel* m_status = nullptr;
    IconItem* m_avatar = nullptr;
    bool m_destroyed = false;
};

#endif // ARTISTPAGE_H
