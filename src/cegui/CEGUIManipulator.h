#ifndef CEGUIMANIPULATOR_H
#define CEGUIMANIPULATOR_H

#include "src/ui/ResizableRectItem.h"

// This is a rectangle that is synchronised with given CEGUI widget,
// it provides moving and resizing functionality

class CEGUIManipulator : public ResizableRectItem
{
public:

    CEGUIManipulator(QGraphicsItem* parent = nullptr);

    virtual QSizeF getMinSize() const override;
    virtual QSizeF getMaxSize() const override;

    virtual void notifyHandleSelected(ResizingHandle* handle) override;
    virtual void notifyResizeStarted(ResizingHandle* handle) override;
    virtual void notifyResizeProgress(QPointF newPos, QRectF newRect) override;
    virtual void notifyResizeFinished(QPointF newPos, QRectF newRect) override;
    virtual void notifyMoveStarted() override;
    virtual void notifyMoveProgress(QPointF newPos) override;
    virtual void notifyMoveFinished(QPointF newPos) override;

    virtual void updateFromWidget(bool callUpdate = false, bool updateAncestorLCs = false);
    virtual void detach(bool detachWidget = true, bool destroyWidget = true, bool recursive = true);

    void moveToFront();
    void triggerPropertyManagerCallback(QStringList propertyNames);

protected:

    virtual QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;
};

#endif // CEGUIMANIPULATOR_H