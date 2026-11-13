#include "settingspage.h"
#include "pagemanager.h"
#include "myi18n.h"
#include <QskLinearBox.h>
#include <QskTextLabel.h>
#include <QskPushButton.h>
#include <QskComboBox.h>
#include <QskLabelData.h>
#include <QskSwitchButton.h>
#include <QskSeparator.h>
#include <QskFontRole.h>
#include <QskSkinManager.h>
#include <QskSkin.h>
#include <QskStackBox.h>
#include <QskStackBoxAnimator.h>
#include <QSettings>
#include <QCoreApplication>

namespace {

const char* skinName(int idx)
{
    static const char* names[] = { "Fusion", "Fluent2", "Material3" };
    return names[qBound(0, idx, 2)];
}

} // namespace

SettingsPage::SettingsPage(QQuickItem* parent)
    : Page(parent)
{
}

void SettingsPage::retranslateUi()
{
    // 重建界面（组合框选项与标签随语言刷新）
    if (m_building) return;
    auto children = childItems();
    for (auto* c : children) c->deleteLater();
    buildUi();
}

void SettingsPage::onCreate(const QVariantMap&, const QVariantMap&)
{
    buildUi();
}

void SettingsPage::buildUi()
{
    m_building = true;

    setAutoLayoutChildren(true);
    auto* layout = new QskLinearBox(Qt::Vertical, this);
    layout->setPanel(true);

    // ── TopBar ──
    auto* topBar = new QskLinearBox(Qt::Horizontal, layout);
    topBar->setPanel(true);
    topBar->setPreferredHeight(56);

    auto* backBtn = new QskPushButton(QString::fromUtf8("←"), topBar);
    backBtn->setPreferredSize(44, 44);
    auto* title = new QskTextLabel(tr("Settings"), topBar);
    title->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Preferred);
    title->setAlignment(Qt::AlignCenter);

    connect(backBtn, &QskAbstractButton::clicked, this, [this]() {
        finish();
    });

    // 行构造 helper：标签 + 控件
    auto addRow = [layout](const QString& label) -> QskLinearBox* {
        layout->addSpacer(10, 0);
        auto* row = new QskLinearBox(Qt::Horizontal, layout);
        row->setPreferredHeight(44);
        auto* lbl = new QskTextLabel(label, row);
        lbl->setPreferredWidth(140);
        lbl->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
        return row;
    };

    // ── 外观：皮肤 ──
    {
        auto* row = addRow(tr("Skin"));
        m_skinCombo = new QskComboBox(row);
        m_skinCombo->setPreferredWidth(180);
        /* QskComboBox：addOption(iconSource, text) 不是 (text, value)；
         * 选项值即索引，用 QskLabelData 重载显式给文本 */
        m_skinCombo->addOption(QskLabelData(QStringLiteral("Fusion")));
        m_skinCombo->addOption(QskLabelData(QStringLiteral("Fluent2")));
        m_skinCombo->addOption(QskLabelData(QStringLiteral("Material3")));
        connect(m_skinCombo, &QskComboBox::currentIndexChanged, this, [this](int idx) {
            QSettings().setValue("skin", idx);
            auto* sm = QskSkinManager::instance();
            sm->setSkin(skinName(idx));
        });
    }

    // ── 外观：深色模式 ──
    {
        auto* row = addRow(tr("Dark mode"));
        m_darkSwitch = new QskSwitchButton(row);
        connect(m_darkSwitch, &QskSwitchButton::toggled, this, [this](bool on) {
            QSettings().setValue("darkMode", on);
            auto* skin = QskSkinManager::instance()->skin();
            if (skin)
                skin->setColorScheme(on ? QskSkin::DarkScheme : QskSkin::LightScheme);
        });
    }

    // ── 外观：页面转场 ──
    {
        auto* row = addRow(tr("Transition"));
        m_transitionCombo = new QskComboBox(row);
        m_transitionCombo->setPreferredWidth(180);
        m_transitionCombo->addOption(QskLabelData(tr("None")));
        m_transitionCombo->addOption(QskLabelData(QStringLiteral("Animator1")));
        m_transitionCombo->addOption(QskLabelData(QStringLiteral("Animator2")));
        m_transitionCombo->addOption(QskLabelData(tr("Animator4 (default)")));
        connect(m_transitionCombo, &QskComboBox::currentIndexChanged, this, [this](int idx) {
            QSettings().setValue("transition", idx);
        });
    }

    // ── 语言 ──
    {
        auto* row = addRow(tr("Language"));
        m_langCombo = new QskComboBox(row);
        m_langCombo->setPreferredWidth(180);
        /* 0 = 跟随系统（QSettings 无 language 键）；其余为显式选择 */
        m_langCombo->addOption(QskLabelData(tr("System default")));
        m_langCombo->addOption(QskLabelData(tr("English")));
        m_langCombo->addOption(QskLabelData(QString::fromUtf8("简体中文")));
        m_langCombo->addOption(QskLabelData(QString::fromUtf8("繁體中文")));
        connect(m_langCombo, &QskComboBox::currentIndexChanged, this, [this](int idx) {
            if (idx == 0)
                Lang::instance().useSystemLanguage();
            else
                Lang::instance().setLanguage(
                    idx == 1 ? "en" : (idx == 3 ? "zh-TW" : "zh-CN"));
        });
    }

    layout->addStretch(1);

    applySettings();
    m_building = false;
}

void SettingsPage::applySettings()
{
    QSettings s;

    const int skinIdx = s.value("skin", 0).toInt();
    if (m_skinCombo) m_skinCombo->setCurrentIndex(qBound(0, skinIdx, 2));

    const bool dark = s.value("darkMode", true).toBool();
    if (m_darkSwitch) m_darkSwitch->setChecked(dark);

    const int transIdx = s.value("transition", 3).toInt();
    if (m_transitionCombo) m_transitionCombo->setCurrentIndex(qBound(0, transIdx, 3));

    const QString lang = Lang::instance().code();
    const bool systemLang = QSettings().value("language", QString()).toString().isEmpty();
    if (m_langCombo)
        m_langCombo->setCurrentIndex(
            systemLang ? 0
                       : (lang == "en" ? 1 : (lang == "zh-TW" ? 3 : 2)));
}
