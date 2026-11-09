#include "navbar.h"
#include <QskPushButton.h>
#include <QskSizePolicy.h>

namespace {
const QStringList kTabs = {
    QString::fromUtf8("推荐"),
    QString::fromUtf8("分类"),
    QString::fromUtf8("我的"),
};
}

NavBar::NavBar(QQuickItem* parent)
    : QskLinearBox(Qt::Horizontal, parent)
{
    setPanel(true);
    setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Fixed);
    setPreferredHeight(56);

    for (int i = 0; i < kTabs.size(); ++i) {
        auto* btn = new QskPushButton(kTabs.at(i), this);
        btn->setCheckable(true);
        btn->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Minimum);
        connect(btn, &QskAbstractButton::clicked, this, [this, i]() {
            setCurrentIndex(i);
        });
        m_buttons[i] = btn;
    }
    syncChecked();
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
