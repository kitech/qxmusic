#ifndef STATUS_LINE_H
#define STATUS_LINE_H

#include <QskLinearBox.h>

class QskTextLabel;
class QskPushButton;

/*
 * 页面状态行（图标表示类型 + 语义配色 + 空文本自动整行隐藏）：
 *   Loading ⏳ 灰 / Info ℹ️ 灰 / Warn ⚠️ 黄 / Error ⛔ 红
 * Error 态右侧挂「重试」按钮（retryClicked 信号）。
 */
class StatusLine : public QskLinearBox
{
    Q_OBJECT
public:
    enum Level { Hidden, Loading, Info, Warn, Error };

    explicit StatusLine(QQuickItem* parent = nullptr);

    void setStatus(Level level, const QString& text);
    void clearStatus() { setStatus(Hidden, QString()); }
    bool isError() const { return m_level == Error; }

Q_SIGNALS:
    void retryClicked();

private:
    QskTextLabel* m_icon = nullptr;
    QskTextLabel* m_text = nullptr;
    QskPushButton* m_retry = nullptr;
    Level m_level = Hidden;
};

#endif
