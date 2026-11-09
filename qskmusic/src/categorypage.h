#ifndef CATEGORY_PAGE_H
#define CATEGORY_PAGE_H

#include "page.h"
#include "ncmcatalog.h"

class QskLinearBox;
class QskTextLabel;

/*
 * 分类页（qlmusic ContentView::CategoryGridMode）：
 *   顶栏返回 + 分类树（热门分类 + 分组）；点分类 push 歌单列表页；
 *   空串哨兵 = 热门歌单（qlmusic __hot__）。
 */
class CategoryPage : public Page
{
    Q_OBJECT
public:
    explicit CategoryPage(QQuickItem* parent = nullptr);

protected:
    void onCreate(const QVariantMap& launchArgs,
                  const QVariantMap& savedState) override;

private:
    void rebuild(const QStringList& hot, const QList<CategoryGroup>& groups);
    void addCategory(const QString& cat);

    QskLinearBox* m_rows = nullptr;
    QskTextLabel* m_status = nullptr;
};

#endif
