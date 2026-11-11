#include "searchpage.h"
#include "pagemanager.h"
#include "playback.h"
#include "statusline.h"
#include <private/qquicktaphandler_p.h>
#include <QColor>
#include <QskLinearBox.h>
#include <QskTextLabel.h>
#include <QskPushButton.h>
#include <QskScrollArea.h>
#include <QskTextField.h>
#include <QskTextInput.h>
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
    // 规则：首字符禁止空格（docs/空格键播控规则.md 配套 1）
    connect(m_field, &QskTextInput::textEdited, this,
        [this](const QString& text) {
            if (text.startsWith(QLatin1Char(' ')))
                m_field->setText(text.trimmed());
        });    connect(searchBtn, &QskAbstractButton::clicked, this, [this]() {
        const QString kw = m_field->text().trimmed();
        if (!kw.isEmpty()) {
            m_status->setStatus(StatusLine::Loading, QString::fromUtf8("正在搜索…"));
            NcmApi::instance().searchAsync(kw, 30);
        }
    });

    m_status = new StatusLine(layout);

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
                m_status->setStatus(StatusLine::Error,
                    QString::fromUtf8("搜索失败：%1").arg(err));
                return;
            }
            m_songs = songs;
            if (m_songs.isEmpty())
                m_status->setStatus(StatusLine::Info, QString::fromUtf8("无结果"));
            else
                m_status->clearStatus();
            rebuildList();
        });

    // 打开即搜默认词（保留原搜索自动行为，不占首屏）
    m_status->setStatus(StatusLine::Loading, QString::fromUtf8("正在搜索…"));
    NcmApi::instance().searchAsync(QStringLiteral("周杰伦"), 30);

    // 实锤灰歌…（保留原说明）
    // 正在播放 ▶：切歌只改文本
    connect(&Playback::instance(), &Playback::songChanged, this,
        [this]() { updatePlayingMarker(); });
}

void SearchPage::rebuildList()
{
    const auto children = m_rows->childItems();
    for (auto* c : children) c->deleteLater();
    m_playingMarkers.clear();

    for (int i = 0; i < m_songs.size(); ++i) {
        const auto& s = m_songs.at(i);
        auto* row = new QskLinearBox(Qt::Horizontal, m_rows);
        row->setPreferredHeight(44);

        // 正在播放标识列（▶ / 空，14dp）
        auto* marker = new QskTextLabel(QString(), row);
        marker->setPreferredWidth(14);
        marker->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
        marker->setTextColor(QColor(0x3d, 0x8b, 0xfd));
        m_playingMarkers.append({ s.id, marker });

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

        // 单独播放按钮（保留；安卓触摸由外层 TapHandler 兜底）
        auto* playBtn = new QskPushButton(QString::fromUtf8("▶"), row);
        playBtn->setPreferredSize(30, 30);
        connect(playBtn, &QskAbstractButton::clicked, this, [this, i]() {
            Playback::instance().setQueue(m_songs, i);
        });
    }
    updatePlayingMarker();
    m_rows->update();
}

void SearchPage::updatePlayingMarker()
{
    const qint64 playing = Playback::instance().currentSongId();
    for (auto& p : m_playingMarkers) {
        if (p.second)
            p.second->setText(p.first != 0 && p.first == playing
                ? QString::fromUtf8("▶") : QString());
    }
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
