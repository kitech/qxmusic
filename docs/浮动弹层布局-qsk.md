# 浮动弹层（QskPopup）布局坑与正确姿势 — qskmusic

状态：生效　更新：2026-11-11

## 症状

QskPopup 打开后，面板/内容没有出现在预期位置，而是**退化为屏幕左上角（或顶部一条）**：
标题、内容挤在顶部，看起来像“页面顶部多了一行字”。

## 根因

1. QskPopup **不会自动获得窗口尺寸**，其自身 `rect()` 默认为空/0；
   以 `popup->rect()` 作为布局基准 → `isEmpty()` 直接返回 → 几何从未赋值
   → panel/layout/文本停留在 (0,0)，即“退化到顶部”。
2. 只设置 panel 不设置 popup 自身几何、或 open 后不补 relayout，也会复现。

## 正确姿势（照 qskcomp/dialogpopup）

```cpp
// 1) 坐标基准 = 父项所在窗口尺寸（不是 popup->rect()）
QRectF parentRectOf(QQuickItem* parent) {
    if (!parent) return {};
    if (auto* w = parent->window()) return QRectF(QPointF(), w->size());
    return QRectF(-parent->x(), -parent->y(), parent->width(), parent->height());
}

// 2) 计算居中矩形并 setGeometry 到 popup 自身
QRectF popupRect(0, 0, w, h);
popupRect.moveCenter(parentRect.center());
setGeometry(popupRect);

// 3) panel 填满 popup；layout 与 panel 同尺寸
m_panel->setGeometry(QRectF(0, 0, w, h));
m_layout->setGeometry(QRectF(0, 0, w, h));

// 4) setPolishOnResize(true) + setPolishOnParentResize(true)
// 5) 打开前 relayout() 一次；open() 后 QTimer::singleShot(0) 再 relayout() 一次
//    （open 首帧窗口尺寸才最终可用）
```

## 新增弹层必查

- [ ] popup 自身 setGeometry（居中矩形）
- [ ] panel 填满 popup；layout 跟随 panel
- [ ] 坐标基准用 window()->size()，禁止用 popup->rect()
- [ ] open 前/后各 relayout 一次
- [ ] 面板不透明（皮肤 Panel + alpha 0.9x），否则全透明
- [ ] 图标/角标用绝对 setGeometry 定位，不依赖线性布局流
