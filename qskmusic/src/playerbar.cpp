#include "playerbar.h"
#include <QskPushButton.h>
#include <QskProgressBar.h>
#include <QskTextLabel.h>
#include <QskSizePolicy.h>

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

    m_progress = new QskProgressBar(0, 1000, this);
    m_progress->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Preferred);
    m_progress->setValue(0);

    m_timeLabel = new QskTextLabel(QStringLiteral("0:00 / 0:00"), this);
    m_timeLabel->setPreferredWidth(72);
    m_timeLabel->setSizePolicy(QskSizePolicy::Minimum, QskSizePolicy::Preferred);
    m_timeLabel->setAlignment(Qt::AlignVCenter | Qt::AlignRight);

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

void PlayerBar::setProgressMs(int ms)
{
    m_progressMs = ms;
    m_progress->setValue(m_durationMs > 0
        ? int(qreal(ms) / m_durationMs * 1000.0) : 0);
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
