#include "navbar.h"
#include <QskPushButton.h>
#include <QQuickWindow>

namespace {
const QStringList kTabs = {
    QString::fromUtf8("歌曲"),
    QString::fromUtf8("分类"),
    QString::fromUtf8("我喜欢的音乐"),
    QString::fromUtf8("最近播放"),
    QString::fromUtf8("下载音乐"),
};
// Material compact 断点：<600dp 为紧凑屏，用底栏（Qt 逻辑像素 = dp）
constexpr int kRailMinWidth = 600;
}

NavBar::NavBar(QQuickItem* parent)
    : QskLinearBox(Qt::Horizontal, parent)
{
    setPanel(true);

    for (int i = 0; i < kTabs.size(); ++i) {
        auto* btn = new QskPushButton(kTabs.at(i), this);
        btn->setCheckable(true);
        connect(btn, &QskAbstractButton::clicked, this, [this, i]() {
            setCurrentIndex(i);
        });
        m_buttons[i] = btn;
    }
    syncChecked();

    // 宽度自适应：挂上 window 后监听宽度（含旋转/分屏），照 qskcomp 范式
    connect(this, &QQuickItem::windowChanged, this, [this](QQuickWindow* w) {
        if (w)
            connect(w, &QWindow::widthChanged, this, &NavBar::updateMode);
        updateMode();
    });
}

void NavBar::updateMode()
{
    const bool rail = window() && window()->width() >= kRailMinWidth;
    if (rail == m_rail) return;
    m_rail = rail;

    if (rail) {
        setOrientation(Qt::Vertical);
        setSizePolicy(QskSizePolicy::Fixed, QskSizePolicy::Expanding);
        setPreferredWidth(160);
        for (auto* b : m_buttons) {
            b->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Fixed);
            b->setPreferredHeight(48);
        }
    } else {
        setOrientation(Qt::Horizontal);
        setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Fixed);
        setPreferredHeight(56);
        for (auto* b : m_buttons) {
            b->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Minimum);
            b->setPreferredHeight(56);
        }
    }
    Q_EMIT modeChanged(rail);
}

void NavBar::setCurrentIndex(int index)
{
    if (index < 0 || index >= kTabs.size() || index == m_current) return;
    m_current = index;
    syncChecked();
    Q_EMIT currentIndexChanged(index);
}

void NavBar::syncChecked()
{
    for (int i = 0; i < kTabs.size(); ++i)
        m_buttons[i]->setChecked(i == m_current);
}
