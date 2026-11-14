#ifndef CONTENTVIEW_H
#define CONTENTVIEW_H

#include "compat34.h"
#include "musicmodel.h"
#include "radiolist.h"
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

    /* 站点电台：电台列表（复用 PlaylistListMode 渲染，点击发 radioSelected） */
    void showRadioList(const QString& title,
                       const std::vector<RemotePlaylistCard>& radios,
                       bool hasMore, bool loading);
    /* 站点电台：节目列表（复用 TracksMode；m_tempPl 作宿主保证 m_pl 有效） */
    void showRadioPrograms(qint64 rid, const QString& title,
                           const std::vector<Track>& programs);
    /* 网络电台：地区内固定电台清单（复用 TracksMode 行；HLS 行灰态不可播） */
    void showNetRadioRegion(const QString& regionKey, const QString& title);
    /* 当前电台地区 key（MainWindow 取流时用） */
    QString currentNetRegion() const { return m_netRegion; }
    /* 当前歌曲列表对应的 NCM 歌单 id（0 = 本地歌单/电台），刷新按钮用 */
    qint64 currentNcmPlaylistId() const;

    /* CategoryGridMode：NCM 分类宫格 */
    void showCategoryGrid(const QStringList& cats);

public slots:
    /* TracksMode：本地库歌单（SideNav::playlistSelected 连接目标） */
    void showPlaylist(int playlistId);

    void onScroll(int value);

signals:
    void trackActivated(int playlistId, int trackIndex);
    /* TracksMode 右键菜单"收藏/取消收藏"（MainWindow 落 FavoriteStore） */
    void favToggled(int playlistId, int trackIndex);
    void playlistActivated(qint64 ncmPlaylistId);
    /* 站点电台：列表里点了某电台（rid） */
    void radioSelected(qint64 rid);
    /* 网络电台：选了某地区（原始 key，如 "cn"） */
    void netRadioRegionSelected(const QString& regionKey);
    /* 网络电台：点了某电台行（索引进当前 m_netRadioStreams 顺序） */
    void netRadioStreamSelected(int index);
    void categoryActivated(const QString& cat);
    void loadMoreRequested();
    void backRequested();
    /* 标题行 ⟳：MainWindow 按当前模式重载数据 */
    void refreshRequested();

protected:
    void paintEvent(QPaintEvent* e);
    void mousePressEvent(QMouseEvent* e);
    void contextMenuEvent(QContextMenuEvent* e);
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
    QRect refreshRect() const;
    QRect loadMoreRect() const;

    std::vector<Playlist> m_lib;
    const Playlist* m_pl;
    bool m_radioList;        /* PlaylistListMode 的电台变体：点卡片发 radioSelected */
    bool m_radioProgramMode; /* 站点电台节目列表：计数单位「期」 */
    Playlist m_tempPl;       /* showRadioPrograms 的宿主，生命周期由 ContentView 持有 */
    bool m_netRadioMode;     /* TracksMode 的电台变体：行=直播流，不走 trackActivated */
    QString m_netRegion;     /* 当前电台地区 key（供 MainWindow 取流清单） */
    std::vector<RadioStation> m_netRadioStreams;

    ViewMode m_mode;
    std::vector<Row> m_rows;
    std::vector<RemotePlaylistCard> m_cards;
    QStringList m_gridCats;
    LimeScrollBar* m_scrollBar;
    int m_scrollY;
    int m_scrollDelta;
    int m_hover;
    int m_current;
    bool m_hoverRefresh;     /* 标题行 ⟳ 悬停态 */
    qint64 m_playingNcmId;   /* 正在播放的 NCM 曲目 id（0=无，本地曲不标） */
    int m_playlistId;    int m_headerH;
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
