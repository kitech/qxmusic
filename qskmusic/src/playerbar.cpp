#include "playerbar.h"
#include "progressline.h"
#include <QskPushButton.h>
#include <QskProgressBar.h>
#include <QskTextLabel.h>
#include <QskFontRole.h>
#include <QskBoxShapeMetrics.h>
#include <QskSizePolicy.h>
#include <private/qquickdraghandler_p.h>
#include <private/qquicktaphandler_p.h>
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

    // ① icon（40×40，垂直居中于两行高）
    m_icon = new QskTextLabel(QString::fromUtf8("♪"), this);
    m_icon->setPreferredSize(40, 40);
    m_icon->setPanel(true);
    m_icon->setBoxShapeHint(QskTextLabel::Panel,
        QskBoxShapeMetrics(8, Qt::AbsoluteSize));
    m_icon->setAlignment(Qt::AlignCenter);

    // ② 文本列：歌名 / 歌手（两行；左右横划切上下一首）
    auto* textCol = new QskLinearBox(Qt::Vertical, this);
    textCol->setSpacing(0);
    textCol->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Preferred);

    m_title = new QskTextLabel(textCol);
    m_title->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Preferred);
    m_artist = new QskTextLabel(QString::fromUtf8("左右横划切换上下一首"), textCol);
    m_artist->setFontRole(QskFontRole(QskFontRole::Caption, QskFontRole::Normal));
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

    // ⑤ 队列键（点击=队列；长按=歌词——原「词」按钮功能保留）
    m_queueBtn = new QskPushButton(QString::fromUtf8("☰"), this);
    m_queueBtn->setPreferredSize(36, 36);
    connect(m_queueBtn, &QskAbstractButton::clicked,
            this, &PlayerBar::queueRequested);
    {
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

    // ⑦ 下载进度（右侧瞬时显示：仅下载中出现，完成 1.2s 自动隐藏）
    m_dlBar = new QskProgressBar(0, 100, this);
    m_dlBar->setPreferredSize(40, 6);
    m_dlBar->setSizePolicy(QskSizePolicy::Minimum, QskSizePolicy::Preferred);
    m_dlPct = new QskTextLabel(this);
    m_dlPct->setPreferredWidth(32);
    m_dlPct->setAlignment(Qt::AlignVCenter | Qt::AlignRight);
    m_dlHideTimer = new QTimer(this);
    m_dlHideTimer->setSingleShot(true);
    connect(m_dlHideTimer, &QTimer::timeout, this, [this]() {
        m_dlBar->setVisible(false);
        m_dlPct->setVisible(false);
    });
    m_dlBar->setVisible(false);
    m_dlPct->setVisible(false);

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

void PlayerBar::setSongInfo(const QString& title, const QString& artist)
{
    m_title->setText(elided(title, 10));
    const QString a = elided(artist, 14);
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
    m_dlHideTimer->stop();

    if (pct == 100) {
        // 完成：短暂留痕后隐藏（qlmusic setDownloadHiddenLater 同款 1.2s）
        m_dlBar->setVisible(false);
        m_dlPct->setVisible(true);
        m_dlPct->setText(QString::fromUtf8("✓"));
        m_dlHideTimer->start(1200);
        return;
    }
    if (pct < 0) {
        // -1 未定态；-2 失败
        m_dlBar->setVisible(false);
        m_dlPct->setVisible(true);
        m_dlPct->setText(pct == -2 ? QString::fromUtf8("✕")
                                   : QString::fromUtf8("↓"));
        return;
    }
    m_dlBar->setVisible(true);
    m_dlBar->setValue(pct);
    m_dlPct->setVisible(true);
    m_dlPct->setText(QString::fromUtf8("%1%").arg(pct));
}
