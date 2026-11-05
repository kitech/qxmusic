#ifndef PROGRESS_POPUP_H
#define PROGRESS_POPUP_H

#include <QskPopup.h>
#include <QString>
#include <QColor>

class QskProgressBar;
class QskTextLabel;
class QskLinearBox;
class QskPushButton;
class QskBox;
class LogListView;
class LogModel;

/*
 * 通用任务进度浮动窗口（自 anystik syncprogresspopup 重构：
 * 裁 SyncEngine/davbisync 依赖，宿主经公开槽注入进度与日志）：
 *   - 实时结构化日志：时间戳 / 级别(配色) / [tag] / 原文，自动滚底
 *   - 进度条（setProgress）与完成/失败摘要（finishRun）
 *   - 过滤：级别下拉 + 关键词（LogListView 内建）
 *   - 运行中禁用「关闭」；「取消」仅发 cancelRequested 由宿主处理
 * 数据存于私有 LogModel 实例（不依赖全局日志，避免相互污染）。
 */
class ProgressPopup : public QskPopup
{
    Q_OBJECT
public:
    explicit ProgressPopup(const QString& title, QQuickItem* parent = nullptr);
    ~ProgressPopup() override;

    // ── 宿主注入 ──
    void appendLog(int level, const QString& tag, const QString& msg);
    void setProgress(int cur, int total);       // total<=0 → 忙碌态（进度条不确定）
    void setDetail(const QString& text);        // 统计行
    void setStatus(const QString& text, const QColor& color = QColor());
    void finishRun(bool ok, const QString& summary);
    void clearLog();
    void resetForRun();

Q_SIGNALS:
    void cancelRequested();

protected:
    void updateLayout() override;
    bool eventFilter(QObject* obj, QEvent* ev) override;

private:
    void updateGeometry();

    LogModel* m_model = nullptr;
    QskBox* m_panel = nullptr;
    QskLinearBox* m_layout = nullptr;
    LogListView* m_logList = nullptr;

    QskProgressBar* m_progressBar = nullptr;
    QskTextLabel* m_pctLabel = nullptr;
    QskTextLabel* m_detailLabel = nullptr;
    QskTextLabel* m_statusLabel = nullptr;
    QskPushButton* m_cancelBtn = nullptr;
    QskPushButton* m_closeBtn = nullptr;
    QskPushButton* m_cornerCloseBtn = nullptr;
    bool m_finished = false;
    bool m_escFilterInstalled = false;
};

#endif // PROGRESS_POPUP_H
