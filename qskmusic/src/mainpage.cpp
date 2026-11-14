#include "mainpage.h"
#include "pagemanager.h"
#include "menuoverlay.h"
#include "statusline.h"
#include "toastpopup.h"
#include "iconitem.h"
#include "icongen.h"
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
#include "androidutils.h"
#include <QSettings>
#include <QTimer>
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

    // ── 顶栏（单行 56dp）：左 头像+昵称/状态（两行） | 中 标题 | 右 ☰ ──
    auto* topBar = new QskLinearBox(Qt::Horizontal, layout);
    topBar->setPanel(true);
    topBar->setPreferredHeight(56);

    // 左：头像 28dp（圆形；未登录 identicon，ql 同款 seed "anonymous"）
    m_userAvatar = new IconItem(topBar);
    m_userAvatar->setPreferredSize(28, 28);
    m_userAvatar->setShape(IconItem::Circle);
    m_userAvatar->setFallback(qximage::identicon(
        QStringLiteral("anonymous"), IconItem::pxForDp(28)));

    // 左：两行文本（昵称 / 状态），固定宽避免挤压中间标题
    auto* userCol = new QskLinearBox(Qt::Vertical, topBar);
    userCol->setSpacing(0);
    userCol->setPreferredWidth(96);
    m_userName = new QskTextLabel(tr("Not signed in"), userCol);
    m_userName->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    m_userStatus = new QskTextLabel(tr("Tap to sign in"), userCol);
    m_userStatus->setFontRole(QskFontRole(QskFontRole::Caption, QskFontRole::Normal));
    m_userStatus->setTextColor(QColor(0x9a, 0x9a, 0x9a));
    m_userStatus->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);

    // 左块整块可点 → 登录占位（qlmusic onUserLoginPlaceholder 同响应）
    {
        auto* userTap = new QQuickTapHandler(m_userAvatar);
        connect(userTap, &QQuickTapHandler::singleTapped, this,
            [this](QEventPoint, Qt::MouseButton) {
                ToastPopup::show(this, tr("Sign-in is under development"));
            });
        auto* userTap2 = new QQuickTapHandler(m_userName);
        connect(userTap2, &QQuickTapHandler::singleTapped, this,
            [this](QEventPoint, Qt::MouseButton) {
                ToastPopup::show(this, tr("Sign-in is under development"));
            });
    }

    // 中：标题（Expanding + 居中；左侧用户块固定宽，标题空间可预期）
    auto* title = new QskTextLabel("qskmusic", topBar);
    title->setFontRole(QskFontRole(QskFontRole::Title, QskFontRole::Normal));
    title->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Preferred);
    title->setAlignment(Qt::AlignVCenter | Qt::AlignHCenter);

    auto* menuBtn = new QskPushButton(QString::fromUtf8("☰"), topBar);
    menuBtn->setPreferredSize(44, 44);
    menuBtn->setBoxShapeHint(QskPushButton::Panel,
        QskBoxShapeMetrics(8, Qt::AbsoluteSize));
    connect(menuBtn, &QskAbstractButton::clicked, this, [this, menuBtn]() {
        showOptionsMenu(menuBtn->mapToItem(this, QPointF(0, menuBtn->height())));
    });

    new QskSeparator(Qt::Horizontal, layout);

    // ── 音乐站点（qlmusic SideNav 站点 combo 内容区化；占位：仅 163 可用）──
    {
        auto* siteRow = new QskLinearBox(Qt::Horizontal, layout);
        siteRow->setPreferredHeight(34);
        siteRow->setSpacing(6);

        auto* siteLbl = new QskTextLabel(tr("Music Site"), siteRow);
        siteLbl->setPreferredWidth(68);
        siteLbl->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);

        m_siteBtn = new QskPushButton(tr("163 Music"), siteRow);
        m_siteBtn->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Preferred);
        m_siteBtn->setPreferredHeight(28);
        connect(m_siteBtn, &QskAbstractButton::clicked, this,
            [this]() { showSiteMenu(); });
    }

    new QskSeparator(Qt::Horizontal, layout);

    // ── 分类树（qlmusic SideNav：动态加载）──
    m_status = new StatusLine(layout);
    m_status->setStatus(StatusLine::Loading, tr("Loading categories…"));
    connect(m_status, &StatusLine::retryClicked, this, [this]() {
        m_status->setStatus(StatusLine::Loading, tr("Loading categories…"));
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
                /* 固定导航项：与 qlmusic onNavAction(0/1) 同响应（stub） */
                if (cat == QStringLiteral("__rank__")) {
                    ToastPopup::show(this, tr("Rankings coming soon"));
                    return;
                }
                if (cat == QStringLiteral("__mymusic__")) {
                    ToastPopup::show(this, tr("Sign in required"));
                    return;
                }
                if (cat == QStringLiteral("__local__")) {
                    ToastPopup::show(this, tr("Local music coming soon"));
                    return;
                }
                if (cat == QStringLiteral("__netradio__")) {
                    /* 网络电台：空处理（占位，不请求数据） */
                    ToastPopup::show(this, tr("Internet radio coming soon"));
                    return;
                }
                if (cat == QStringLiteral("__radio__")) {
                    if (auto* pm = pageManager()) {
                        QVariantMap args;
                        args.insert("radio", true);
                        args.insert("title", tr("Stations & Radio"));
                        pm->open("playlists", args);
                    }
                    return;
                }
                openPlaylistList(cat, cat.isEmpty()
                    ? tr("Hot Playlists") : cat);
            });
    }

    connect(&NcmCatalog::instance(), &NcmCatalog::categoriesDone, this,
        [this](const QStringList& hot, const QList<CategoryGroup>& groups,
               const QString& err) {
            if (!err.isEmpty()) {
                m_status->setStatus(StatusLine::Error,
                    tr("Load failed: %1").arg(err));
                return;
            }
            m_status->clearStatus();
            rebuild(hot, groups);
        });

    // 启动即拉分类（qlmusic loadCategories 同口径）
    NcmCatalog::instance().loadCategoriesAsync();

    // 屏幕常亮：默认开；延迟到窗口就绪后应用（anystik 同做法）
    m_keepScreenOn = QSettings().value("keepScreenOn", true).toBool();
    QTimer::singleShot(50, this, [this]() { setKeepScreenOn(m_keepScreenOn); });
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

    const int idxSearch = menu->addOption(QskLabelData(tr("Search")));
    menu->addSeparator();
    const int idxFav = menu->addOption(QskLabelData(tr("My Favorites")));
    const int idxLog = menu->addOption(QskLabelData(tr("Logs")));
    const int idxSettings = menu->addOption(QskLabelData(tr("Settings")));
    const int idxAbout = menu->addOption(QskLabelData(tr("About")));
    const int idxKeepOn = menu->addOption(QskLabelData(
        m_keepScreenOn ? QString::fromUtf8("✓ Keep Screen On")
                       : QString::fromUtf8("  Keep Screen On")));
    menu->setOrigin(origin);

    connect(menu, &QskMenu::triggered, this,
        [this, idxSearch, idxFav, idxLog, idxSettings, idxAbout, idxKeepOn](int index) {
            if (auto* pm = pageManager()) {
                if (index == idxSearch)
                    pm->open("search");
                else if (index == idxFav)
                    pm->open("favorites");
                else if (index == idxLog)
                    pm->open("logs");
                else if (index == idxSettings)
                    pm->open("settings");
                else if (index == idxAbout)
                    pm->open("about");
            }
            if (index == idxKeepOn) {
                m_keepScreenOn = !m_keepScreenOn;
                setKeepScreenOn(m_keepScreenOn);
                QSettings().setValue("keepScreenOn", m_keepScreenOn);
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
    /* qlmusic SideNav 固定项顺序：推荐 → 排行榜 → 我的音乐（stub 同响应） */
    addNavEntry(tr("Rankings"), QStringLiteral("__rank__"), QStringLiteral("rank"));
    addNavEntry(tr("My Music"), QStringLiteral("__mymusic__"), QStringLiteral("mymusic"));
    addNavEntry(tr("Local Music"), QStringLiteral("__local__"), QStringLiteral("local"));
    addNavEntry(tr("Internet Radio"), QStringLiteral("__netradio__"),
                QStringLiteral("radio"));
    addNavEntry(tr("Stations & Radio"), QStringLiteral("__radio__"),
                QStringLiteral("radio"));

    auto addGroup = [&](const QString& title, const QStringList& cats) {
        if (cats.isEmpty()) return;
        addHeader(title);
        auto* grid = addSectionGrid();
        for (const auto& c : cats)
            addChip(grid, c, c);
    };
    addGroup(tr("Hot Categories"), hot);
    for (const auto& g : groups)
        addGroup(g.name, g.categories);
    m_rows->update();
}

void HomePage::addHotEntry()
{
    // 整行高亮入口（保留 qlmusic 推荐位醒目性）
    auto* row = new QskLinearBox(Qt::Horizontal, m_rows);
    row->setPreferredHeight(40);
    row->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Preferred);
    row->setSpacing(6);

    auto* icon = new IconItem(row);
    icon->setPreferredSize(22, 22);
    icon->setCornerRadius(6);
    icon->setFallback(qximage::navIcon(QStringLiteral("hot"), 44));

    auto* lbl = new QskTextLabel(tr("Hot Playlists"), row);
    lbl->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Preferred);
    lbl->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);

    m_chipCats.push_back({ row, QString() });   // 空串 = 热门哨兵
}

