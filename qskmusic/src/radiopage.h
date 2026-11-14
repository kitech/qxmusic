#ifndef RADIO_PAGE_H
#define RADIO_PAGE_H

#include "page.h"
#include "ncmapi.h"
#include <QPair>

class QskLinearBox;
class QskTextLabel;
class StatusLine;

/*
 * 站点电台 · 节目列表页（qlmusic onNcmDjPrograms 移植）：
 *   launchArgs: {"rid": qint64, "title": 页标题}
 * 数据：NcmCatalog::loadDjProgramsAsync → DjProgram
 * 播放：mainSongId → Song.id → Playback（复用现有 resolve 链路）；
 *       无 mainSong 的节目为灰态（点击提示无音源、不提供下载）。
 */
class RadioPage : public Page
{
    Q_OBJECT
public:
    explicit RadioPage(QQuickItem* parent = nullptr);

protected:
    void onCreate(const QVariantMap& launchArgs,
                  const QVariantMap& savedState) override;

private:
    void buildRows();
    int songRowAt(const QPointF& localPos) const;
    void requestDownload(int index);
    void updatePlayingMarker();

    qint64 m_rid = 0;
    QString m_title;
    QList<Song> m_songs;
    qint64 m_pendingDlId = 0;
    QList<QPair<qint64, QskTextLabel*>> m_playingMarkers;
    QskLinearBox* m_rows = nullptr;
    QskTextLabel* m_count = nullptr;
    StatusLine* m_status = nullptr;
};

#endif
