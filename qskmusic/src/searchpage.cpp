#include "searchpage.h"
#include "pagemanager.h"
#include "playerbar.h"
#include "toastpopup.h"
#include "logmodel.h"
#include "audioplayer.h"
#include <QskLinearBox.h>
#include <QskTextLabel.h>
#include <QskPushButton.h>
#include <QskScrollArea.h>
#include <QskTextField.h>
#include <QskSeparator.h>
#include <QskFontRole.h>
#include <QUrl>

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
    title->setSizePolicy(QskSizePolicy::Preferred, QskSizePolicy::Preferred);
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

    m_playerBar = new PlayerBar(layout);

    // ── 数据 / 播放接线 ──
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

    connect(&NcmApi::instance(), &NcmApi::resolveDone, this,
        [this](qint64 songId, const QString& url, const QString& err) {
            m_resolving = false;
            if (songId != currentSongId()) return;
            if (!err.isEmpty() || url.isEmpty()) {
                ToastPopup::show(this, QString::fromUtf8("获取播放链接失败：%1").arg(err));
                return;
            }
            AudioPlayer::instance()->playUrl(QString::number(songId), QUrl(url));
            m_playerBar->setPlaying(true);
        });

    auto* ap = AudioPlayer::instance();
    connect(ap, &AudioPlayer::finished, this, [this](const QString& id) {
        m_playerBar->setPlaying(false);
        if (id.toLongLong() != currentSongId()) return;
        playSong(m_current + 1);
    });
    connect(ap, &AudioPlayer::positionChanged, this,
        [this](qint64 ms) { m_playerBar->setProgressMs(int(ms)); });
    connect(ap, &AudioPlayer::durationChanged, this,
        [this](qint64 ms) { m_playerBar->setDurationMs(int(ms)); });

    connect(m_playerBar, &PlayerBar::playPauseRequested, this, [this]() {
        auto* ap = AudioPlayer::instance();
        if (!ap->playingId().isEmpty()) {
            ap->pause();
            m_playerBar->setPlaying(false);
        } else if (!ap->pausedId().isEmpty()) {
            ap->resume();
            m_playerBar->setPlaying(true);
        } else if (m_current >= 0) {
            playSong(m_current);
        }
    });
    connect(m_playerBar, &PlayerBar::prevRequested, this, [this]() {
        playSong(m_current - 1);
    });
    connect(m_playerBar, &PlayerBar::nextRequested, this, [this]() {
        playSong(m_current + 1);
    });
    connect(m_playerBar, &PlayerBar::volumeClicked, this, [this]() {
        ToastPopup::show(m_playerBar, QString::fromUtf8("音量弹层：下轮实装"));
    });

    // 打开即搜默认词（保留原搜索自动行为，不再是启动首屏）
    NcmApi::instance().searchAsync(QStringLiteral("周杰伦"), 30);
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
            s.artists.isEmpty() ? s.name
                                : QString("%1 - %2").arg(s.name, s.artists), row);
        name->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Preferred);
        name->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);

        const int total = s.durationMs / 1000;
        auto* dur = new QskTextLabel(
            QString("%1:%2").arg(total / 60).arg(total % 60, 2, 10, QLatin1Char('0')), row);
        dur->setPreferredWidth(48);
        dur->setAlignment(Qt::AlignVCenter | Qt::AlignRight);

        auto* playBtn = new QskPushButton(QString::fromUtf8("▶"), row);
        playBtn->setPreferredSize(32, 32);
        connect(playBtn, &QskAbstractButton::clicked, this, [this, i]() {
            playSong(i);
        });
    }
    m_rows->update();
}

void SearchPage::playSong(int index)
{
    if (index < 0 || index >= m_songs.size()) return;
    const auto& s = m_songs.at(index);
    if (!s.playable) {
        ToastPopup::show(this, QString::fromUtf8("灰色歌曲，无可播资源"));
        return;
    }
    if (m_resolving) return;
    m_current = index;
    m_resolving = true;
    m_playerBar->setDurationMs(s.durationMs);
    m_playerBar->setProgressMs(0);
    LogModel::instance().append(LogModel::Info, "player",
        QString("play: %1 - %2").arg(s.name, s.artists));
    NcmApi::instance().resolveAsync(s.id, NCM_Q_EXHIGH);
}

qint64 SearchPage::currentSongId() const
{
    return (m_current >= 0 && m_current < m_songs.size())
        ? m_songs.at(m_current).id : 0;
}
