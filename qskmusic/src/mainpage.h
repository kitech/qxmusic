#ifndef MAIN_PAGE_H
#define MAIN_PAGE_H

#include "page.h"
#include "ncmcatalog.h"
#include <QPair>
#include <QRectF>
#include <QPointF>
#include <QString>

class QskLinearBox;
class QskTextLabel;
class QskPushButton;
class QskQuickItem;
class StatusLine;
class IconItem;

/*
 * 首页（qlmusic SideNav 的手机化）：
 *   顶栏（单行 56dp）= 左「头像 + 昵称/状态两行」 + 中标题 + 右 ☰；
 *   内容顶部 = 音乐站点下拉（占位，仅 163 可用）；
 *   内容 = 分类树：热门歌单 → 排行榜 → 我的音乐 → 热门分类 + 分组分类。
 */
class HomePage : public Page
{
    Q_OBJECT
public:
    explicit HomePage(QQuickItem* parent = nullptr);
    /* 用户区（qlmusic SideNav::setUserLoggedIn 同义；登录能力落地前无调用方） */
    void setLoggedIn(const QString& nickname, const QString& avatarSeed);

protected:
    void onCreate(const QVariantMap& launchArgs,
                  const QVariantMap& savedState) override;

private:
    void showOptionsMenu(const QPointF& origin);
    void showSiteMenu();
    void rebuild(const QStringList& hot, const QList<CategoryGroup>& groups);
    void addHotEntry();
    void addNavEntry(const QString& label, const QString& key);
    QskLinearBox* addSectionGrid();
    void addChip(QskLinearBox* grid, const QString& label, const QString& cat);
    void openPlaylistList(const QString& cat, const QString& title);
    bool chipAt(const QPointF& localPos, QString* cat) const;

    QskLinearBox* m_rows = nullptr;
    StatusLine* m_status = nullptr;
    IconItem* m_userAvatar = nullptr;
    QskTextLabel* m_userName = nullptr;
    QskTextLabel* m_userStatus = nullptr;
    QskPushButton* m_siteBtn = nullptr;
    QString m_userNick;
    bool m_loggedIn = false;
    bool m_keepScreenOn = true;   // 屏幕常亮（默认开，QSettings 持久化）
    // chips 命中表：item → cat（TapHandler 用 mapRectToItem 测交，嵌套网格亦可）
    QList<QPair<QQuickItem*, QString>> m_chipCats;
};

#endif
