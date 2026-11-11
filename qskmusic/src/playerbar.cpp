#include "playerbar.h"
#include "progressline.h"
#include <QskPushButton.h>
#include <QskProgressBar.h>
#include <QskTextLabel.h>
#include <QskFontRole.h>
#include <QskSizePolicy.h>
#include <QTimer>

namespace {

// anystik stickerlist 范式：代码级截断，文本永不画出 item 边界
QString elided(const QString& t, int maxChars)
{
    return t.size() > maxChars ? t.left(maxChars - 1) + QString::fromUtf8("…") : t;
}

} // namespace

PlayerBar::PlayerBar(QQuickItem* parent)
    : QskLinearBox(Qt::Horizontal, parent)
{
    setPanel(true);
    setFixedHeight(52);
    setMargins(6);
    setSpacing(6);

    m_prevBtn = new QskPushButton(QString::fromUtf8("⏮"), this);
    m_prevBtn->setPreferredSize(30, 30);
    connect(m_prevBtn, &QskAbstractButton::clicked, this, &PlayerBar::prevRequested);

    m_playBtn = new QskPushButton(QString::fromUtf8("▶"), this);
    m_playBtn->setPreferredSize(36, 36);
    connect(m_playBtn, &QskAbstractButton::clicked, this, &PlayerBar::playPauseRequested);

    m_nextBtn = new QskPushButton(QString::fromUtf8("⏭"), this);
    m_nextBtn->setPreferredSize(30, 30);
    connect(m_nextBtn, &QskAbstractButton::clicked, this, &PlayerBar::nextRequested);

    // 当前歌名/歌手（qlmusic 播放条同款；窄屏放不下时优先让进度条收缩）
    auto* infoBox = new QskLinearBox(Qt::Vertical, this);
    infoBox->setSpacing(0);
    infoBox->setPreferredWidth(140);
    infoBox->setSizePolicy(QskSizePolicy::Preferred, QskSizePolicy::Preferred);
    m_title = new QskTextLabel(infoBox);
    m_title->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Preferred);
    m_artist = new QskTextLabel(infoBox);
    m_artist->setFontRole(QskFontRole(QskFontRole::Caption, QskFontRole::Normal));
    m_artist->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Preferred);

    // 进度条已改为播放条上沿的全宽细线（ProgressLineItem，main.cpp 装配），
    // 此处不再创建内嵌 QskProgressBar——释放横向空间


    m_timeLabel = new QskTextLabel(QStringLiteral("0:00 / 0:00"), this);
    m_timeLabel->setPreferredWidth(72);
    m_timeLabel->setSizePolicy(QskSizePolicy::Minimum, QskSizePolicy::Preferred);
    m_timeLabel->setAlignment(Qt::AlignVCenter | Qt::AlignRight);

    // 下载进度（qlmusic PlayerBar::setDownloadState 同位置：右侧图标区）
    m_dlBar = new QskProgressBar(0, 100, this);
    m_dlBar->setPreferredSize(46, 6);
    m_dlBar->setSizePolicy(QskSizePolicy::Minimum, QskSizePolicy::Preferred);
    m_dlPct = new QskTextLabel(this);
    m_dlPct->setPreferredWidth(34);
    m_dlPct->setAlignment(Qt::AlignVCenter | Qt::AlignRight);
    m_dlHideTimer = new QTimer(this);
    m_dlHideTimer->setSingleShot(true);
    connect(m_dlHideTimer, &QTimer::timeout, this, [this]() {
        m_dlBar->setVisible(false);
        m_dlPct->setVisible(false);
    });
    m_dlBar->setVisible(false);
    m_dlPct->setVisible(false);

    m_lyricBtn = new QskPushButton(QString::fromUtf8("词"), this);
    m_lyricBtn->setPreferredSize(30, 30);
    connect(m_lyricBtn, &QskAbstractButton::clicked, this, &PlayerBar::lyricRequested);

    m_volumeBtn = new QskPushButton(QString::fromUtf8("🔊"), this);
    m_volumeBtn->setPreferredSize(30, 30);
    connect(m_volumeBtn, &QskAbstractButton::clicked, this, &PlayerBar::volumeClicked);

    m_queueBtn = new QskPushButton(QString::fromUtf8("☰"), this);
    m_queueBtn->setPreferredSize(30, 30);
    connect(m_queueBtn, &QskAbstractButton::clicked, this, &PlayerBar::queueRequested);

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
    m_artist->setText(elided(artist, 14));
}

void PlayerBar::setProgressMs(int ms)
{
    m_progressMs = ms;
    if (m_line)
        m_line->setRatio(m_durationMs > 0
            ? qreal(ms) / m_durationMs : 0.0);
    m_timeLabel->setText(fmtMs(m_progressMs) + " / " + fmtMs(m_durationMs));
}

void PlayerBar::setDurationMs(int ms)
{
    m_durationMs = ms;
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
        // -1 未定态（转圈以「↓」占位）；-2 失败
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
