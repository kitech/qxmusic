#include "songdetailpage.h"
#include "playback.h"
#include "pagemanager.h"
#include "iconitem.h"
#include "icongen.h"
#include <QskLinearBox.h>
#include <QskTextLabel.h>
#include <QskPushButton.h>
#include <QskFontRole.h>
#include <QskTextOptions.h>

SongDetailPage::SongDetailPage(QQuickItem* parent)
    : Page(parent)
{
}

void SongDetailPage::onCreate(const QVariantMap& args, const QVariantMap&)
{
    m_songId = args.value(QStringLiteral("songId")).toLongLong();

    setAutoLayoutChildren(true);
    auto* layout = new QskLinearBox(Qt::Vertical, this);
    layout->setPanel(true);

    /* ── TopBar：返回 + 标题 ── */
    auto* topBar = new QskLinearBox(Qt::Horizontal, layout);
    topBar->setPanel(true);
    topBar->setPreferredHeight(56);
    auto* backBtn = new QskPushButton(QString::fromUtf8("←"), topBar);
    backBtn->setPreferredSize(44, 44);
    auto* barTitle = new QskTextLabel(tr("Song detail"), topBar);
    barTitle->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Preferred);
    barTitle->setAlignment(Qt::AlignCenter);
    connect(backBtn, &QskAbstractButton::clicked, this, [this]() { finish(); });

    /* ── 封面（居中）── */
    auto* coverRow = new QskLinearBox(Qt::Horizontal, layout);
    coverRow->setPreferredHeight(220);
    coverRow->setDefaultAlignment(Qt::AlignHCenter);
    m_cover = new IconItem(coverRow);
    m_cover->setPreferredSize(200, 200);
    m_cover->setCornerRadius(12);

    /* ── 歌名 ── */
    m_title = new QskTextLabel(layout);
    m_title->setAlignment(Qt::AlignHCenter);
    m_title->setFontRole(QskFontRole(QskFontRole::Title, QskFontRole::High));

    /* ── 歌手（按钮形态：QSkinny 里只有按钮能稳定收到点击）── */
    auto* artistRow = new QskLinearBox(Qt::Horizontal, layout);
    artistRow->setDefaultAlignment(Qt::AlignHCenter);
    m_artistBtn = new QskPushButton(artistRow);
    connect(m_artistBtn, &QskAbstractButton::clicked, this, [this]() {
        /* 恢复队列快照不含 artistIds：id=0 也允许进入（页面降级为仅名字） */
        if (m_artistName.isEmpty() || !pageManager()) return;
        pageManager()->open(QStringLiteral("artist"),
            { { QStringLiteral("artistId"), m_artistId },
              { QStringLiteral("name"), m_artistName } });
    });

    /* ── 专辑 · 时长 ── */
    m_meta = new QskTextLabel(layout);
    m_meta->setAlignment(Qt::AlignHCenter);
    m_meta->setFontRole(QskFontRole(QskFontRole::Caption, QskFontRole::Normal));

    /* ── 统计行：播放数无单曲公开口径统一 --；评论数异步回填 ── */
    auto* statsRow = new QskLinearBox(Qt::Horizontal, layout);
    statsRow->setDefaultAlignment(Qt::AlignHCenter);
    statsRow->setSpacing(12);
    m_stats = new QskTextLabel(statsRow);
    m_stats->setFontRole(QskFontRole(QskFontRole::Caption, QskFontRole::Normal));
    m_commentBtn = new QskPushButton(tr("Comments"), statsRow);
    connect(m_commentBtn, &QskAbstractButton::clicked, this, [this]() {
        if (m_songId <= 0 || !pageManager()) return;
        pageManager()->open(QStringLiteral("comments"), {
            { QStringLiteral("songId"), m_songId },
            { QStringLiteral("title"), m_title ? m_title->text() : QString() }
        });
    });
    updateStats();

    /* ── 歌词 ── */
    m_lyric = new QskTextLabel(layout);
    m_lyric->setFontRole(QskFontRole(QskFontRole::Caption, QskFontRole::Normal));
    m_lyric->setWrapMode(QskTextOptions::Wrap);
    m_lyric->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Preferred);

    /* 数据：优先取当前播放曲（不在队列里时也能显示） */
    const auto& pb = Playback::instance();
    const Song* cur = pb.currentSong();
    if (cur && (m_songId <= 0 || cur->id == m_songId)) {
        applySong(*cur);
    } else {
        for (const auto& s : pb.queue()) {
            if (s.id == m_songId) { applySong(s); break; }
        }
    }

    /* 评论：异步拉取，总数回填统计行（站点分流由 NcmApi 内部处理） */
    connect(&NcmApi::instance(), &NcmApi::commentsDone, this,
        [this](qint64 id, const QList<Comment>&, int total, const QString&) {
            if (m_destroyed || id != m_songId) return;
            m_commentCount = total;
            updateStats();
        });
    if (m_songId > 0)
        NcmApi::instance().commentsAsync(m_songId, 20, 0);
}

void SongDetailPage::onDestroy()
{
    m_destroyed = true;   /* 网络回调晚到不再触碰已销毁控件 */
    Page::onDestroy();
}

void SongDetailPage::updateStats()
{
    if (!m_stats) return;
    const QString cmt = m_commentCount < 0
        ? QStringLiteral("…") : QString::number(m_commentCount);
    /* 文本越界防护（AGENTS.md 4.4） */
    m_stats->setText(QStringLiteral("▷ --    💬 %1").arg(cmt));
}

/* LRC → 纯文本：剥掉 [mm:ss.xx] / [ti:..] 标签与空行（详情页只做可读展示） */
static QString lrcToPlain(const QString& lrc)
{
    QString out;
    const QStringList lines = lrc.split(QLatin1Char('\n'));
    for (const QString& raw : lines) {
        QString line = raw;
        while (!line.isEmpty() && line.startsWith(QLatin1Char('['))) {
            const int e = line.indexOf(QLatin1Char(']'));
            if (e < 0) break;
            line = line.mid(e + 1);
        }
        line = line.trimmed();
        if (line.isEmpty()) continue;
        if (!out.isEmpty()) out += QLatin1Char('\n');
        out += line;
    }
    return out;
}

void SongDetailPage::applySong(const Song& s)
{
    m_artistId = s.artistIds.isEmpty() ? 0 : s.artistIds.first();
    m_artistName = s.artists;
    if (m_cover) {
        m_cover->setSource(s.cover, 200);
        m_cover->setFallback(qximage::identicon(QString::number(s.id), IconItem::pxForDp(200)));
    }
    if (m_title) m_title->setText(s.name);
    if (m_artistBtn) m_artistBtn->setText(s.artists);
    if (m_meta) {
        QString meta = s.album;
        if (s.durationMs > 0) {
            const int sec = s.durationMs / 1000;
            const QString d = QStringLiteral("%1:%2").arg(sec / 60)
                                  .arg(sec % 60, 2, 10, QLatin1Char('0'));
            if (!meta.isEmpty()) meta += QStringLiteral(" · ");
            meta += d;
        }
        m_meta->setText(meta);
    }
    if (m_lyric) {
        const QString plain = lrcToPlain(Playback::instance().lrcText());
        m_lyric->setText(plain.isEmpty() ? tr("No lyrics") : plain);
    }
}
