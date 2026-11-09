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

namespace {

QString fmtCount(qint64 n)
{
    if (n >= 100000000)
        return QString("%1亿").arg(double(n) / 100000000.0, 0, 'f', 1);
    if (n >= 10000)
        return QString("%1万").arg(double(n) / 10000.0, 0, 'f', 1);
    return QString::number(n);
}

// anystik stickerlist 范式：代码级截断，文本永不画出 item 边界
QString elided(const QString& text, int maxChars)
{
    return text.size() > maxChars
        ? text.left(maxChars - 1) + QString::fromUtf8("…") : text;
}

} // namespace

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

    // ── 热门歌单列表（qlmusic ContentView::PlaylistListMode）──
    m_status = new QskTextLabel(QString::fromUtf8("加载中…"), layout);
    m_status->setPreferredHeight(32);
    m_status->setAlignment(Qt::AlignCenter);

    m_cardRows = new QskLinearBox(Qt::Vertical);
    m_cardRows->setSpacing(2);
    m_cardRows->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Minimum);
    auto* scroll = new QskScrollArea(layout);
    scroll->setScrolledItem(m_cardRows);
    scroll->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Expanding);

    m_moreBtn = new QskPushButton(QString::fromUtf8("加载更多"), layout);
    m_moreBtn->setPreferredHeight(44);
    m_moreBtn->setVisible(false);
    connect(m_moreBtn, &QskAbstractButton::clicked, this, [this]() {
        requestPage(m_cards.size());
    });

    connect(&NcmCatalog::instance(), &NcmCatalog::topPlaylistsDone, this,
        [this](const QList<PlaylistCard>& cards, bool more, const QString& err) {
            if (!m_loading) return;
            m_loading = false;
            if (!err.isEmpty()) {
                m_status->setText(QString::fromUtf8("加载失败：%1").arg(err));
                m_moreBtn->setVisible(m_more);
                return;
            }
            m_cards += cards;
            m_more = more;
            m_status->setText(m_cards.isEmpty() ? QString::fromUtf8("暂无歌单")
                                                : QString());
            m_moreBtn->setVisible(m_more);
            rebuildCards();
        });

    requestPage(0);   // 启动即拉（qlmusic 启动口径）
}

void HomePage::showOptionsMenu(const QPointF& origin)
{
    //旧菜单/遮罩先清（重复点击不叠加），anystik showOptionsMenu 同款
    for (auto* old : findChildren<QskMenu*>())
        old->deleteLater();
    for (auto* old : findChildren<MenuOverlay*>())
        old->deleteLater();

    auto* menu = new QskMenu(this);
    menu->setModal(true);
    menu->setPopupFlag(QskPopup::DeleteOnClose, false);

    const int idxCategory = menu->addOption(QskLabelData(QString::fromUtf8("分类")));
    const int idxSearch = menu->addOption(QskLabelData(QString::fromUtf8("搜索")));
    menu->addSeparator();
    const int idxLog = menu->addOption(QskLabelData(tr("Logs")));
    const int idxSettings = menu->addOption(QskLabelData(tr("Settings")));
    const int idxAbout = menu->addOption(QskLabelData(tr("About")));
    menu->setOrigin(origin);

    connect(menu, &QskMenu::triggered, this,
        [this, idxCategory, idxSearch, idxLog, idxSettings, idxAbout](int index) {
            if (auto* pm = pageManager()) {
                if (index == idxCategory)
                    pm->open("category");
                else if (index == idxSearch)
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

void HomePage::requestPage(int offset)
{
    if (m_loading) return;
    m_loading = true;
    m_status->setText(QString::fromUtf8("加载中…"));
    m_moreBtn->setVisible(false);
    if (offset == 0) {
        m_cards.clear();
        m_more = false;
    }
    // 热门哨兵：qlmusic 口径 cat="全部" order="hot"
    NcmCatalog::instance().loadTopPlaylistsAsync(QString::fromUtf8("全部"),
                                                 QStringLiteral("hot"), 30, offset);
}

void HomePage::rebuildCards()
{
    const auto children = m_cardRows->childItems();
    for (auto* c : children) c->deleteLater();

    for (const auto& card : m_cards) {
        auto* row = new QskLinearBox(Qt::Horizontal, m_cardRows);
        row->setPreferredHeight(56);
        row->setSpacing(8);

        auto* openBtn = new QskPushButton(elided(card.name, 16), row);
        openBtn->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Preferred);
        connect(openBtn, &QskAbstractButton::clicked, this, [this, card]() {
            if (auto* pm = pageManager()) {
                QVariantMap args;
                args.insert("playlistId", card.id);
                args.insert("title", card.name);
                pm->open("tracks", args);
            }
        });

        QString sub = card.creator.isEmpty()
            ? QString() : elided(card.creator, 8) + QString::fromUtf8(" · ");
        sub += QString::fromUtf8("%1 首").arg(card.trackCount);
        sub += QString::fromUtf8(" · 播放 %1").arg(fmtCount(card.playCount));
        auto* subLbl = new QskTextLabel(sub, row);
        subLbl->setPreferredWidth(120);
        subLbl->setSizePolicy(QskSizePolicy::Minimum, QskSizePolicy::Preferred);
        subLbl->setAlignment(Qt::AlignVCenter | Qt::AlignRight);
    }
    m_cardRows->update();
}
