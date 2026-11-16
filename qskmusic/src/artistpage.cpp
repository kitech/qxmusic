#include "artistpage.h"
#include "playback.h"
#include "iconitem.h"
#include "icongen.h"
#include <QskLinearBox.h>
#include <QskTextLabel.h>
#include <QskPushButton.h>
#include <QskFontRole.h>
#include <QDebug>

ArtistPage::ArtistPage(QQuickItem* parent)
    : Page(parent)
{
}

void ArtistPage::onCreate(const QVariantMap& args, const QVariantMap&)
{
    m_artistId = args.value(QStringLiteral("artistId")).toLongLong();
    m_name = args.value(QStringLiteral("name")).toString();

    setAutoLayoutChildren(true);
    auto* layout = new QskLinearBox(Qt::Vertical, this);
    layout->setPanel(true);

    /* ── TopBar ── */
    auto* topBar = new QskLinearBox(Qt::Horizontal, layout);
    topBar->setPanel(true);
    topBar->setPreferredHeight(56);
    auto* backBtn = new QskPushButton(QString::fromUtf8("←"), topBar);
    backBtn->setPreferredSize(44, 44);
    auto* barTitle = new QskTextLabel(tr("Artist"), topBar);
    barTitle->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Preferred);
    barTitle->setAlignment(Qt::AlignCenter);
    connect(backBtn, &QskAbstractButton::clicked, this, [this]() { finish(); });

    /* ── 头部：圆形头像 + 名字 ── */
    auto* head = new QskLinearBox(Qt::Horizontal, layout);
    head->setPreferredHeight(112);
    m_avatar = new IconItem(head);
    m_avatar->setPreferredSize(96, 96);
    m_avatar->setShape(IconItem::Circle);
    m_avatar->setFallback(qximage::identicon(
        m_name.isEmpty() ? QString::number(m_artistId) : m_name,
        IconItem::pxForDp(96)));
    auto* nameCol = new QskLinearBox(Qt::Vertical, head);
    nameCol->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Preferred);
    auto* n = new QskTextLabel(m_name, nameCol);
    n->setFontRole(QskFontRole(QskFontRole::Title, QskFontRole::High));
    m_status = new QskTextLabel(tr("Loading…"), nameCol);
    m_status->setFontRole(QskFontRole(QskFontRole::Caption, QskFontRole::Normal));

    /* ── 歌曲列表 ── */
    m_rows = new QskLinearBox(Qt::Vertical, layout);
    m_rows->setSpacing(0);

    /* 网络结果 */
    connect(&NcmApi::instance(), &NcmApi::artistDone, this,
        [this](qint64 id, const QString& pic, const QString&) {
            if (m_destroyed || id != m_artistId || pic.isEmpty() || !m_avatar) return;
            m_avatar->setSource(pic, 96);
        });
    connect(&NcmApi::instance(), &NcmApi::artistSongsDone, this,
        [this](qint64 id, const QList<Song>& songs, const QString& err) {
            if (m_destroyed || id != m_artistId) return;   /* 晚到/别家歌手：丢弃 */
            if (!err.isEmpty()) { showLoading(false, err); return; }
            m_songs = songs;
            showLoading(false);
            buildRows();
        });

    if (m_artistId > 0) {
        NcmApi::instance().artistAsync(m_artistId);
        NcmApi::instance().artistSongsAsync(m_artistId, 50, 0);
    } else {
        showLoading(false, tr("No artist info"));
    }
}

void ArtistPage::onDestroy()
{
    m_destroyed = true;   /* 网络回调晚到不再触碰已销毁控件 */
    Page::onDestroy();
}

void ArtistPage::showLoading(bool on, const QString& err)
{
    if (!m_status) return;
    if (!err.isEmpty()) {
        m_status->setText(err);
        return;
    }
    m_status->setText(on ? tr("Loading…")
                         : tr("%1 hot songs").arg(m_songs.size()));
}

void ArtistPage::buildRows()
{
    if (!m_rows) return;
    /* 清空旧行（重复拉取时） */
    const auto children = m_rows->childItems();
    for (QQuickItem* c : children) {
        if (c != m_status) c->deleteLater();
    }
    for (int i = 0; i < m_songs.size(); ++i) {
        const Song& s = m_songs.at(i);
        const int sec = s.durationMs / 1000;
        const QString text = QStringLiteral("%1. %2 — %3   %4:%5")
            .arg(i + 1).arg(s.name, s.artists)
            .arg(sec / 60).arg(sec % 60, 2, 10, QLatin1Char('0'));
        auto* row = new QskPushButton(text, m_rows);
        row->setPreferredHeight(44);
        QList<Song> snapshot = m_songs;
        connect(row, &QskAbstractButton::clicked, this, [this, snapshot, i]() {
            Playback::instance().setQueue(snapshot, i);
        });
    }
    showLoading(false);
}
