// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later

#include "edgeautohide.h"

#include <QWidget>
#include <QGuiApplication>
#include <QScreen>
#include <QCursor>
#include <QEvent>
#include <QMouseEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QPropertyAnimation>
#include <QEasingCurve>
#include <QAbstractAnimation>
#include <QDebug>

namespace {
constexpr int kHoverPollMs = 80;     // 贴边后悬停检测周期
constexpr int kRehideDelayMs = 800;  // 鼠标离开后重新隐藏的延时
constexpr int kRevealMargin = 18;    // 悬停热区在细边外扩大的范围
constexpr int kLeaveMargin = 12;     // 判断“鼠标已离开窗口”的容差
} // namespace

EdgeAutoHide::EdgeAutoHide(QWidget *w, QObject *parent)
    : QObject(parent)
    , m_w(w)
{
    m_hoverTimer.setInterval(kHoverPollMs);
    connect(&m_hoverTimer, &QTimer::timeout, this, &EdgeAutoHide::onHoverTick);

    m_rehideTimer.setSingleShot(true);
    m_rehideTimer.setInterval(kRehideDelayMs);
    connect(&m_rehideTimer, &QTimer::timeout, this, &EdgeAutoHide::onRehideTimeout);

    if (m_w) {
        m_w->installEventFilter(this);
    }
}

bool EdgeAutoHide::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_w) {
        if (event->type() == QEvent::Show) {
            if (m_docked) {
                startHoverPolling();
            }
        } else if (event->type() == QEvent::Hide) {
            stopHoverPolling();
        }
    }
    return QObject::eventFilter(watched, event);
}

void EdgeAutoHide::startDrag(const QPoint &globalPos)
{
    if (!m_w) return;

    if (m_docked) {
        // 从细边/滑入状态抓取窗口：先滑回屏幕内再跟随鼠标，
        // 避免窗口大部分位于屏幕外时松手被误判为仍贴边而再次隐藏
        const QPoint visiblePos = visibleRect().topLeft();
        stopHoverPolling();
        m_rehideTimer.stop();
        if (m_anim) {
            m_anim->stop();
        }
        m_docked = false;
        m_slidIn = false;
        m_w->move(visiblePos);
    }

    m_dragging = true;
    m_dragStartGlobal = globalPos;
    m_dragStartWidgetPos = m_w->pos();
}

void EdgeAutoHide::dragTo(const QPoint &globalPos)
{
    if (!m_dragging || !m_w) return;
    m_w->move(m_dragStartWidgetPos + (globalPos - m_dragStartGlobal));
}

void EdgeAutoHide::endDrag()
{
    if (!m_dragging) return;
    m_dragging = false;
    checkAndDock();
}

QRect EdgeAutoHide::screenAvailableRect() const
{
    if (!m_w) return QRect();
    QScreen *screen = QGuiApplication::screenAt(m_w->frameGeometry().center());
    if (!screen) {
        screen = QGuiApplication::primaryScreen();
    }
    if (!screen) return QRect();
    return screen->availableGeometry();
}

bool EdgeAutoHide::checkAndDock()
{
    if (!m_w || !m_w->isVisible()) {
        return false;
    }

    const QRect avail = screenAvailableRect();
    if (avail.isEmpty()) {
        return false;
    }

    const QRect geo = m_w->frameGeometry();
    const int distLeft = geo.left() - avail.left();
    const int distRight = avail.right() - geo.right();
    const int distTop = geo.top() - avail.top();
    const int distBottom = avail.bottom() - geo.bottom();

    Edge nearest = Left;
    int minDist = distLeft;
    if (distRight < minDist) { minDist = distRight; nearest = Right; }
    if (distTop < minDist) { minDist = distTop; nearest = Top; }
    if (distBottom < minDist) { minDist = distBottom; nearest = Bottom; }

    if (minDist > m_snapThreshold) {
        cancelDock();
        return false;
    }

    m_edge = nearest;
    m_docked = true;
    m_slidIn = true; // 当前还在屏幕上，立即执行滑出
    startHoverPolling();
    slideOut();
    return true;
}

void EdgeAutoHide::slideIn()
{
    if (!m_docked || !m_w || m_slidIn) return;
    m_slidIn = true;
    m_rehideTimer.stop();
    animateTo(visibleRect().topLeft());
}

void EdgeAutoHide::slideOut()
{
    if (!m_docked || !m_w) return;
    m_slidIn = false;
    m_rehideTimer.stop();
    animateTo(sliverRect().topLeft());
}

