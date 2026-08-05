// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef EDGEAUTOHIDE_H
#define EDGEAUTOHIDE_H

#include <QObject>
#include <QWidget>
#include <QPoint>
#include <QRect>
#include <QTimer>
#include <QPointer>
#include <QtGlobal>

class QWidget;
class QMouseEvent;
class QPropertyAnimation;

// 贴边自动隐藏助手（P4-T1）：
// 紧凑窗口被拖拽到屏幕边缘后自动滑出屏幕、仅保留一条细边；
// 鼠标悬停到细边附近时自动滑回屏幕；鼠标离开后延时重新隐藏。
// 适用于快速录入窗口（QuickEntryDialog）和桌面便签卡片（StickyNoteCard）。
class EdgeAutoHide : public QObject
{
    Q_OBJECT
public:
    enum Edge { Left = 0, Right = 1, Top = 2, Bottom = 3 };

    explicit EdgeAutoHide(QWidget *w, QObject *parent = nullptr);

    bool isDocked() const { return m_docked; }
    bool isSlidIn() const { return m_slidIn; }
    Edge dockedEdge() const { return m_edge; }

    // 手动拖拽接口（供窗口自身 / 拖拽手柄调用）
    void startDrag(const QPoint &globalPos);
    void dragTo(const QPoint &globalPos);
    void endDrag();

    // 拖拽/移动结束后调用：若窗口贴近屏幕边缘则贴边隐藏，返回是否已贴边
    bool checkAndDock();

    // 贴边状态下滑回屏幕 / 重新滑出（供外部主动调用）
    void slideIn();
    void slideOut();
    void cancelDock();

    // 悬停检测参数（默认 30px 吸附阈值、6px 可见细边）
    void setSnapThreshold(int px) { m_snapThreshold = qMax(8, px); }
    void setSliverWidth(int px) { m_sliver = qBound(2, px, 40); }

    // 跟踪宿主窗口显示/隐藏：隐藏时停止悬停检测，重新显示后自动恢复
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void onHoverTick();
    void onRehideTimeout();

private:
    void startHoverPolling();
    void stopHoverPolling();
    QRect screenAvailableRect() const;
    QRect sliverRect() const;   // 贴边隐藏后窗口的几何位置（大部分在屏幕外）
    QRect visibleRect() const;  // 完全滑回屏幕时的几何位置
    QRect revealRect() const;   // 触发滑回的悬停热区
    void animateTo(const QPoint &pos);

    QWidget *m_w;
    bool m_docked = false;
    bool m_slidIn = false;
    Edge m_edge = Left;
    int m_snapThreshold = 30;
    int m_sliver = 6;

    QPoint m_dragStartGlobal;
    QPoint m_dragStartWidgetPos;
    bool m_dragging = false;

    QTimer m_hoverTimer;
    QTimer m_rehideTimer;
    QPointer<QPropertyAnimation> m_anim;
};

// 拖拽手柄：按住并移动即可拖动宿主窗口（用于无边框紧凑窗口）
class DragHandle : public QWidget
{
    Q_OBJECT
public:
    explicit DragHandle(EdgeAutoHide *helper, QWidget *parent = nullptr);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    EdgeAutoHide *m_helper;
};

#endif // EDGEAUTOHIDE_H
