#include "playerbar.h"
#include "progressline.h"
#include "iconitem.h"
#include "icongen.h"
#include "playback.h"
#include <QskPushButton.h>
#include <QskProgressBar.h>
#include <QskTextLabel.h>
#include <QskFontRole.h>
#include <QskBoxShapeMetrics.h>
#include <QskSizePolicy.h>
#include <private/qquickdraghandler_p.h>
#include <private/qquicktaphandler_p.h>
#include <QDebug>
#include <QTimer>

namespace {

// anystik stickerlist 范式：代码级截断，文本永不画出 item 边界
QString elided(const QString& t, int n)
{
    return t.size() > n ? t.left(n - 1) + QString::fromUtf8("…") : t;
}

constexpr int kSwipeThreshold = 36;   // 横划切歌阈值（dp）

} // namespace

PlayerBar::PlayerBar(QQuickItem* parent)
    : QskLinearBox(Qt::Horizontal, parent)
{
    setPanel(true);
    setFixedHeight(56);      // 两行文本高度
    setMargins(6);
    setSpacing(8);

    // ① icon（40×40，垂直居中于两行高；专辑封面，无图用 identicon）
    m_icon = new IconItem(this);
    m_icon->setPreferredSize(40, 40);
    m_icon->setCornerRadius(8);
    m_icon->setFallback(qximage::identicon(QStringLiteral("qskmusic"), IconItem::pxForDp(40)));

    // ② 文本列：歌名 / 歌手（两行；左右横划切上下一首）
    auto* textCol = new QskLinearBox(Qt::Vertical, this);
    textCol->setSpacing(0);
    textCol->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Preferred);

    m_title = new QskTextLabel(textCol);
    m_title->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Preferred);

    // 歌手行：20px 圆形头像 + 歌手文本（头像跟随当前播放曲目的首位歌手）
    auto* artistRow = new QskLinearBox(Qt::Horizontal, textCol);
    artistRow->setSpacing(4);

    m_artistAvatar = new IconItem(artistRow);
    m_artistAvatar->setPreferredSize(20, 20);
    m_artistAvatar->setShape(IconItem::Circle);
    m_artistAvatar->setVisible(false);

    m_artist = new QskTextLabel(QString::fromUtf8("左右横划切换上下一首"), artistRow);
    m_artist->setFontRole(QskFontRole(QskFontRole::Caption, QskFontRole::Normal));

    // resolve 中间态：慢网取播放链接可达数秒，期间歌手行显示提示（songChanged
    // 已先行显示歌名，切曲瞬间即有反馈）；resolveDone 恢复 m_lastArtist
    connect(&Playback::instance(), &Playback::resolvingChanged, this,
        [this](bool resolving) {
            if (resolving)
                m_artist->setText(QString::fromUtf8("正在获取播放链接…"));
            else if (!m_lastArtist.isEmpty())
                m_artist->setText(m_lastArtist);
        });
    m_artist->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Preferred);

    auto* drag = new QQuickDragHandler(textCol);
    connect(drag, &QQuickDragHandler::activeChanged, this, [this, drag]() {
        if (drag->active()) return;
        const qreal dx = drag->translation().x();
        if (dx > kSwipeThreshold)
            Q_EMIT prevRequested();
        else if (dx < -kSwipeThreshold)
            Q_EMIT nextRequested();
    });

    // ③ 圆形播放/暂停（唯一，44×44）
    m_playBtn = new QskPushButton(QString::fromUtf8("▶"), this);
    m_playBtn->setPreferredSize(44, 44);
    m_playBtn->setBoxShapeHint(QskPushButton::Panel,
        QskBoxShapeMetrics(22, Qt::AbsoluteSize));   // 半径=半宽 → 正圆
    connect(m_playBtn, &QskAbstractButton::clicked,
            this, &PlayerBar::playPauseRequested);

    // ④ 时间列：当前 / 总时长（两行，右对齐）
    auto* timeCol = new QskLinearBox(Qt::Vertical, this);
    timeCol->setSpacing(0);
    timeCol->setPreferredWidth(48);
    timeCol->setSizePolicy(QskSizePolicy::Minimum, QskSizePolicy::Preferred);
    m_timeCur = new QskTextLabel(QStringLiteral("0:00"), timeCol);
    m_timeCur->setAlignment(Qt::AlignVCenter | Qt::AlignRight);
    m_timeDur = new QskTextLabel(QStringLiteral("0:00"), timeCol);
    m_timeDur->setFontRole(QskFontRole(QskFontRole::Caption, QskFontRole::Normal));
    m_timeDur->setAlignment(Qt::AlignVCenter | Qt::AlignRight);

    // ⑤ 词按钮（歌词弹层入口，原入口恢复）+ 队列键
    m_lyricBtn = new QskPushButton(QString::fromUtf8("词"), this);
    m_lyricBtn->setPreferredSize(36, 36);   // 提高触摸命中率
    connect(m_lyricBtn, &QskAbstractButton::clicked, this, [this]() {
        qDebug() << "[DIAG] lyricBtn clicked";
        Q_EMIT lyricRequested();
    });

    m_queueBtn = new QskPushButton(QString::fromUtf8("☰"), this);
    m_queueBtn->setPreferredSize(36, 36);
    connect(m_queueBtn, &QskAbstractButton::clicked,
            this, &PlayerBar::queueRequested);
    {
        // 队列键长按=歌词（额外通道，既有行为保留）
        auto* tap = new QQuickTapHandler(m_queueBtn);
        connect(tap, &QQuickTapHandler::longPressed, this, [this]() {
            Q_EMIT lyricRequested();
        });
    }

    // ⑥ 音量：仅桌面（Android 隐藏——设计明确要求）
