#ifndef CONTENTVIEW_H
#define CONTENTVIEW_H

#include "compat34.h"
#include "musicmodel.h"
#include <qwidget.h>
#include <qrect.h>
#include <qstringlist.h>

class LimeScrollBar;

/* 远端（NCM）歌单卡片，PlaylistListMode 数据 */
struct RemotePlaylistCard {
    qint64 id;
    QString name;
    QString creator;
    QString coverUrl;
    int trackCount;
    qint64 playCount;
    RemotePlaylistCard() : id(0), trackCount(0), playCount(0) {}
};

class ContentView : public QWidget {
    Q_OBJECT
public:
    explicit ContentView(QWidget* parent = 0);
    void setLibrary(const std::vector<Playlist>& lib);
    void setPlayingTrackId(qint64 ncmId);   /* 正在播放的 NCM 曲目（0=无） */    void retranslateUi();

    /* PlaylistListMode：NCM 歌单列表（热门歌单/分类列表） */
    void showPlaylistList(const QString& title, const std::vector<RemotePlaylistCard>& cards,
                          bool hasMore, bool loading);
    void setListLoading(bool loading);

    /* CategoryGridMode：NCM 分类宫格 */
    void showCategoryGrid(const QStringList& cats);

public slots:
    /* TracksMode：本地库歌单（SideNav::playlistSelected 连接目标） */
    void showPlaylist(int playlistId);

    void onScroll(int value);

signals:
    void trackActivated(int playlistId, int trackIndex);
    void playlistActivated(qint64 ncmPlaylistId);
    void categoryActivated(const QString& cat);
    void loadMoreRequested();
    void backRequested();

protected:
    void paintEvent(QPaintEvent* e);
    void mousePressEvent(QMouseEvent* e);
    void mouseMoveEvent(QMouseEvent* e);
    void wheelEvent(QWheelEvent* e);
    void resizeEvent(QResizeEvent* e);
    void leaveEvent(QEvent* e);

private:
    enum ViewMode { TracksMode, PlaylistListMode, CategoryGridMode };

    struct Row {
        int index;      /* TracksMode: trackIndex; List/Grid: 卡片/分类下标 */
        int y;
        int h;
    };

    void rebuildRows();
    void updateScrollBar();
    int rowAt(const QPoint& p) const;
    int totalHeight() const;
    QRect playAllRect() const;
    QRect backRect() const;
    QRect loadMoreRect() const;

    std::vector<Playlist> m_lib;
    const Playlist* m_pl;
    ViewMode m_mode;
    std::vector<Row> m_rows;
    std::vector<RemotePlaylistCard> m_cards;
    QStringList m_gridCats;
    LimeScrollBar* m_scrollBar;
    int m_scrollY;
    int m_scrollDelta;
    int m_hover;
    int m_current;
    qint64 m_playingNcmId;   /* 正在播放的 NCM 曲目 id（0=无，本地曲不标） */
    int m_playlistId;
    int m_headerH;
    int m_rowH;
    QString m_playAll;
    QString m_listTitle;
    QString m_back;
    QString m_loadMore;
    QString m_loading;
    bool m_hasMore;
    bool m_loadingNow;
};

#endif // CONTENTVIEW_H
