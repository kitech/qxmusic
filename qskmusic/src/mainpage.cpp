#include "mainpage.h"
#include "pagemanager.h"
#include "menuoverlay.h"
#include "statusline.h"
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
#include <private/qquicktaphandler_p.h>
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
    m_status = new StatusLine(layout);
    m_status->setStatus(StatusLine::Loading, QString::fromUtf8("正在加载分类…"));
    connect(m_status, &StatusLine::retryClicked, this, [this]() {
        m_status->setStatus(StatusLine::Loading, QString::fromUtf8("正在加载分类…"));
        NcmCatalog::instance().loadCategoriesAsync();
    });

    m_rows = new QskLinearBox(Qt::Vertical);
    m_rows->setSpacing(2);
    m_rows->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Minimum);
    auto* scroll = new QskScrollArea(layout);
    scroll->setScrolledItem(m_rows);
    scroll->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Expanding);

    // 安卓触摸坑：滚动内容里的行点击不能靠 QskPushButton::clicked（flickable
    // 拦 touch 后不再合成 mouse），照 anystik stickerlist 用 QQuickTapHandler
    {
        auto* tap = new QQuickTapHandler(m_rows);
        tap->setGesturePolicy(QQuickTapHandler::DragThreshold);
        connect(tap, &QQuickTapHandler::singleTapped, this,
            [this](QEventPoint point, Qt::MouseButton) {
                QString cat;
                if (!chipAt(point.position(), &cat)) return;
                openPlaylistList(cat, cat.isEmpty()
                    ? QString::fromUtf8("热门歌单") : cat);
            });
    }

    connect(&NcmCatalog::instance(), &NcmCatalog::categoriesDone, this,
        [this](const QStringList& hot, const QList<CategoryGroup>& groups,
               const QString& err) {
            if (!err.isEmpty()) {
                m_status->setStatus(StatusLine::Error,
                    QString::fromUtf8("加载失败：%1").arg(err));
                return;
            }
            m_status->clearStatus();
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
    m_chipCats.clear();

    auto addHeader = [this](const QString& text) {
        auto* lbl = new QskTextLabel(text, m_rows);
        lbl->setFontRole(QskFontRole(QskFontRole::Title, QskFontRole::Normal));
        lbl->setPreferredHeight(30);   // 紧凑：38 → 30
        lbl->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    };

    // 首行 = 热门歌单（qlmusic __hot__ 哨兵；服务端 cat="全部"）
    addHotEntry();

    auto addGroup = [&](const QString& title, const QStringList& cats) {
        if (cats.isEmpty()) return;
        addHeader(title);
        auto* grid = addSectionGrid();
        for (const auto& c : cats)
            addChip(grid, c, c);
    };
    addGroup(QString::fromUtf8("热门分类"), hot);
    for (const auto& g : groups)
        addGroup(g.name, g.categories);
    m_rows->update();
}

void HomePage::addHotEntry()
{
    // 整行高亮入口（保留 qlmusic 推荐位醒目性）
    auto* lbl = new QskTextLabel(QString::fromUtf8("热门歌单"), m_rows);
    lbl->setPreferredHeight(40);
    lbl->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Preferred);
    lbl->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    m_chipCats.push_back({ lbl, QString() });   // 空串 = 热门哨兵
}

QskLinearBox* HomePage::addSectionGrid()
{
    // 三列 chips 网格（QSkinny 官方 dimension 口径：超 3 个自动换行）
    auto* grid = new QskLinearBox(Qt::Vertical, 3, m_rows);
    grid->setSpacing(6);
    return grid;
}

void HomePage::addChip(QskLinearBox* grid, const QString& label,
    const QString& cat)
{
    auto* chip = new QskTextLabel(label, grid);
    chip->setPreferredHeight(32);   // 紧凑 chip（原 44dp 通栏按钮）
    chip->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Preferred);
    chip->setAlignment(Qt::AlignCenter);
    m_chipCats.push_back({ chip, cat });
}

bool HomePage::chipAt(const QPointF& localPos, QString* cat) const
{
    // 嵌套网格命中：逐 chips 把其 rect 映射到 m_rows 坐标系做 contains
    for (const auto& p : m_chipCats) {
        if (!p.first) continue;
        const QRectF r = p.first->mapRectToItem(
            m_rows, QRectF(0, 0, p.first->width(), p.first->height()));
        if (r.contains(localPos)) {
            if (cat) *cat = p.second;
            return true;
        }
    }
    return false;
}

void HomePage::openPlaylistList(const QString& cat, const QString& title)
{
    if (auto* pm = pageManager()) {
        QVariantMap args;
        args.insert("cat", cat);
        args.insert("title", title);
        pm->open("playlists", args);
    }
}