#ifndef Q_OS_ANDROID
    m_volumeBtn = new QskPushButton(QString::fromUtf8("🔊"), this);
    m_volumeBtn->setPreferredSize(30, 30);
    connect(m_volumeBtn, &QskAbstractButton::clicked,
            this, &PlayerBar::volumeClicked);
#endif

    // ⑦ 下载进度改为播放条下方的独立进度线（main.cpp 装配，
    //    本类只做数据转发；播放条内不再保留下载控件）

    update();
}

QString PlayerBar::fmtMs(int ms)
{
    const int total = ms / 1000;
    const int m = total / 60;
    const int s = total % 60;
    QString ss = QString::number(s);
    if (s < 10) ss = QString("0") + ss;
    return QString::number(m) + ":" + ss;
}

void PlayerBar::setArtistAvatar(const QString& url, const QString& seed)
{
    if (url.isEmpty() && seed.isEmpty()) {
        m_artistAvatar->setVisible(false);
        return;
    }
    m_artistAvatar->setFallback(qximage::letterAvatar(seed, IconItem::pxForDp(20), true));
    m_artistAvatar->setSource(url, 20);
    m_artistAvatar->setVisible(true);
}

void PlayerBar::setCover(const QString& url, const QString& seed)
{
    m_icon->setFallback(qximage::identicon(seed, IconItem::pxForDp(40)));
    m_icon->setSource(url, 40);
}

void PlayerBar::setSongInfo(const QString& title, const QString& artist)
{
    m_title->setText(elided(title, 10));
    const QString a = elided(artist, 14);
    m_lastArtist = a;   // resolving 提示结束后由此恢复
    m_artist->setText(a.isEmpty()
        ? QString::fromUtf8("左右横划切换上下一首")   // 无歌/无歌手提示
        : a);
}

void PlayerBar::setProgressMs(int ms)
{
    m_progressMs = ms;
    m_timeCur->setText(fmtMs(ms));
    if (m_line)
        m_line->setRatio(m_durationMs > 0
            ? qreal(ms) / m_durationMs : 0.0);
}

void PlayerBar::setDurationMs(int ms)
{
    m_durationMs = ms;
    m_timeDur->setText(fmtMs(ms));
    setProgressMs(m_progressMs);
}

void PlayerBar::setPlaying(bool playing)
{
    m_playing = playing;
    m_playBtn->setText(playing ? QString::fromUtf8("⏸") : QString::fromUtf8("▶"));
}

void PlayerBar::setDownloadProgress(int pct)
{
    if (!m_dlLine) return;
    if (pct >= 0 && pct < 100) {
        m_dlLine->setRatio(pct / 100.0);     // 下载中：绿线按比例填充
    } else {
        // 100 完成 / -2 失败 / 其它：无进度（线回底槽色，常显）
        m_dlLine->setRatio(0.0);
    }
}
