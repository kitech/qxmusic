#include "playlistlistpage.h"
#include "pagemanager.h"
#include "statusline.h"
#include <private/qquicktaphandler_p.h>
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

    // ── 歌单卡片滚动区（状态行并入滚动内容首项，不占顶部固定带）──
    auto* scroll = new QskScrollArea(layout);
    scroll->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Expanding);
    auto* content = new QskLinearBox(Qt::Vertical);
    content->setSpacing(2);
    content->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Minimum);
    scroll->setScrolledItem(content);

    m_status = new StatusLine(content);
    connect(m_status, &StatusLine::retryClicked, this, [this]() {
        requestPage(0);
    });

    m_cardRows = new QskLinearBox(Qt::Vertical, content);
    m_cardRows->setSpacing(2);
    m_cardRows->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Minimum);

    // 安卓触摸坑（AGENTS.md 4.2）：滚动内容行点击走 QQuickTapHandler
    {
        auto* tap = new QQuickTapHandler(m_cardRows);
        tap->setGesturePolicy(QQuickTapHandler::DragThreshold);
        connect(tap, &QQuickTapHandler::singleTapped, this,
            [this](QEventPoint point, Qt::MouseButton) {
                const int idx = cardRowAt(point.position());
                if (idx < 0 || idx >= m_cards.size()) return;
                const auto& card = m_cards.at(idx);
                if (auto* pm = pageManager()) {
                    QVariantMap args;
                    args.insert("playlistId", card.id);
                    args.insert("title", card.name);
                    pm->open("tracks", args);
                }
            });
    }

    // ── 底部：加载更多（状态行已并入滚动内容）──
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
                m_status->setStatus(StatusLine::Error,
                    QString::fromUtf8("加载失败：%1").arg(err));
                m_moreBtn->setVisible(m_more);
                return;
            }
            m_cards += cards;
            m_more = more;
            if (m_cards.isEmpty())
                m_status->setStatus(StatusLine::Info, QString::fromUtf8("暂无歌单"));
            else
                m_status->clearStatus();
            m_moreBtn->setVisible(m_more);
            rebuildCards();
        });

    requestPage(0);
}

void PlaylistListPage::requestPage(int offset)
{
    if (m_loading) return;
    m_loading = true;
    m_status->setStatus(StatusLine::Loading, QString::fromUtf8("正在加载歌单…"));
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

        auto* name = new QskTextLabel(elided(card.name, 16), row);
        name->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Preferred);
        name->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);

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

int PlaylistListPage::cardRowAt(const QPointF& localPos) const
{
    const auto rows = m_cardRows->childItems();
    for (int i = 0; i < rows.size(); ++i) {
        if (localPos.y() >= rows[i]->y()
            && localPos.y() < rows[i]->y() + rows[i]->height()) {
            return i;   // 本页行全部可点，直接返回行号
        }
    }
    return -1;
}
