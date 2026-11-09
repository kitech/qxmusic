#include "playlistlistpage.h"
#include "pagemanager.h"
#include <QskLinearBox.h>
#include <QskTextLabel.h>
#include <QskPushButton.h>
#include <QskScrollArea.h>
#include <QskSeparator.h>
#include <QskFontRole.h>

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

PlaylistListPage::PlaylistListPage(QQuickItem* parent)
    : Page(parent)
{
}

void PlaylistListPage::onCreate(const QVariantMap& launchArgs,
    const QVariantMap&)
{
    m_cat = launchArgs.value("cat").toString();
    m_title = launchArgs.value("title").toString();
    if (m_title.isEmpty())
        m_title = m_cat.isEmpty() ? QString::fromUtf8("热门歌单") : m_cat;

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
    auto* title = new QskTextLabel(m_title, topBar);
    title->setFontRole(QskFontRole(QskFontRole::Title, QskFontRole::Normal));
    title->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Preferred);
    title->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);

    new QskSeparator(Qt::Horizontal, layout);

    // ── 歌单卡片滚动区 ──
    m_cardRows = new QskLinearBox(Qt::Vertical);
    m_cardRows->setSpacing(2);
    m_cardRows->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Minimum);
    auto* scroll = new QskScrollArea(layout);
    scroll->setScrolledItem(m_cardRows);
    scroll->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Expanding);

    // ── 底部：状态 + 加载更多 ──
    m_status = new QskTextLabel(layout);
    m_status->setPreferredHeight(32);
    m_status->setAlignment(Qt::AlignCenter);
    m_moreBtn = new QskPushButton(QString::fromUtf8("加载更多"), layout);
    m_moreBtn->setPreferredHeight(44);
    m_moreBtn->setVisible(false);
    connect(m_moreBtn, &QskAbstractButton::clicked, this, [this]() {
        requestPage(m_cards.size());
    });

    connect(&NcmCatalog::instance(), &NcmCatalog::topPlaylistsDone, this,
        [this](const QList<PlaylistCard>& cards, bool more, const QString& err) {
            if (!m_loading) return;   // 非本页在途请求
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

    requestPage(0);
}

void PlaylistListPage::requestPage(int offset)
{
    if (m_loading) return;
    m_loading = true;
    m_status->setText(QString::fromUtf8("加载中…"));
    m_moreBtn->setVisible(false);
    if (offset == 0) {
        m_cards.clear();
        m_more = false;
    }
    // 热门哨兵：服务端 cat="全部"（qlmusic 同口径）
    const QString cat = m_cat.isEmpty() ? QString::fromUtf8("全部") : m_cat;
    NcmCatalog::instance().loadTopPlaylistsAsync(cat, QStringLiteral("hot"),
                                                 30, offset);
}

void PlaylistListPage::rebuildCards()
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

        QString sub;
        if (!card.creator.isEmpty())
            sub = elided(card.creator, 8) + QString::fromUtf8(" · ");
        sub += QString::fromUtf8("%1 首").arg(card.trackCount);
        sub += QString::fromUtf8(" · 播放 %1").arg(fmtCount(card.playCount));
        auto* subLbl = new QskTextLabel(sub, row);
        subLbl->setPreferredWidth(120);
        subLbl->setSizePolicy(QskSizePolicy::Preferred, QskSizePolicy::Preferred);
        subLbl->setAlignment(Qt::AlignVCenter | Qt::AlignRight);
    }
    m_cardRows->update();
}
