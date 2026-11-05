#include "progresspopup.h"
#include "logmodel.h"
#include "loglistview.h"
#include <QskLinearBox.h>
#include <QskTextLabel.h>
#include <QskProgressBar.h>
#include <QskPushButton.h>
#include <QskFontRole.h>
#include <QskBox.h>
#include <QskGradient.h>
#include <QskBoxShapeMetrics.h>
#include <QClipboard>
#include <QKeyEvent>
#include <QGuiApplication>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTimer>

namespace {

QColor levelColor(const LogModel::Entry& e)
{
    switch (e.level) {
        case LogModel::Error: return QColor(255, 91, 91);
        case LogModel::Warn:  return QColor(255, 180, 84);
        case LogModel::Info:  return QColor(150, 205, 255);
        default:              return QColor();
    }
}

QString levelTag(const LogModel::Entry& e)
{
    switch (e.level) {
        case LogModel::Error: return QStringLiteral("ERR");
        case LogModel::Warn:  return QStringLiteral("WRN");
        case LogModel::Info:  return QStringLiteral("INF");
        default:              return QStringLiteral("DBG");
    }
}

LogListView::RowItem buildRow(const LogModel::Entry& e)
{
    LogListView::RowItem item;
    item.level = int(e.level);
    item.tag = e.tag;
    item.message = e.message;
    item.text = e.timestamp + QStringLiteral("  [") + e.tag + QStringLiteral("] ")
                + levelTag(e) + QStringLiteral("  ") + e.message;
    item.color = levelColor(e);
    return item;
}

QRectF popupParentRect(QQuickItem* parent)
{
    if (!parent) {
        return {};
    }
    if (auto* w = parent->window()) {
        return QRectF(QPointF(), w->size());
    }
    return QRectF(-parent->x(), -parent->y(),
                  parent->width(), parent->height());
}

// 面板不透明度：回读当前皮肤面板填充色，仅改 alpha（保留主题配色，对齐 DialogPopup 语义）
void applyPanelOpacity(QskBox* panel, qreal opacity)
{
    if (!panel)
        return;

    QskGradient g = panel->fillGradient();
    if (!g.isValid())
        return;   // 皮肤未提供填充色时保持不透明，不擅自替色

    g.setAlpha(qRound(qBound(0.0, opacity, 1.0) * 255.0));
    panel->setFillGradient(g);
}

} // namespace

ProgressPopup::ProgressPopup(const QString& title, QQuickItem* parent)
    : QskPopup(parent)
    , m_model(new LogModel(this))
{
    setModal(true);
    setOverlay(true);
    setPopupFlag(QskPopup::DeleteOnClose, false);
    setPolishOnResize(true);
    setPolishOnParentResize(true);

    m_panel = new QskBox(this);
    m_panel->setBoxShapeHint(QskBox::Panel,
        QskBoxShapeMetrics(14, Qt::AbsoluteSize));
    applyPanelOpacity(m_panel, 0.9);

    m_layout = new QskLinearBox(Qt::Vertical, m_panel);
    m_layout->setSpacing(8);
    m_layout->setMargins(18);

    // ── 标题行（左：标题 / 右：关闭按钮）──
    auto* titleRow = new QskLinearBox(Qt::Horizontal, m_layout);
    titleRow->setSpacing(8);

    auto* titleLabel = new QskTextLabel(title, titleRow);
    titleLabel->setFontRole(QskFontRole::Title);
    titleLabel->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    titleLabel->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Preferred);

    m_cornerCloseBtn = new QskPushButton(QStringLiteral("✕"), titleRow);
    connect(m_cornerCloseBtn, &QskAbstractButton::clicked,
            this, &QskPopup::close);

    // ── 进度条 ──
    auto* progressRow = new QskLinearBox(Qt::Horizontal, m_layout);
    progressRow->setSpacing(8);
    m_progressBar = new QskProgressBar(0, 100, progressRow);
    m_progressBar->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Preferred);
    m_progressBar->setValue(0);
    m_pctLabel = new QskTextLabel(QStringLiteral("0%"), progressRow);
    m_pctLabel->setPreferredWidth(40);
    m_pctLabel->setAlignment(Qt::AlignVCenter | Qt::AlignRight);

    // ── 统计行 ──
    m_detailLabel = new QskTextLabel(QStringLiteral(" "), m_layout);
    m_detailLabel->setFontRole(QskFontRole::Caption);
    m_detailLabel->setWrapMode(QskTextOptions::WrapAnywhere);
    m_detailLabel->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Constrained);

    // ── 状态行 ──
    m_statusLabel = new QskTextLabel(QStringLiteral("运行中..."), m_layout);
    m_statusLabel->setFontRole(QskFontRole::Caption);
    m_statusLabel->setTextColor(QColor(150, 205, 255));
    m_statusLabel->setWrapMode(QskTextOptions::WrapAnywhere);
    m_statusLabel->setSizePolicy(QskSizePolicy::Expanding, QskSizePolicy::Constrained);

    // ── 日志组件：过滤条 + 滚动列表 + 工具行(计数/复制/清空) ──
    m_logList = new LogListView(m_layout);
    m_logList->setLevelComboOptions(
        { QStringLiteral("全部"), QStringLiteral("Info"),
          QStringLiteral("Warn"), QStringLiteral("Error") });
    m_logList->setCountLabelFormat(QStringLiteral("%1 / %2 条"));
    m_logList->setListPreferredHeight(340);

    connect(m_logList, &LogListView::copyClicked, this, [this](const QString& text) {
        QGuiApplication::clipboard()->setText(text);
        m_statusLabel->setText(QStringLiteral("已复制 ")
            + QString::number(text.isEmpty() ? 0 : text.count(QLatin1Char('\n')) + 1)
            + QStringLiteral(" 条到剪贴板"));
    });
    connect(m_logList, &LogListView::clearClicked, this, [this]() {
        clearLog();
    });

    // ── 宿主按钮行：取消/关闭（不属于日志组件）──
    auto* buttonRow = new QskLinearBox(Qt::Horizontal, m_layout);
    buttonRow->setSpacing(6);
    buttonRow->addSpacer(0, 0);

    m_cancelBtn = new QskPushButton(QStringLiteral("取消"), buttonRow);
    connect(m_cancelBtn, &QskAbstractButton::clicked, this, [this]() {
        Q_EMIT cancelRequested();
    });

    m_closeBtn = new QskPushButton(QStringLiteral("关闭"), buttonRow);
    m_closeBtn->setEnabled(false);
    connect(m_closeBtn, &QskAbstractButton::clicked, this, &QskPopup::close);

    m_cornerCloseBtn->setEnabled(false);

    connect(this, &QskPopup::opened, this, [this]() {
        if (m_escFilterInstalled)
            return;
        if (auto* w = window()) {
            w->installEventFilter(this);
            m_escFilterInstalled = true;
        }
    });
    QTimer::singleShot(0, this, [this]() { updateGeometry(); });
}

