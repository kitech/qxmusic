#include "mainpage.h"
#include "pagemanager.h"
#include "menuoverlay.h"
#include <QskLinearBox.h>
#include <QskTextLabel.h>
#include <QskPushButton.h>
#include <QskScrollArea.h>
#include <QskSeparator.h>
#include <QskFontRole.h>
#include <QskMenu.h>
#include <QskLabelData.h>
#include <QskPopup.h>
#include <QskBoxShapeMetrics.h>
#include <QPointF>

HomePage::HomePage(QQuickItem* parent)
    : Page(parent)
{
}

void HomePage::onCreate(const QVariantMap&, const QVariantMap&)
{
    setAutoLayoutChildren(true);
    auto* layout = new QskLinearBox(Qt::Vertical, this);
    layout->setPanel(true);

    // ── 顶栏：标题 + ☰（anystik stickerhomepage 同款）──
    auto* topBar = new QskLinearBox(Qt::Horizontal, layout);
    topBar->setPanel(true);
    topBar->setPreferredHeight(56);

    auto* title = new QskTextLabel("qskmusic", topBar);
    title->setFontRole(QskFontRole(QskFontRole::Title, QskFontRole::Normal));
    title->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Preferred);
    title->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);

    auto* menuBtn = new QskPushButton(QString::fromUtf8("☰"), topBar);
    menuBtn->setPreferredSize(44, 44);
    menuBtn->setBoxShapeHint(QskPushButton::Panel,
        QskBoxShapeMetrics(8, Qt::AbsoluteSize));
    connect(menuBtn, &QskAbstractButton::clicked, this, [this, menuBtn]() {
        showOptionsMenu(menuBtn->mapToItem(this, QPointF(0, menuBtn->height())));
    });

    new QskSeparator(Qt::Horizontal, layout);

    // ── 分类树（qlmusic SideNav：动态加载）──
    m_status = new QskTextLabel(QString::fromUtf8("分类加载中…"), layout);
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

    // 启动即拉分类（qlmusic loadCategories 同口径）
    NcmCatalog::instance().loadCategoriesAsync();
}

void HomePage::showOptionsMenu(const QPointF& origin)
{
    // 旧菜单/遮罩先清（重复点击不叠加），anystik showOptionsMenu 同款
    for (auto* old : findChildren<QskMenu*>())
        old->deleteLater();
    for (auto* old : findChildren<MenuOverlay*>())
        old->deleteLater();

    auto* menu = new QskMenu(this);
    menu->setModal(true);
    menu->setPopupFlag(QskPopup::DeleteOnClose, false);

    const int idxSearch = menu->addOption(QskLabelData(QString::fromUtf8("搜索")));
    menu->addSeparator();
    const int idxLog = menu->addOption(QskLabelData(tr("Logs")));
    const int idxSettings = menu->addOption(QskLabelData(tr("Settings")));
    const int idxAbout = menu->addOption(QskLabelData(tr("About")));
    menu->setOrigin(origin);

    connect(menu, &QskMenu::triggered, this,
        [this, idxSearch, idxLog, idxSettings, idxAbout](int index) {
            if (auto* pm = pageManager()) {
                if (index == idxSearch)
                    pm->open("search");
                else if (index == idxLog)
                    pm->open("logs");
                else if (index == idxSettings)
                    pm->open("settings");
                else if (index == idxAbout)
                    pm->open("about");
            }
            if (auto* m = qobject_cast<QskMenu*>(sender()))
                m->close();
        });

    {
        auto* overlay = new MenuOverlay(menu);
        connect(menu, &QObject::destroyed, overlay, &QObject::deleteLater);
        connect(menu, &QskPopup::closed, overlay, &QObject::deleteLater);
    }
    menu->open();
}

void HomePage::rebuild(const QStringList& hot,
    const QList<CategoryGroup>& groups)
{
    const auto children = m_rows->childItems();
    for (auto* c : children) c->deleteLater();

    // 第一项 = 热门歌单（qlmusic SideNav __hot__ 哨兵，服务端 cat="全部"）
    addEntry(QString::fromUtf8("热门歌单"), QString());

    if (!hot.isEmpty()) {
        auto* lbl = new QskTextLabel(QString::fromUtf8("热门分类"), m_rows);
        lbl->setFontRole(QskFontRole(QskFontRole::Title, QskFontRole::Normal));
        lbl->setPreferredHeight(38);
        lbl->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
        for (const auto& c : hot)
            addEntry(c, c);
    }
    for (const auto& g : groups) {
        auto* lbl = new QskTextLabel(g.name, m_rows);
        lbl->setFontRole(QskFontRole(QskFontRole::Title, QskFontRole::Normal));
        lbl->setPreferredHeight(38);
        lbl->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
        for (const auto& c : g.categories)
            addEntry(c, c);
    }
    m_rows->update();
}

void HomePage::addEntry(const QString& label, const QString& cat)
{
    auto* btn = new QskPushButton(label, m_rows);
    btn->setPreferredHeight(44);
    connect(btn, &QskAbstractButton::clicked, this, [this, cat]() {
        if (auto* pm = pageManager()) {
            QVariantMap args;
            args.insert("cat", cat);
            args.insert("title", cat.isEmpty()
                ? QString::fromUtf8("热门歌单") : cat);
            pm->open("playlists", args);
        }
    });
}
