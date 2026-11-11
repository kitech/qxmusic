#ifndef PLAYLIST_TRACKS_PAGE_H
#define PLAYLIST_TRACKS_PAGE_H

#include "page.h"
#include "ncmapi.h"
#include <QPair>

class QskLinearBox;
class QskTextLabel;
class StatusLine;

/*
 * 歌单曲目页（qlmusic onNcmPlaylistTracks + playAll 移植）：
 *   launchArgs: {"playlistId": qint64, "title": 歌单名}
 * 数据：NcmCatalog::playlistTracksDone；点曲 → Playback::setQueue(m_songs, i)；
 * 信息行「共 N 首 + ▶ 播放全部」（qlmusic contentview playAll 同义）；
 * 行长按 → 下载菜单（手动下载通道）。
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
    int songRowAt(const QPointF& localPos) const;   // 非曲目行=-1
    void requestDownload(int index);
    void updatePlayingMarker();                     // ▶ 标识刷新（不重建列表）

    qint64 m_playlistId = 0;
    QString m_title;
    QList<Song> m_songs;
    qint64 m_pendingDlId = 0;
    QList<QPair<qint64, QskTextLabel*>> m_playingMarkers;   // songId → 标识列
    QskLinearBox* m_rows = nullptr;
    QskTextLabel* m_header = nullptr;
    QskTextLabel* m_count = nullptr;
    StatusLine* m_status = nullptr;
};

#endif
