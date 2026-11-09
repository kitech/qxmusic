#include "categorypage.h"
#include "pagemanager.h"
#include "ncmcatalog.h"
#include <QskLinearBox.h>
#include <QskTextLabel.h>
#include <QskPushButton.h>
#include <QskScrollArea.h>
#include <QskSeparator.h>
#include <QskFontRole.h>

CategoryPage::CategoryPage(QQuickItem* parent)
    : Page(parent)
{
}

void CategoryPage::onCreate(const QVariantMap&, const QVariantMap&)
{
    setAutoLayoutChildren(true);
    auto* layout = new QskLinearBox(Qt::Vertical, this);
    layout->setPanel(true);

    // ── 顶栏：返回 + 标题 ──
    auto* topBar = new QskLinearBox(Qt::Horizontal, layout);
    topBar->setPanel(true);
    topBar->setPreferredHeight(56);

    auto* backBtn = new QskPushButton(QString::fromUtf8("←"), topBar);
    connect(backBtn, &QskAbstractButton::clicked, this, [this]() {
        if (auto* pm = pageManager()) pm->back();
    });
    auto* title = new QskTextLabel(QString::fromUtf8("分类"), topBar);
    title->setFontRole(QskFontRole(QskFontRole::Title, QskFontRole::Normal));
    title->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Preferred);
    title->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);

    new QskSeparator(Qt::Horizontal, layout);

    m_status = new QskTextLabel(QString::fromUtf8("加载中…"), layout);
    m_status->setPreferredHeight(32);
    m_status->setAlignment(Qt::AlignCenter);

    m_rows = new QskLinearBox(Qt::Vertical);
    m_rows->setSpacing(2);
    m_rows->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Minimum);
    auto* scroll = new QskScrollArea(layout);
    scroll->setScrolledItem(m_rows);
    scroll->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Expanding);

    connect(&NcmCatalog::instance(), &NcmCatalog::categoriesDone, this,
        [this](const QStringList& hot, const QList<CategoryGroup>& groups,
               const QString& err) {
            if (!err.isEmpty()) {
                m_status->setText(QString::fromUtf8("分类加载失败：%1").arg(err));
                return;
            }
            m_status->setText(QString());
            rebuild(hot, groups);
        });

    // 启动时（home 构造）已后台拉取；这里没拿到缓存信号就现拉
    NcmCatalog::instance().loadCategoriesAsync();
}

void CategoryPage::rebuild(const QStringList& hot,
    const QList<CategoryGroup>& groups)
{
    const auto children = m_rows->childItems();
    for (auto* c : children) c->deleteLater();

    if (!hot.isEmpty()) {
        auto* lbl = new QskTextLabel(QString::fromUtf8("热门分类"), m_rows);
        lbl->setFontRole(QskFontRole(QskFontRole::Title, QskFontRole::Normal));
        lbl->setPreferredHeight(38);
        lbl->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
        for (const auto& c : hot)
            addCategory(c);
    }
    for (const auto& g : groups) {
        auto* lbl = new QskTextLabel(g.name, m_rows);
        lbl->setFontRole(QskFontRole(QskFontRole::Title, QskFontRole::Normal));
        lbl->setPreferredHeight(38);
        lbl->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
        for (const auto& c : g.categories)
            addCategory(c);
    }
    m_rows->update();
}

void CategoryPage::addCategory(const QString& cat)
{
    auto* btn = new QskPushButton(cat, m_rows);
    btn->setPreferredHeight(40);
    connect(btn, &QskAbstractButton::clicked, this, [this, cat]() {
        if (auto* pm = pageManager()) {
            QVariantMap args;
            args.insert("cat", cat);
            args.insert("title", cat);
            pm->open("playlists", args);
        }
    });
}
