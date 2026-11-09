#ifndef PLAYLIST_TRACKS_PAGE_H
#define PLAYLIST_TRACKS_PAGE_H

#include "page.h"
#include "ncmapi.h"

class QskLinearBox;
class QskTextLabel;

/*
 * 歌单曲目页（qlmusic onNcmPlaylistTracks 移植）：
 *   launchArgs: {"playlistId": qint64, "title": 歌单名}
 * 数据：NcmCatalog::playlistTracksDone；点曲 → Playback::setQueue(m_songs, i)
 * （播放/进度/自动下一首全在全局 Playback + 全局 PlayerBar）。
 */
class PlaylistTracksPage : public Page
{
    Q_OBJECT
public:
    explicit PlaylistTracksPage(QQuickItem* parent = nullptr);

protected:
    void onCreate(const QVariantMap& launchArgs,
                  const QVariantMap& savedState) override;

private:
    void buildRows();

    qint64 m_playlistId = 0;
    QString m_title;
    QList<Song> m_songs;
    QskLinearBox* m_rows = nullptr;
    QskTextLabel* m_header = nullptr;
    QskTextLabel* m_status = nullptr;
};

#endif
