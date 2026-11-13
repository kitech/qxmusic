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

/* 选项值只用索引 —— 绝不把 code/key 放进列表（Qt i18n 规范）；
 * 文本在为 QskComboBox 插入时用 tr(字面量) 求值（见 buildUi） */

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
         * 选项值即索引，文本在插入时 tr() 求值（字面量便于 lupdate 抽取） */
        const QStringList skins{ QStringLiteral("Fusion"),
                                 QStringLiteral("Fluent2"),
                                 QStringLiteral("Material3") };
        for (const auto& s : skins)
            m_skinCombo->addOption(QskLabelData(s));
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
        /* 与 anystik/QskStackBoxAnimator1..4 一一对应
         * （3=Perspective=默认 Animator4；名称是动画风格，不是开关） */
        const QStringList transitions{ tr("Slide"), tr("2D"),
                                       tr("3D"), tr("Perspective") };
        for (const auto& s : transitions)
            m_transitionCombo->addOption(QskLabelData(s));
        connect(m_transitionCombo, &QskComboBox::currentIndexChanged, this, [this](int idx) {
            QSettings().setValue("transition", idx);
            applyTransition(idx);
        });
    }

    // ── 语言 ──
    {
        auto* row = addRow(tr("Language"));
        m_langCombo = new QskComboBox(row);
        m_langCombo->setPreferredWidth(180);
        /* 0 = 跟随系统（QSettings 无 language 键）；其余为显式选择 */
        const QStringList langs{ tr("System default"), tr("English"),
                                 QString::fromUtf8("简体中文"),
                                 QString::fromUtf8("繁體中文") };
        for (const auto& s : langs)
            m_langCombo->addOption(QskLabelData(s));
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

    /* 防御：任何形如 "xxx.yyy" 的未翻译键都不允许上屏（i18n 规范）；
     * 只告警，便于真机 logcat 一眼发现配置遗漏 */
    for (auto* combo : { m_skinCombo, m_transitionCombo, m_langCombo }) {
        if (!combo) continue;
        for (int i = 0; i < combo->options().size(); ++i) {
            const QString t = combo->optionAt(i).text();
            if (t.contains(QLatin1Char('.')) && !t.contains(QLatin1Char(' ')))
                qWarning("[SETTINGS] untranslated key in combo: %s",
                         qUtf8Printable(t));
        }
    }
}

void SettingsPage::applyTransition(int idx)
{
    auto* pm = pageManager();
    QskStackBox* box = pm ? pm->stackBox() : nullptr;
    if (!box)
        return;
    QskStackBoxAnimator* a = nullptr;
    switch (qBound(0, idx, 3)) {
    case 0: a = new QskStackBoxAnimator1(box); break;
    case 1: a = new QskStackBoxAnimator2(box); break;
    case 2: a = new QskStackBoxAnimator3(box); break;
    default: a = new QskStackBoxAnimator4(box); break;   // Perspective
    }
    box->setAnimator(a);   // QskStackBox 接管旧 animator 生命周期
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
