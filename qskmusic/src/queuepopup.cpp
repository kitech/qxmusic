#include "queuepopup.h"
#include "iconitem.h"
#include "icongen.h"
#include <QskBox.h>
#include <QskBoxShapeMetrics.h>
#include <QskLinearBox.h>
#include <QskTextLabel.h>
#include <QskPushButton.h>
#include <QskFontRole.h>
#include <QskScrollArea.h>
#include <QColor>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTimer>

namespace {

QRectF popupParentRect(QQuickItem* parent)
{
    if (!parent)
        return {};
    if (auto* w = parent->window())
        return QRectF(QPointF(), w->size());
    return QRectF(-parent->x(), -parent->y(),
                  parent->width(), parent->height());
}

} // namespace

QueuePopup::QueuePopup(QQuickItem* parent)
    : QskPopup(parent)
{
    setModal(true);
    setOverlay(true);
    setPopupFlag(QskPopup::DeleteOnClose, false);
    setPolishOnResize(true);
    setPolishOnParentResize(true);

    m_panel = new QskBox(this);
    m_panel->setBoxShapeHint(QskBox::Panel,
        QskBoxShapeMetrics(14, Qt::AbsoluteSize));

    m_layout = new QskLinearBox(Qt::Vertical, m_panel);
    m_layout->setSpacing(8);
    m_layout->setMargins(18);

    // ── 标题行 ──
    auto* titleRow = new QskLinearBox(Qt::Horizontal, m_layout);
    titleRow->setSpacing(8);

    auto* title = new QskTextLabel(tr("Play Queue"), titleRow);
    title->setFontRole(QskFontRole::Title);
    title->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Preferred);

    auto* closeBtn = new QskPushButton(QStringLiteral("✕"), titleRow);
    connect(closeBtn, &QskAbstractButton::clicked, this, &QskPopup::close);

    // ── 歌曲行列表（QskScrollArea + 内容盒 Expanding/Minimum 范式）──
    m_rowsBox = new QskLinearBox(Qt::Vertical);
    m_rowsBox->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Minimum);

    auto* scroll = new QskScrollArea(m_layout);
    scroll->setScrolledItem(m_rowsBox);
    scroll->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Expanding);
    scroll->setPreferredHeight(360);

    QTimer::singleShot(0, this, [this]() { updateGeometry(); });
}

void QueuePopup::setTracks(const QStringList& titles)
{
    m_titles = titles;
    m_currentIndex = -1;
    rebuildRows();
}

void QueuePopup::setCurrentIndex(int idx)
{
    m_currentIndex = idx;
    rebuildRows();
}

void QueuePopup::setTrackCovers(const QStringList& covers, const QStringList& seeds)
{
    m_covers = covers;
    m_seeds = seeds;
}

void QueuePopup::rebuildRows()
{
    const auto children = m_rowsBox->childItems();
    for (auto* c : children) c->deleteLater();

    for (int i = 0; i < m_titles.size(); ++i) {
        auto* row = new QskLinearBox(Qt::Horizontal, m_rowsBox);
        row->setPreferredHeight(40);

        const QString cover = i < m_covers.size() ? m_covers.at(i) : QString();
        const QString seed = i < m_seeds.size() ? m_seeds.at(i)
                                                : QString::number(i);
        auto* icon = new IconItem(row);
        icon->setPreferredSize(32, 32);
        icon->setCornerRadius(6);
        icon->setFallback(qximage::identicon(seed, IconItem::pxForDp(32)));
        icon->setSource(cover, 32);

        // 当前播放行：序号前 ▶（蓝色标识列）
        auto* marker = new QskTextLabel(
            i == m_currentIndex ? QString::fromUtf8("▶") : QString(), row);
        marker->setPreferredWidth(14);
        marker->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
        marker->setTextColor(QColor(0x3d, 0x8b, 0xfd));

        auto* num = new QskTextLabel(QString::number(i + 1), row);
        num->setPreferredWidth(28);
        num->setAlignment(Qt::AlignVCenter | Qt::AlignRight);

        auto* name = new QskTextLabel(m_titles.at(i), row);
        name->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Preferred);
        name->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);

        auto* playBtn = new QskPushButton(QString::fromUtf8("▶"), row);
        playBtn->setPreferredSize(30, 30);
        connect(playBtn, &QskAbstractButton::clicked, this, [this, i]() {
            Q_EMIT trackSelected(i);
            close();
        });
    }
    m_rowsBox->update();
}

void QueuePopup::updateLayout()
{
    updateGeometry();
    m_layout->setGeometry(layoutRect());
}

void QueuePopup::updateGeometry()
{
    const auto parentRect = popupParentRect(parentItem());
    if (parentRect.isEmpty())
        return;

    const auto hint = m_layout->effectiveSizeHint(Qt::PreferredSize, QSizeF());
    const qreal maxW = qMin(0.5 * parentRect.width(), 420.0);
    const qreal maxH = 0.85 * parentRect.height();

    const qreal panelW = qBound(300.0, hint.width() + 36, maxW);
    const qreal panelH = qBound(320.0, hint.height() + 36, maxH);

    QRectF r(0, 0, panelW, panelH);
    r.moveCenter(parentRect.center());
    setGeometry(r);
    m_panel->setGeometry(r.translated(-r.topLeft()));
}

#include "moc_queuepopup.cpp"