void HomePage::addNavEntry(const QString& label, const QString& key,
                           const QString& iconKind)
{
    /* qlmusic SideNav 固定导航项同款行：图标 + 主文本，整行可点 */
    auto* row = new QskLinearBox(Qt::Horizontal, m_rows);
    row->setPreferredHeight(40);
    row->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Preferred);
    row->setSpacing(6);

    auto* iconItem = new IconItem(row);
    iconItem->setPreferredSize(22, 22);
    iconItem->setCornerRadius(6);
    iconItem->setFallback(qximage::navIcon(iconKind, 44));

    auto* lbl = new QskTextLabel(label, row);
    lbl->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Preferred);
    lbl->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);

    m_chipCats.push_back({ row, key });   // "__rank__" / "__mymusic__" 哨兵
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
    auto* chip = new QskLinearBox(Qt::Horizontal, grid);
    chip->setPreferredHeight(32);   // 紧凑 chip（原 44dp 通栏按钮）
    chip->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Preferred);
    chip->setSpacing(4);

    // 分类图标（本地生成首字色块，无网络依赖；尺寸固定不参与伸缩）
    auto* icon = new IconItem(chip);
    icon->setPreferredSize(18, 18);
    icon->setCornerRadius(5);
    icon->setFallback(qximage::categoryIcon(label, 36));

    auto* text = new QskTextLabel(label, chip);
    text->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Preferred);
    text->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);

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

