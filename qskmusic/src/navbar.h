#ifndef NAV_BAR_H
#define NAV_BAR_H

#include <QskLinearBox.h>

class QskPushButton;

/*
 * 底部导航条（anystik 思路：单形态，不做宽窄屏切换）：
 *   3 个顶级目的地 tab，横排底栏 56dp 高；tab 切换不入 PageManager 栈
 *   （Material：返回键不在 tab 间导航）。
 */
class NavBar : public QskLinearBox
{
    Q_OBJECT
public:
    explicit NavBar(QQuickItem* parent = nullptr);

Q_SIGNALS:
    void currentIndexChanged(int index);

public Q_SLOTS:
    void setCurrentIndex(int index);

private:
    void syncChecked();

    QskPushButton* m_buttons[3] = {};
    int m_current = 0;
};

#endif
