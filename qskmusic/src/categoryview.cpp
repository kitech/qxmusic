#include "categoryview.h"
#include "ncmcatalog.h"
#include <QskScrollArea.h>
#include <QskTextLabel.h>
#include <QskPushButton.h>
#include <QskFontRole.h>

CategoryView::CategoryView(QQuickItem* parent)
    : QskLinearBox(Qt::Vertical, parent)
{
    setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Expanding);

    m_scroll = new QskScrollArea(this);
    m_scroll->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Expanding);
    m_rows = new QskLinearBox(Qt::Vertical);
    m_rows->setSpacing(2);
    m_rows->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Minimum);
    m_scroll->setScrolledItem(m_rows);

    connect(&NcmCatalog::instance(), &NcmCatalog::categoriesDone, this,
        [this](const QStringList& hot, const QList<CategoryGroup>& groups,
               const QString& err) {
            const auto children = m_rows->childItems();
            for (auto* c : children) c->deleteLater();
            if (!err.isEmpty()) {
                addHeader(QString::fromUtf8("分类加载失败：%1").arg(err));
                auto* retry = new QskPushButton(QStringLiteral("重试"), m_rows);
                retry->setPreferredHeight(40);
                connect(retry, &QskAbstractButton::clicked, this,
                        &CategoryView::reload);
                m_rows->update();
                return;
            }
            rebuild(hot, groups);
        });

    reload();
}

void CategoryView::reload()
{
    const auto children = m_rows->childItems();
    for (auto* c : children) c->deleteLater();
    addHeader(QString::fromUtf8("分类加载中…"));
    m_rows->update();
    NcmCatalog::instance().loadCategoriesAsync();
}

void CategoryView::rebuild(const QStringList& hot,
    const QList<CategoryGroup>& groups)
{
    // 热门歌单入口（哨兵：空 cat，服务端 cat="全部" order="hot"）
    auto* hotBtn = new QskPushButton(QString::fromUtf8("热门歌单"), m_rows);
    hotBtn->setPreferredHeight(44);
    connect(hotBtn, &QskAbstractButton::clicked, this, [this]() {
        Q_EMIT categorySelected(QString());
    });

    if (!hot.isEmpty()) {
        addHeader(QString::fromUtf8("热门分类"));
        for (const auto& c : hot)
            addCategory(c);
    }
    for (const auto& g : groups) {
        addHeader(g.name);
        for (const auto& c : g.categories)
            addCategory(c);
    }
    m_rows->update();
}

void CategoryView::addHeader(const QString& text)
{
    auto* lbl = new QskTextLabel(text, m_rows);
    lbl->setFontRole(QskFontRole(QskFontRole::Title, QskFontRole::Normal));
    lbl->setPreferredHeight(38);
    lbl->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
}

void CategoryView::addCategory(const QString& cat)
{
    auto* btn = new QskPushButton(cat, m_rows);
    btn->setPreferredHeight(40);
    connect(btn, &QskAbstractButton::clicked, this, [this, cat]() {
        Q_EMIT categorySelected(cat);
    });
}
