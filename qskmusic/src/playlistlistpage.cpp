#include "playlistlistpage.h"
#include "pagemanager.h"
#include "statusline.h"
#include "iconitem.h"
#include "icongen.h"
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
    /* 计数单位走翻译：简中「亿/万」、英文「B/K」（QString::arg 直译占位） */
    if (n >= 100000000)
        return QCoreApplication::translate("PlaylistListPage", "%1B")
            .arg(double(n) / 100000000.0, 0, 'f', 1);
    if (n >= 10000)
        return QCoreApplication::translate("PlaylistListPage", "%1K")
            .arg(double(n) / 10000.0, 0, 'f', 1);
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
    m_radio = launchArgs.value("radio").toBool();
    m_title = launchArgs.value("title").toString();
    if (m_title.isEmpty())
        m_title = m_cat.isEmpty() ? tr("Hot Playlists") : m_cat;

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
                    if (m_radio) {
                        /* 站点电台：卡片 id 是电台 rid → 节目页 */
                        args.insert("rid", card.id);
                        args.insert("title", card.name);
                        pm->open("radioPrograms", args);
                    } else {
                        args.insert("playlistId", card.id);
                        args.insert("title", card.name);
                        pm->open("tracks", args);
                    }
                }
            });
    }

    // ── 底部：状态 + 加载更多 ──
    m_status = new StatusLine(layout);
    connect(m_status, &StatusLine::retryClicked, this, [this]() {
        requestPage(0);
    });
    m_moreBtn = new QskPushButton(tr("Load more"), layout);
    m_moreBtn->setPreferredHeight(44);
    m_moreBtn->setVisible(false);
    connect(m_moreBtn, &QskAbstractButton::clicked, this, [this]() {
        requestPage(m_cards.size());
    });

    connect(&NcmCatalog::instance(), &NcmCatalog::topPlaylistsDone, this,
        [this](const QList<PlaylistCard>& cards, bool more, const QString& err) {
            if (m_radio) return;      // 电台模式走 djHotDone
            if (!m_loading) return;   // 非本页在途请求
            m_loading = false;
            if (!err.isEmpty()) {
                m_status->setStatus(StatusLine::Error,
                    tr("Load failed: %1").arg(err));
                m_moreBtn->setVisible(m_more);
                return;
            }
            m_cards += cards;
            m_more = more;
            if (m_cards.isEmpty())
                m_status->setStatus(StatusLine::Info, tr("No playlists"));
            else
                m_status->clearStatus();
            m_moreBtn->setVisible(m_more);
            rebuildCards();
        });

    connect(&NcmCatalog::instance(), &NcmCatalog::djHotDone, this,
        [this](const QList<DjRadio>& radios, const QString& err) {
            if (!m_radio || !m_loading) return;
            m_loading = false;
            if (!err.isEmpty()) {
                m_status->setStatus(StatusLine::Error,
                    tr("Load failed: %1").arg(err));
                m_moreBtn->setVisible(m_more);
                return;
            }
            for (const auto& r : radios) {
                PlaylistCard c;
                c.id = r.id;
                c.name = r.name;
                c.creator = r.nickname;
                c.cover = r.cover;
                c.trackCount = r.programCount;
                m_cards << c;
            }
            if (m_cards.isEmpty())
                m_status->setStatus(StatusLine::Info, tr("No radios"));
            else
                m_status->clearStatus();
            m_moreBtn->setVisible(false);   // 电台首页一次 30 条，不滚动分页
            rebuildCards();
        });

    requestPage(0);
}

void PlaylistListPage::requestPage(int offset)
{
    if (m_loading) return;
    m_loading = true;
    m_status->setStatus(StatusLine::Loading,
        m_radio ? tr("Loading radio…") : tr("Loading playlists…"));
    m_moreBtn->setVisible(false);
    if (offset == 0) {
        m_cards.clear();
        m_more = false;
    }
    if (m_radio) {
        /* 站点电台：热门电台（cateId=0 全部，分页 30/次） */
        NcmCatalog::instance().loadDjHotAsync(0, 30, offset);
        return;
    }
    // 热门哨兵：服务端 cat="全部"（qlmusic 同口径）
    const QString cat = m_cat.isEmpty() ? tr("All") : m_cat;
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

        auto* icon = new IconItem(row);
        icon->setPreferredSize(48, 48);
        icon->setCornerRadius(8);
        icon->setFallback(qximage::identicon(QString::number(card.id), IconItem::pxForDp(48)));
        icon->setSource(card.cover, 48);

        auto* name = new QskTextLabel(elided(card.name, 16), row);
        name->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Preferred);
        name->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);

        QString sub;
        if (!card.creator.isEmpty())
            sub = elided(card.creator, 8) + QString::fromUtf8(" · ");
        if (m_radio)
            sub += tr("%1 programs").arg(card.trackCount);
        else {
            sub += tr("%1 tracks").arg(card.trackCount);
            sub += tr(" · %1 plays").arg(fmtCount(card.playCount));
        }
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
