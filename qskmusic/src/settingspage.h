#ifndef SETTINGS_PAGE_H
#define SETTINGS_PAGE_H

#include "page.h"

class QskComboBox;
class QskSwitchButton;

/*
 * 设置页（自 anystik settingspage 模板级重写：裁 Android/贴纸/字体缩放系统，
 * 保留皮肤/深色/转场/语言四项，QSettings 键与 main.cpp 恢复段对齐）。
 */
class SettingsPage : public Page
{
    Q_OBJECT
public:
    SettingsPage(QQuickItem* parent = nullptr);

protected:
    void onCreate(const QVariantMap& launchArgs,
                  const QVariantMap& savedState) override;
    void retranslateUi() override;

private:
    void buildUi();
    void applySettings();
    void applyTransition(int idx);   // 即时切换 StackBox 动画（anystik 同口径）

    QskComboBox* m_skinCombo = nullptr;
    QskComboBox* m_transitionCombo = nullptr;
    QskComboBox* m_langCombo = nullptr;
    QskSwitchButton* m_darkSwitch = nullptr;
    bool m_building = false;
};

#endif
