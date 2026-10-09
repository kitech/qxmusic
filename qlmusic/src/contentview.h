#ifndef CONTENTVIEW_H
#define CONTENTVIEW_H

#include "compat34.h"
#include "musicmodel.h"
#include <qwidget.h>

class LimeScrollBar;

class ContentView : public QWidget {
    Q_OBJECT
public:
    explicit ContentView(QWidget* parent = 0);
    void setLibrary(const std::vector<Playlist>& lib);
    void retranslateUi();

public slots:
    void showPlaylist(int playlistId);
    void onScroll(int value);

signals:
    void trackActivated(int playlistId, int trackIndex);

protected:
    void paintEvent(QPaintEvent* e);
    void mousePressEvent(QMouseEvent* e);
    void mouseMoveEvent(QMouseEvent* e);
    void wheelEvent(QWheelEvent* e);
    void resizeEvent(QResizeEvent* e);
    void leaveEvent(QEvent* e);

private:
    struct Row {
        int trackIndex;
        int y;
        int h;
    };

    void rebuildRows();
    void updateScrollBar();
    int rowAt(const QPoint& p) const;
    int totalHeight() const;
    QRect playAllRect() const;

    std::vector<Playlist> m_lib;
    const Playlist* m_pl;
    std::vector<Row> m_rows;
    LimeScrollBar* m_scrollBar;
    int m_scrollY;
    int m_scrollDelta;
    int m_hover;
    int m_current;
    int m_playlistId;
    int m_headerH;
    int m_rowH;
    QString m_playAll;
};

#endif // CONTENTVIEW_H
