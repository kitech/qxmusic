#ifndef SIDENAV_H
#define SIDENAV_H

#include "compat34.h"
#include "musicmodel.h"
#include <qwidget.h>

class SearchLineEdit;

class SideNav : public QWidget {
    Q_OBJECT
public:
    explicit SideNav(QWidget* parent = 0);
    void setLibrary(const std::vector<Playlist>& lib);
    void setCurrentPlaylist(int playlistId);
    void retranslateUi();

signals:
    void playlistSelected(int playlistId);

protected:
    void paintEvent(QPaintEvent* e);
    void mousePressEvent(QMouseEvent* e);
    void mouseMoveEvent(QMouseEvent* e);
    void leaveEvent(QEvent* e);
    void resizeEvent(QResizeEvent* e);

private:
    struct NavItem {
        int id;
        QString text;
        int y;
        int h;
    };
    void relayout();
    int itemAt(const QPoint& p) const;

    SearchLineEdit* m_search;
    std::vector<Playlist> m_lib;
    std::vector<NavItem> m_items;
    int m_current;
    int m_hover;
    QString m_navRecommend;
    QString m_navRank;
    QString m_navMine;
    QString m_groupCreated;
    QString m_groupCollected;
};

#endif // SIDENAV_H
