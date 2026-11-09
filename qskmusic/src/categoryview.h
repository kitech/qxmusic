#ifndef CATEGORY_VIEW_H
#define CATEGORY_VIEW_H

#include <QskLinearBox.h>

struct CategoryGroup;
class QskScrollArea;

/*
 * 「分类」tab 内容（qlmusic 侧栏分类树的扁平化移植）：
 *   热门歌单入口（哨兵：空 cat）+ 热门分类 + 分组（组名 header + 分类项）
 * 点击发 categorySelected(cat)；空串 = 热门。
 */
class CategoryView : public QskLinearBox
{
    Q_OBJECT
public:
    explicit CategoryView(QQuickItem* parent = nullptr);

    void reload();

Q_SIGNALS:
    void categorySelected(const QString& cat);

private:
    void rebuild(const QStringList& hot, const QList<CategoryGroup>& groups);
    void addHeader(const QString& text);
    void addCategory(const QString& cat);

    QskScrollArea* m_scroll = nullptr;
    QskLinearBox* m_rows = nullptr;
};

#endif
