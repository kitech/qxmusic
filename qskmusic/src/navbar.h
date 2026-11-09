#ifndef NAV_BAR_H
#define NAV_BAR_H

#include <QskLinearBox.h>

class QskPushButton;

/*
 * 自适应导航条（Material 3：底部导航栏 / nav rail 双形态）：
 *   窗宽 <600dp  → 横排底栏（56dp 高）
 *   窗宽 ≥600dp  → 竖排 rail（160dp 宽）
 * 5 个顶级目的地 tab；tab 切换不入 PageManager 栈
 * （Material：返回键不在 tab 间导航）。
 */
class NavBar : public QskLinearBox
{
    Q_OBJECT
public:
    explicit NavBar(QQuickItem* parent = nullptr);

Q_SIGNALS:
    void currentIndexChanged(int index);
    void modeChanged(bool rail);

public Q_SLOTS:
    void setCurrentIndex(int index);

private:
    void updateMode();
    void syncChecked();

    QskPushButton* m_buttons[5] = {};
    int m_current = 0;
    bool m_rail = false;
};

#endif
