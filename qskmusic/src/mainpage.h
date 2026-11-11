#ifndef MAIN_PAGE_H
#define MAIN_PAGE_H

#include "page.h"
#include "ncmcatalog.h"
#include <QPair>
#include <QRectF>
#include <QPointF>

class QskLinearBox;
class QskTextLabel;
class QskPushButton;
class QskQuickItem;
class StatusLine;

/*
 * 首页（qlmusic SideNav 的手机化）：
 *   顶栏 = 标题 + ☰（搜索/日志/设置/关于）；
 *   内容 = 分类树：第 1 项「热门歌单」(=__hot__ 哨兵) + 热门分类 + 分组分类，
 *          启动即动态拉取（qlmusic loadCategories）；点项 push 歌单列表页。
 */
class HomePage : public Page
{
    Q_OBJECT
public:
    explicit HomePage(QQuickItem* parent = nullptr);

protected:
    void onCreate(const QVariantMap& launchArgs,
                  const QVariantMap& savedState) override;

private:
    void showOptionsMenu(const QPointF& origin);
    void rebuild(const QStringList& hot, const QList<CategoryGroup>& groups);
    void addHotEntry();
    QskLinearBox* addSectionGrid();
    void addChip(QskLinearBox* grid, const QString& label, const QString& cat);
    void openPlaylistList(const QString& cat, const QString& title);
    bool chipAt(const QPointF& localPos, QString* cat) const;

    QskLinearBox* m_rows = nullptr;
    StatusLine* m_status = nullptr;
    // chips 命中表：item → cat（TapHandler 用 mapRectToItem 测交，嵌套网格亦可）
    QList<QPair<QQuickItem*, QString>> m_chipCats;
};

#endif
