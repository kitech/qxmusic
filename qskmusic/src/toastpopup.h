#ifndef TOAST_POPUP_H
#define TOAST_POPUP_H

#include <QskPopup.h>
#include <QString>

class QskBox;
class QskLinearBox;
class QskTextLabel;

/*
 * Android 风格 toast：非模态、居中、延时自动消失。
 * qskmusic 为桌面工程（自 anystik 裁 Android 原生 toast 分支而来），
 * 走 QQuickWindow 内 QskPopup 桌面浮层（非阻塞、closed→deleteLater）。
 */
class ToastPopup : public QskPopup
{
    Q_OBJECT
public:
    static void show(QQuickItem* parent, const QString& text,
                     int durationMs = 2600);

protected:
    void updateLayout() override;

private:
    ToastPopup(const QString& text, QQuickItem* parent);
    void updateGeometry();

    QskBox* m_panel = nullptr;
    QskLinearBox* m_layout = nullptr;
    QskTextLabel* m_label = nullptr;
};

#endif // TOAST_POPUP_H