void HomePage::showSiteMenu()
{
    /* 站点点按：占位语义与 qlmusic onSiteChanged 一致（仅 163 可用） */
    for (auto* old : findChildren<QskMenu*>())
        old->deleteLater();
    for (auto* old : findChildren<MenuOverlay*>())
        old->deleteLater();

    const QString kSites[7] = {
        tr("163 Music"), tr("QQ Music"), tr("Baidu Music"), tr("Kugou Music"),
        tr("Kuwo Music"), tr("Bilibili Music"), tr("YouTube Music")
    };
    auto* menu = new QskMenu(this);
    menu->setModal(true);
    menu->setPopupFlag(QskPopup::DeleteOnClose, false);
    QList<int> ids;
    for (int i = 0; i < 7; ++i) {
        ids.append(menu->addOption(QskLabelData(
            (i == 0 ? QString::fromUtf8("✓ ") : QString::fromUtf8("  ")) + kSites[i])));
    }
    menu->setOrigin(m_siteBtn->mapToItem(this,
        QPointF(0, m_siteBtn->height())));
    connect(menu, &QskMenu::triggered, this, [this, ids](int index) {
        const int i = ids.indexOf(index);
        if (i <= 0) {
            m_siteBtn->setText(tr("163 Music"));
        } else {
            qDebug() << "[SITE] switch pending:" << i;
            ToastPopup::show(this, tr("This site is coming soon"));
            m_siteBtn->setText(tr("163 Music"));
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

void HomePage::setLoggedIn(const QString& nickname, const QString& avatarSeed)
{
    m_loggedIn = !nickname.isEmpty();
    m_userNick = nickname;
    if (!m_userName) return;   // onCreate 之前调用则只记录状态
    m_userName->setText(m_loggedIn ? nickname : tr("Not signed in"));
    m_userStatus->setText(m_loggedIn
        ? tr("Signed in")
        : tr("Tap to sign in"));
    m_userAvatar->setFallback(qximage::identicon(
        m_loggedIn ? avatarSeed : QStringLiteral("anonymous"),
        IconItem::pxForDp(28)));
}
