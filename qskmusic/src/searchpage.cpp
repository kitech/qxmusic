#include "searchpage.h"
#include "pagemanager.h"
#include "playback.h"
#include <private/qquicktaphandler_p.h>
#include <QskLinearBox.h>
#include <QskTextLabel.h>
#include <QskPushButton.h>
#include <QskScrollArea.h>
#include <QskTextField.h>
#include <QskSeparator.h>
#include <QskFontRole.h>

namespace {

QString elided(const QString& text, int maxChars)
{
    return text.size() > maxChars
        ? text.left(maxChars - 1) + QString::fromUtf8("…") : text;
}

} // namespace

SearchPage::SearchPage(QQuickItem* parent)
    : Page(parent)
{
}

void SearchPage::onCreate(const QVariantMap&, const QVariantMap&)
{
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
    auto* title = new QskTextLabel(QString::fromUtf8("搜索"), topBar);
    title->setFontRole(QskFontRole(QskFontRole::Title, QskFontRole::Normal));
    title->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Preferred);
    title->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);

    new QskSeparator(Qt::Horizontal, layout);

    // ── 搜索行 ──
    auto* searchBar = new QskLinearBox(Qt::Horizontal, layout);
    searchBar->setMargins(8, 8, 8, 0);
    searchBar->setSpacing(8);
    m_field = new QskTextField(searchBar);
    m_field->setPlaceholderText(QStringLiteral("搜索歌曲、歌手…"));
    m_field->setFixedHeight(30);   // QSkinny 输入框后半高字坑：必须锁死高
    m_field->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Preferred);
    auto* searchBtn = new QskPushButton(QStringLiteral("搜索"), searchBar);
    connect(searchBtn, &QskAbstractButton::clicked, this, [this]() {
        const QString kw = m_field->text().trimmed();
        if (!kw.isEmpty())
            NcmApi::instance().searchAsync(kw, 30);
    });

    m_status = new QskTextLabel(layout);
    m_status->setPreferredHeight(32);
    m_status->setAlignment(Qt::AlignCenter);

    m_rows = new QskLinearBox(Qt::Vertical);
    m_rows->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Minimum);
    auto* scroll = new QskScrollArea(layout);
    scroll->setScrolledItem(m_rows);
    scroll->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Expanding);

    // 安卓触摸坑（AGENTS.md 4.2）：结果行点击走 QQuickTapHandler
    {
        auto* tap = new QQuickTapHandler(m_rows);
        tap->setGesturePolicy(QQuickTapHandler::DragThreshold);
        connect(tap, &QQuickTapHandler::singleTapped, this,
            [this](QEventPoint point, Qt::MouseButton) {
                const int idx = songRowAt(point.position());
                if (idx < 0 || idx >= m_songs.size()) return;
                Playback::instance().setQueue(m_songs, idx);
            });
    }

    connect(&NcmApi::instance(), &NcmApi::searchDone, this,
        [this](const QList<Song>& songs, const QString& err) {
            if (!err.isEmpty()) {
                m_status->setText(QString::fromUtf8("搜索失败：%1").arg(err));
                return;
            }
            m_songs = songs;
            m_status->setText(m_songs.isEmpty() ? QString::fromUtf8("无结果")
                                                : QString());
            rebuildList();
        });

    // 打开即搜默认词（保留原搜索自动行为，不占首屏）
    NcmApi::instance().searchAsync(QStringLiteral("周杰伦"), 30);

    // 实锤灰歌 → 本地行摘对号并重建（qlmusic 实时兜底同款）
    connect(&Playback::instance(), &Playback::playableChanged, this,
        [this](qint64 songId) {
            for (auto& s : m_songs) {
                if (s.id == songId && s.playable) {
                    s.playable = false;
                    rebuildList();
                    break;
                }
            }
        });
}

void SearchPage::rebuildList()
{
    const auto children = m_rows->childItems();
    for (auto* c : children) c->deleteLater();

    for (int i = 0; i < m_songs.size(); ++i) {
        const auto& s = m_songs.at(i);
        auto* row = new QskLinearBox(Qt::Horizontal, m_rows);
        row->setPreferredHeight(44);

        auto* num = new QskTextLabel(QString::number(i + 1), row);
        num->setPreferredWidth(36);
        num->setAlignment(Qt::AlignVCenter | Qt::AlignRight);

        auto* name = new QskTextLabel(
            s.artists.isEmpty()
                ? elided(s.name, 14)
                : elided(QString("%1 - %2").arg(s.name, s.artists), 20), row);
        name->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Preferred);
        name->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);

        const int total = s.durationMs / 1000;
        QString durText = QString("%1:%2").arg(total / 60)
            .arg(total % 60, 2, 10, QLatin1Char('0'));
        if (s.id > 0 && s.playable)
            durText += QString::fromUtf8(" ✓");   // 非灰歌对号（本地歌 id=0 不标）
        auto* dur = new QskTextLabel(durText, row);
        dur->setPreferredWidth(56);
        dur->setAlignment(Qt::AlignVCenter | Qt::AlignRight);
    }
    m_rows->update();
}

int SearchPage::songRowAt(const QPointF& localPos) const
{
    const auto rows = m_rows->childItems();
    for (int i = 0; i < rows.size(); ++i) {
        if (localPos.y() >= rows[i]->y()
            && localPos.y() < rows[i]->y() + rows[i]->height()) {
            return i;
        }
    }
    return -1;
}