ProgressPopup::~ProgressPopup()
{
    if (m_escFilterInstalled) {
        if (auto* w = window())
            w->removeEventFilter(this);
        m_escFilterInstalled = false;
    }
}

void ProgressPopup::appendLog(int level, const QString& tag, const QString& msg)
{
    m_model->append(static_cast<LogModel::Level>(level), tag, msg);
    m_logList->appendItem(buildRow(m_model->at(m_model->count() - 1)));
}

void ProgressPopup::setProgress(int cur, int total)
{
    if (total <= 0) {
        m_pctLabel->setText(QStringLiteral("--"));
        return;
    }
    const int pct = qBound(0, int(qreal(cur) / total * 100.0), 100);
    m_progressBar->setValue(pct);
    m_pctLabel->setText(QStringLiteral("%1%").arg(pct));
}

void ProgressPopup::setDetail(const QString& text)
{
    m_detailLabel->setText(text);
}

void ProgressPopup::setStatus(const QString& text, const QColor& color)
{
    if (color.isValid())
        m_statusLabel->setTextColor(color);
    m_statusLabel->setText(text);
}

void ProgressPopup::finishRun(bool ok, const QString& summary)
{
    m_finished = true;
    m_cancelBtn->setEnabled(false);
    m_closeBtn->setEnabled(true);
    m_cornerCloseBtn->setEnabled(true);

    if (ok) {
        m_statusLabel->setTextColor(QColor(120, 210, 140));
        m_statusLabel->setText(QStringLiteral("完成 · ") + summary);
    } else {
        m_statusLabel->setTextColor(QColor(255, 91, 91));
        m_statusLabel->setText(QStringLiteral("失败 · ") + summary);
    }
}

void ProgressPopup::clearLog()
{
    if (m_model) {
        m_model->clear();
    }
    m_logList->clearItems();
}

void ProgressPopup::resetForRun()
{
    m_finished = false;
    m_progressBar->setValue(0);
    m_pctLabel->setText(QStringLiteral("0%"));
    clearLog();
    m_statusLabel->setTextColor(QColor(150, 205, 255));
    m_statusLabel->setText(QStringLiteral("运行中..."));
    m_cancelBtn->setEnabled(true);
    m_closeBtn->setEnabled(false);
    m_cornerCloseBtn->setEnabled(false);
}

void ProgressPopup::updateLayout()
{
    updateGeometry();
    m_layout->setGeometry(layoutRect());
}

bool ProgressPopup::eventFilter(QObject* obj, QEvent* ev)
{
    // ESC 关闭（仅完成态放行；运行中忽略，防误触中断任务）
    if (m_finished && obj == window() && ev->type() == QEvent::KeyPress) {
        auto* ke = static_cast<QKeyEvent*>(ev);
        if (ke->key() == Qt::Key_Escape) {
            close();
            return true;
        }
    }
    return QskPopup::eventFilter(obj, ev);
}

void ProgressPopup::updateGeometry()
{
    const auto parentRect = popupParentRect(parentItem());
    if (parentRect.isEmpty()) {
        return;
    }

    const auto hint = m_layout->effectiveSizeHint(Qt::PreferredSize, QSizeF());
    const qreal maxW = qMin(0.92 * parentRect.width(), 440.0);
    const qreal maxH = 0.9 * parentRect.height();

    const qreal panelW = qBound(320.0, hint.width() + 36, maxW);
    const qreal panelH = qBound(360.0, hint.height() + 36, maxH);

    QRectF r(0, 0, panelW, panelH);
    r.moveCenter(parentRect.center());
    setGeometry(r);
    m_panel->setGeometry(r.translated(-r.topLeft()));
}

#include "moc_progresspopup.cpp"
