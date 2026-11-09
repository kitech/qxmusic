#include "recview.h"
#include <QskScrollArea.h>
#include <QskTextLabel.h>
#include <QskPushButton.h>

namespace {

QString fmtCount(qint64 n)
{
    if (n >= 100000000)
        return QString("%1亿").arg(double(n) / 100000000.0, 0, 'f', 1);
    if (n >= 10000)
        return QString("%1万").arg(double(n) / 10000.0, 0, 'f', 1);
    return QString::number(n);
}

} // namespace

RecView::RecView(QQuickItem* parent)
    : QskLinearBox(Qt::Vertical, parent)
{
    setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Expanding);

    m_cardRows = new QskLinearBox(Qt::Vertical);
    m_cardRows->setSpacing(2);
    m_cardRows->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Minimum);
    auto* scroll = new QskScrollArea(this);
    scroll->setScrolledItem(m_cardRows);
    scroll->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Expanding);

    m_status = new QskTextLabel(this);
    m_status->setPreferredHeight(32);
    m_status->setAlignment(Qt::AlignCenter);

    m_moreBtn = new QskPushButton(QString::fromUtf8("加载更多"), this);
    m_moreBtn->setPreferredHeight(44);
    m_moreBtn->setVisible(false);
    connect(m_moreBtn, &QskAbstractButton::clicked, this, [this]() {
        requestPage(m_cards.size());
    });

    connect(&NcmCatalog::instance(), &NcmCatalog::topPlaylistsDone, this,
        [this](const QList<PlaylistCard>& cards, bool more, const QString& err) {
            if (!m_loading) return;   // 非本视图在途请求
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

void RecView::reload()
{
    requestPage(0);
}

void RecView::requestPage(int offset)
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

void RecView::rebuildCards()
{
    const auto children = m_cardRows->childItems();
    for (auto* c : children) c->deleteLater();

    for (const auto& card : m_cards) {
        auto* row = new QskLinearBox(Qt::Horizontal, m_cardRows);
        row->setPreferredHeight(56);
        row->setSpacing(8);

        auto* openBtn = new QskPushButton(card.name, row);
        openBtn->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Preferred);
        connect(openBtn, &QskAbstractButton::clicked, this, [this, card]() {
            Q_EMIT playlistSelected(card.id, card.name);
        });

        QString sub = card.creator;
        if (!sub.isEmpty()) sub += QString::fromUtf8(" · ");
        sub += QString::fromUtf8("%1 首").arg(card.trackCount);
        sub += QString::fromUtf8(" · 播放 %1").arg(fmtCount(card.playCount));
        auto* subLbl = new QskTextLabel(sub, row);
        subLbl->setPreferredWidth(120);
        subLbl->setSizePolicy(QskSizePolicy::Preferred, QskSizePolicy::Preferred);
        subLbl->setAlignment(Qt::AlignVCenter | Qt::AlignRight);
    }
    m_cardRows->update();
}
