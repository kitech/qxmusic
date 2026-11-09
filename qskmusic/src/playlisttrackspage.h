#ifndef PLAYLIST_TRACKS_PAGE_H
#define PLAYLIST_TRACKS_PAGE_H

#include "page.h"
#include "ncmapi.h"

class PlayerBar;
class QskLinearBox;
class QskTextLabel;

/*
 * 歌单曲目页（qlmusic onNcmPlaylistTracks 移植）：
 *   launchArgs: {"playlistId": qint64, "title": 歌单名}
 * 数据：NcmCatalog::playlistTracksDone（Song 沿用 ncmapi 类型）；
 * 点曲：NcmApi::resolveAsync（仅调用）→ AudioPlayer 播放，队列=整张歌单。
 */
class PlaylistTracksPage : public Page
{
    Q_OBJECT
public:
    explicit PlaylistTracksPage(QQuickItem* parent = nullptr);

protected:
    void onCreate(const QVariantMap& launchArgs,
                  const QVariantMap& savedState) override;
    void onResume() override;

private:
    void buildRows();
    void playSong(int index);
    qint64 currentSongId() const;

    qint64 m_playlistId = 0;
    QString m_title;
    QList<Song> m_songs;
    int m_current = -1;
    bool m_resolving = false;
    QskLinearBox* m_rows = nullptr;
    QskTextLabel* m_header = nullptr;
    QskTextLabel* m_status = nullptr;
    PlayerBar* m_playerBar = nullptr;
};

#endif
