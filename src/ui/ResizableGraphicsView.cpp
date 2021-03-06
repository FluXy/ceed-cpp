#include "src/ui/ResizableGraphicsView.h"
#include "src/ui/ResizableRectItem.h"
#include "src/ui/ResizingHandle.h"
#include "src/Application.h"
#include "src/util/Settings.h"
#include "qevent.h"
#include "qscrollbar.h"

ResizableGraphicsView::ResizableGraphicsView(QWidget *parent)
    : QGraphicsView(parent)
{
    auto&& settings = qobject_cast<Application*>(qApp)->getSettings();
    ctrlZoom = settings->getEntryValue("global/navigation/ctrl_zoom", true).toBool();

    // Reset to unreachable value
    lastDragScrollMousePosition.setX(-10000);
    lastDragScrollMousePosition.setY(-10000);
}

void ResizableGraphicsView::setTransform(const QTransform& transform)
{
    QGraphicsView::setTransform(transform);

    const qreal scaleX = transform.m11();
    const qreal scaleY = transform.m22();

    // Handle scale change
    for (auto& item : scene()->items())
    {
        auto rectItem = dynamic_cast<ResizableRectItem*>(item);
        if (rectItem) rectItem->onScaleChanged(scaleX, scaleY);
    }
}

void ResizableGraphicsView::zoomIn()
{
    zoomFactor = std::min(zoomFactor * 2.0, 256.0);
    setTransform(QTransform::fromScale(zoomFactor, zoomFactor));
}

void ResizableGraphicsView::zoomOut()
{
    zoomFactor = std::max(zoomFactor * 0.5, 0.125);
    setTransform(QTransform::fromScale(zoomFactor, zoomFactor));
}

void ResizableGraphicsView::zoomReset()
{
    zoomFactor = 1.0;
    setTransform(QTransform::fromScale(zoomFactor, zoomFactor));
}

void ResizableGraphicsView::wheelEvent(QWheelEvent *event)
{
    if (wheelZoomEnabled && (!ctrlZoom || (event->modifiers() & Qt::ControlModifier)))
    {
        if (event->delta() > 0)
            zoomIn();
        else if (event->delta() < 0)
            zoomOut();

        return;
    }

    QGraphicsView::wheelEvent(event);
}

void ResizableGraphicsView::mousePressEvent(QMouseEvent *event)
{
    if (middleButtonDragScrollEnabled && event->buttons() == Qt::MiddleButton)
    {
        lastDragScrollMousePosition = event->pos();
        return;
    }

    QGraphicsView::mousePressEvent(event);
}

// When mouse is released in a resizable view, we have to go through all selected items
// and notify them of the release. This helps track undo movement and undo resize way easier.
void ResizableGraphicsView::mouseReleaseEvent(QMouseEvent *event)
{
    // Handle scale change
    for (auto& selectedItem : scene()->selectedItems())
    {
        auto rectItem = dynamic_cast<ResizableRectItem*>(selectedItem);
        if (rectItem)
        {
            rectItem->mouseReleaseEventSelected(event);
            continue;
        }

        auto handle = dynamic_cast<ResizingHandle*>(selectedItem);
        if (handle)
        {
            handle->mouseReleaseEventSelected(event);
            continue;
        }
    }

    QGraphicsView::mouseReleaseEvent(event);
}

void ResizableGraphicsView::mouseMoveEvent(QMouseEvent *event)
{
    if (middleButtonDragScrollEnabled && event->buttons() == Qt::MiddleButton)
    {
        auto horizontal = horizontalScrollBar();
        horizontal->setSliderPosition(horizontal->sliderPosition() - (event->pos().x() - lastDragScrollMousePosition.x()));
        auto vertical = verticalScrollBar();
        vertical->setSliderPosition(vertical->sliderPosition() - (event->pos().y() - lastDragScrollMousePosition.y()));
        lastDragScrollMousePosition = event->pos();
        return;
    }

    QGraphicsView::mouseMoveEvent(event);
}