void EdgeAutoHide::cancelDock()
{
    if (m_anim) {
        m_anim->stop();
    }
    m_docked = false;
    m_slidIn = false;
    stopHoverPolling();
    m_rehideTimer.stop();
}

void EdgeAutoHide::startHoverPolling()
{
    if (!m_hoverTimer.isActive()) {
        m_hoverTimer.start();
    }
}

void EdgeAutoHide::stopHoverPolling()
{
    m_hoverTimer.stop();
}

QRect EdgeAutoHide::sliverRect() const
{
    const QRect avail = screenAvailableRect();
    const QRect geo = m_w->frameGeometry();
    QRect docked = geo;
    switch (m_edge) {
    case Left:
        docked.moveLeft(avail.left() - (geo.width() - m_sliver));
        break;
    case Right:
        docked.moveLeft(avail.right() - m_sliver + 1);
        break;
    case Top:
        docked.moveTop(avail.top() - (geo.height() - m_sliver));
        break;
    case Bottom:
        docked.moveTop(avail.bottom() - m_sliver + 1);
        break;
    }
    return docked;
}

QRect EdgeAutoHide::visibleRect() const
{
    const QRect avail = screenAvailableRect();
    const QRect geo = m_w->frameGeometry();
    QRect visible = geo;
    switch (m_edge) {
    case Left:
        visible.moveLeft(avail.left());
        break;
    case Right:
        visible.moveLeft(avail.right() - geo.width() + 1);
        break;
    case Top:
        visible.moveTop(avail.top());
        break;
    case Bottom:
        visible.moveTop(avail.bottom() - geo.height() + 1);
        break;
    }
    return visible;
}

QRect EdgeAutoHide::revealRect() const
{
    return sliverRect().adjusted(-kRevealMargin, -kRevealMargin, kRevealMargin, kRevealMargin);
}

void EdgeAutoHide::animateTo(const QPoint &pos)
{
    if (!m_w) return;
    if (m_anim) {
        m_anim->stop();
    }
    m_anim = new QPropertyAnimation(m_w, "pos", this);
    m_anim->setDuration(180);
    m_anim->setStartValue(m_w->pos());
    m_anim->setEndValue(pos);
    m_anim->setEasingCurve(QEasingCurve::OutCubic);
    m_anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void EdgeAutoHide::onHoverTick()
{
    if (!m_docked || !m_w) {
        stopHoverPolling();
        return;
    }
    if (!m_w->isVisible()) {
        // 窗口被关闭/隐藏时停止悬停检测（贴边状态保留，重新显示后自动恢复）
        stopHoverPolling();
        return;
    }

    const QPoint cursor = QCursor::pos();

    if (!m_slidIn) {
        // 贴边隐藏中：鼠标悬停到细边热区则滑回屏幕
        if (revealRect().contains(cursor)) {
            slideIn();
        }
        return;
    }

    // 已滑回屏幕：鼠标离开窗口（及细边热区）后延时重新隐藏
    const QRect leaveZone = m_w->frameGeometry().adjusted(-kLeaveMargin, -kLeaveMargin, kLeaveMargin, kLeaveMargin);
    if (leaveZone.contains(cursor) || revealRect().contains(cursor)) {
        m_rehideTimer.stop();
    } else if (!m_rehideTimer.isActive()) {
        m_rehideTimer.start();
    }
}

void EdgeAutoHide::onRehideTimeout()
{
    if (m_docked && m_slidIn) {
        slideOut();
    }
}

// ─── DragHandle ─────────────────────────────────────────────────────

DragHandle::DragHandle(EdgeAutoHide *helper, QWidget *parent)
    : QWidget(parent)
    , m_helper(helper)
{
    setCursor(Qt::OpenHandCursor);
    setToolTip(tr("拖动到屏幕边缘可贴边隐藏"));
    setFixedSize(24, 24);

    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    QLabel *grip = new QLabel(QString::fromUtf8("⠿"), this);
    grip->setAlignment(Qt::AlignCenter);
    grip->setStyleSheet("color: palette(placeholderText); font-size: 14px; background: transparent;");
    layout->addWidget(grip);
}

void DragHandle::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_helper) {
        m_helper->startDrag(event->globalPosition().toPoint());
        grabMouse();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void DragHandle::mouseMoveEvent(QMouseEvent *event)
{
    if (m_helper) {
        m_helper->dragTo(event->globalPosition().toPoint());
        event->accept();
        return;
    }
    QWidget::mouseMoveEvent(event);
}

void DragHandle::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_helper) {
        releaseMouse();
        m_helper->endDrag();
        setCursor(Qt::OpenHandCursor);
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}
