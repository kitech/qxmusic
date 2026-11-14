#include "netradiopage.h"
#include "radiolist.h"
#include "pagemanager.h"
#include "toastpopup.h"
#include "audioplayer.h"

#include <QskLinearBox.h>
#include <QskTextLabel.h>
#include <QskPushButton.h>
#include <QskScrollArea.h>
#include <QskSeparator.h>
#include <QskFontRole.h>
#include <QUrl>
#include <private/qquicktaphandler_p.h>

namespace {

QString elided(const QString& t, int n)
{
    return t.size() > n ? t.left(n - 1) + QString::fromUtf8("…") : t;
}

/* 地区 key → 英文源文本（qsk tr 走 QTranslator 英文源→译文） */
QString regionLabel(const QString& regionKey)
{
    if (regionKey == QStringLiteral("china"))         return QObject::tr("China");
    if (regionKey == QStringLiteral("hongkong"))     return QObject::tr("HongKong");
    if (regionKey == QStringLiteral("asia"))         return QObject::tr("Asia");
    return QObject::tr("International");
}

} // namespace

NetRadioPage::NetRadioPage(QQuickItem* parent)
    : Page(parent)
{
}

void NetRadioPage::onCreate(const QVariantMap& launchArgs, const QVariantMap&)
{
    const QString region = launchArgs.value("region").toString();
    const QString title = launchArgs.value("title").toString();
    const QString headerTitle = title.isEmpty()
        ? regionLabel(region) : title;

    setAutoLayoutChildren(true);
    m_layout = new QskLinearBox(Qt::Vertical, this);
    m_layout->setPanel(true);

    // ── 顶栏：返回 + 标题 ──
    auto* topBar = new QskLinearBox(Qt::Horizontal, m_layout);
    topBar->setPanel(true);
    topBar->setPreferredHeight(56);

    auto* backBtn = new QskPushButton(QString::fromUtf8("←"), topBar);
    connect(backBtn, &QskAbstractButton::clicked, this, [this]() {
        if (auto* pm = pageManager()) pm->back();
    });

    auto* header = new QskTextLabel(elided(headerTitle, 12), topBar);
    header->setFontRole(QskFontRole(QskFontRole::Title, QskFontRole::Normal));
    header->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Preferred);
    header->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);

    new QskSeparator(Qt::Horizontal, m_layout);

    // ── 地区内电台行列表（region 必传：来自首页地区 chip）──
    {
        const auto list = radiolist::byRegion(region);
        auto* rows = new QskLinearBox(Qt::Vertical);
        rows->setSpacing(2);
        rows->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Minimum);
        auto* scroll = new QskScrollArea(m_layout);
        scroll->setScrolledItem(rows);
        scroll->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Expanding);

        for (int i = 0; i < list.size(); ++i) {
            const auto& s = list.at(i);

            auto* row = new QskLinearBox(Qt::Horizontal, rows);
            row->setPreferredHeight(48);

            auto* marker = new QskTextLabel(QString(), row);
            marker->setPreferredWidth(14);
            marker->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
            marker->setTextColor(QColor(0x3d, 0x8b, 0xfd));

            auto* label = new QskTextLabel(
                elided(QString::fromUtf8(s.name), 24)
                    + (s.hls ? QString::fromUtf8("  · HLS") : QString()),
                row);
            label->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Preferred);
            label->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
            if (s.hls)
                label->setOpacity(0.45);   /* 灰态：HLS 暂不支持 */

            auto* playBtn = new QskPushButton(QString::fromUtf8("▶"), row);
            playBtn->setPreferredSize(30, 30);

            auto play = [this, marker, region, s]() {
                if (s.hls) {
                    ToastPopup::show(this, tr("HLS streams not supported yet"));
                    return;
                }
                marker->setText(QString::fromUtf8("▶"));
                ToastPopup::show(this,
                    tr("Playing radio: %1").arg(elided(QString::fromUtf8(s.name), 10)));
                AudioPlayer::instance()->playUrl(
                    QString::fromUtf8("netradio-%1").arg(region),   // region 唯一即可，避免跨页 id 撞车
                    QUrl(QString::fromUtf8(s.url)));
            };
            connect(playBtn, &QskAbstractButton::clicked, this, play);

            auto* tap = new QQuickTapHandler(row);
            tap->setGesturePolicy(QQuickTapHandler::DragThreshold);
            connect(tap, &QQuickTapHandler::singleTapped, this, play);
        }
    }
}
