#include "commentpage.h"
#include <QskLinearBox.h>
#include <QskTextLabel.h>
#include <QskPushButton.h>
#include <QskScrollArea.h>
#include <QskFontRole.h>
#include <QskTextOptions.h>
#include <QDateTime>
#include <QDebug>

namespace {

/* 文本永不越 item 边界（AGENTS.md 4.4）：代码级截断 */
QString elided(const QString& t, int n)
{
    return t.size() <= n ? t : t.left(n - 1) + QStringLiteral("…");
}

QString commentDate(qint64 ms)
{
    return QDateTime::fromMSecsSinceEpoch(ms).toString(QStringLiteral("yyyy-MM-dd"));
}

}

CommentPage::CommentPage(QQuickItem* parent)
    : Page(parent)
{
}

void CommentPage::onCreate(const QVariantMap& args, const QVariantMap&)
{
    m_songId = args.value(QStringLiteral("songId")).toLongLong();
    m_title = args.value(QStringLiteral("title")).toString();

    setAutoLayoutChildren(true);
    auto* layout = new QskLinearBox(Qt::Vertical, this);
    layout->setPanel(true);

    /* ── TopBar：返回 + 标题 ── */
    auto* topBar = new QskLinearBox(Qt::Horizontal, layout);
    topBar->setPanel(true);
    topBar->setPreferredHeight(56);
    auto* backBtn = new QskPushButton(QString::fromUtf8("←"), topBar);
    backBtn->setPreferredSize(44, 44);
    auto* barTitle = new QskTextLabel(tr("Comments"), topBar);
    barTitle->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Preferred);
    barTitle->setAlignment(Qt::AlignCenter);
    connect(backBtn, &QskAbstractButton::clicked, this, [this]() { finish(); });

    /* ── 状态行（总数 / 加载中 / 错误）── */
    m_status = new QskTextLabel(tr("Loading…"), layout);
    m_status->setFontRole(QskFontRole(QskFontRole::Caption, QskFontRole::Normal));

    /* ── 评论列表（单棵线性树 + QskScrollArea，无内嵌 stackBox）── */
    m_rows = new QskLinearBox(Qt::Vertical);
    m_rows->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Minimum);
    auto* scroll = new QskScrollArea(layout);
    scroll->setScrolledItem(m_rows);
    scroll->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Expanding);

    /* 异步回调：晚到 / 别家 id 丢弃（Transient 页每次打开数据新鲜） */
    connect(&NcmApi::instance(), &NcmApi::commentsDone, this,
        [this](qint64 id, const QList<Comment>& comments, int total,
               const QString& err) {
            if (m_destroyed || id != m_songId) return;
            if (!err.isEmpty()) {
                m_status->setText(err);
                return;
            }
            m_comments = comments;
            m_status->setText(tr("%1 comments").arg(total));
            buildRows();
        });

    if (m_songId > 0)
        NcmApi::instance().commentsAsync(m_songId, 20, 0);
    else
        m_status->setText(tr("No comments"));
}

void CommentPage::onDestroy()
{
    m_destroyed = true;   /* 网络回调晚到不再触碰已销毁控件 */
    Page::onDestroy();
}

void CommentPage::buildRows()
{
    if (!m_rows) return;
    const auto children = m_rows->childItems();
    for (QQuickItem* c : children)
        c->deleteLater();

    if (m_comments.isEmpty()) {
        m_status->setText(tr("No comments"));
        return;
    }

    for (const auto& c : m_comments) {
        auto* box = new QskLinearBox(Qt::Vertical, m_rows);
        box->setSpacing(2);
        const QString head = QStringLiteral("%1  ·  %2  ·  ▷%3  ·  💬%4")
            .arg(elided(c.nickname, 12), commentDate(c.timeMs))
            .arg(c.likedCount).arg(c.replyCount);
        auto* h = new QskTextLabel(head, box);
        h->setFontRole(QskFontRole(QskFontRole::Caption, QskFontRole::Normal));
        auto* body = new QskTextLabel(elided(c.content, 60), box);
        body->setFontRole(QskFontRole(QskFontRole::Caption, QskFontRole::Normal));
        body->setWrapMode(QskTextOptions::Wrap);
        body->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Preferred);
    }
}
