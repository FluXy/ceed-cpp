#include "src/ui/layout/LayoutScene.h"
#include "src/ui/layout/LayoutManipulator.h"
#include "src/ui/layout/WidgetHierarchyDockWidget.h"
#include "src/ui/layout/WidgetHierarchyItem.h"
#include "src/ui/layout/AnchorCornerHandle.h"
#include "src/ui/ResizingHandle.h"
#include "src/editors/layout/LayoutVisualMode.h"
#include "src/editors/layout/LayoutUndoCommands.h"
#include "src/ui/GuideLine.h"
#include <CEGUI/CoordConverter.h>
#include <CEGUI/GUIContext.h>
#include <CEGUI/widgets/SequentialLayoutContainer.h>
#include "qgraphicssceneevent.h"
#include "qevent.h"
#include "qmimedata.h"
#include "qtreeview.h"
#include "qstandarditemmodel.h"
#include <set>

// For properties (may be incapsulated somewhere):
#include "src/ui/MainWindow.h"
#include "src/Application.h"
#include "PropertyWidget/PropertyWidget.h"

LayoutScene::LayoutScene(LayoutVisualMode& visualMode)
    : _visualMode(visualMode)
{
    connect(this, &LayoutScene::selectionChanged, this, &LayoutScene::slot_selectionChanged);
}

void LayoutScene::updateFromWidgets()
{
    if (rootManipulator) rootManipulator->updateFromWidget();
}

// Overridden to keep the manipulators in sync
void LayoutScene::setCEGUIDisplaySize(float width, float height)
{
    CEGUIGraphicsScene::setCEGUIDisplaySize(width, height);
    updateFromWidgets();
}

void LayoutScene::setRootWidgetManipulator(LayoutManipulator* manipulator)
{
    clear();

    rootManipulator = manipulator;

    if (rootManipulator)
    {
        ceguiContext->setRootWindow(rootManipulator->getWidget());

        // Root manipulator changed, perform a full update
        // NB: widget must be already set to a CEGUI context for area calculation
        rootManipulator->updateFromWidget(true);
        addItem(rootManipulator);

        // Setup anchor items

        QPen anchorRectPen(Qt::PenStyle::SolidLine);
        anchorRectPen.setColor(Qt::magenta);
        anchorRectPen.setWidth(2);
        anchorRectPen.setCosmetic(true);

        _anchorParentRect = new QGraphicsRectItem();
        _anchorParentRect->setVisible(false);
        _anchorParentRect->setPen(anchorRectPen);
        addItem(_anchorParentRect);

        QPen anchorGuidePen(Qt::PenStyle::CustomDashLine);
        anchorGuidePen.setDashPattern({ 8.0, 4.0 });
        anchorGuidePen.setColor(Qt::white);
        anchorGuidePen.setWidth(2);
        anchorGuidePen.setCosmetic(true);

        _anchorMinX = new GuideLine(false, nullptr, anchorGuidePen, Qt::cyan);
        _anchorMinX->setVisible(false);
        addItem(_anchorMinX);

        _anchorMinY = new GuideLine(true, nullptr, anchorGuidePen, Qt::cyan);
        _anchorMinY->setVisible(false);
        addItem(_anchorMinY);

        _anchorMaxX = new GuideLine(false, nullptr, anchorGuidePen, Qt::cyan);
        _anchorMaxX->setVisible(false);
        addItem(_anchorMaxX);

        _anchorMaxY = new GuideLine(true, nullptr, anchorGuidePen, Qt::cyan);
        _anchorMaxY->setVisible(false);
        addItem(_anchorMaxY);

        QPen anchorCornerPen(Qt::PenStyle::SolidLine);
        anchorCornerPen.setColor(Qt::white);
        anchorCornerPen.setWidth(1);
        anchorCornerPen.setCosmetic(true);

        _anchorMinXMinY = new AnchorCornerHandle(true, true, nullptr, 16.0, anchorCornerPen, Qt::cyan);
        _anchorMinXMinY->setVisible(false);
        addItem(_anchorMinXMinY);

        _anchorMaxXMinY = new AnchorCornerHandle(false, true, nullptr, 16.0, anchorCornerPen, Qt::cyan);
        _anchorMaxXMinY->setVisible(false);
        addItem(_anchorMaxXMinY);

        _anchorMinXMaxY = new AnchorCornerHandle(true, false, nullptr, 16.0, anchorCornerPen, Qt::cyan);
        _anchorMinXMaxY->setVisible(false);
        addItem(_anchorMinXMaxY);

        _anchorMaxXMaxY = new AnchorCornerHandle(false, false, nullptr, 16.0, anchorCornerPen, Qt::cyan);
        _anchorMaxXMaxY->setVisible(false);
        addItem(_anchorMaxXMaxY);
    }
    else
    {
        ceguiContext->setRootWindow(nullptr);

        _anchorParentRect = nullptr;
        _anchorMinX = nullptr;
        _anchorMinY = nullptr;
        _anchorMaxX = nullptr;
        _anchorMaxY = nullptr;
        _anchorMinXMinY = nullptr;
        _anchorMaxXMinY = nullptr;
        _anchorMinXMaxY = nullptr;
        _anchorMaxXMaxY = nullptr;
    }
}

LayoutManipulator* LayoutScene::getManipulatorByPath(const QString& widgetPath) const
{
    if (!rootManipulator) return nullptr;

    auto sepPos = widgetPath.indexOf('/');
    if (sepPos < 0)
    {
        assert(widgetPath == rootManipulator->getWidgetName());
        return rootManipulator;
    }
    else
    {
        assert(widgetPath.leftRef(sepPos) == rootManipulator->getWidgetName());
        return dynamic_cast<LayoutManipulator*>(rootManipulator->getManipulatorByPath(widgetPath.mid(sepPos + 1)));
    }
}

void LayoutScene::normalizePositionOfSelectedWidgets()
{
    std::vector<LayoutMoveCommand::Record> records;
    bool toRelative = false;

    for (QGraphicsItem* item : selectedItems())
    {
        auto manipulator = dynamic_cast<LayoutManipulator*>(item);
        if (!manipulator) continue;

        const auto pos = manipulator->getWidget()->getPosition();

        LayoutMoveCommand::Record rec;
        rec.path = manipulator->getWidgetPath();
        rec.oldPos = pos;
        records.push_back(std::move(rec));

        // For absolute-relative cycling
        if (!toRelative && (pos.d_x.d_offset != 0.f || pos.d_y.d_offset != 0.f))
            toRelative = true;
    }

    if (records.empty()) return;

    for (auto& rec : records)
    {
        const LayoutManipulator* manipulator = getManipulatorByPath(rec.path);
        const auto baseSize = manipulator->getBaseSize();

        if (toRelative) // command id was base + 12
        {
            rec.newPos.d_x.d_scale = (rec.oldPos.d_x.d_offset + rec.oldPos.d_x.d_scale * baseSize.d_width) / baseSize.d_width;
            rec.newPos.d_x.d_offset = 0.f;
            rec.newPos.d_y.d_scale = (rec.oldPos.d_y.d_offset + rec.oldPos.d_y.d_scale * baseSize.d_height) / baseSize.d_height;
            rec.newPos.d_y.d_offset = 0.f;
        }
        else // command id was base + 13
        {
            rec.newPos.d_x.d_scale = 0.f;
            rec.newPos.d_x.d_offset = rec.oldPos.d_x.d_offset + rec.oldPos.d_x.d_scale * baseSize.d_width;
            rec.newPos.d_y.d_scale = 0.f;
            rec.newPos.d_y.d_offset = rec.oldPos.d_y.d_offset + rec.oldPos.d_y.d_scale * baseSize.d_height;
        }
    }

    _visualMode.getEditor().getUndoStack()->push(new LayoutMoveCommand(_visualMode, std::move(records)));
}

void LayoutScene::normalizeSizeOfSelectedWidgets()
{
    std::vector<LayoutResizeCommand::Record> records;
    bool toRelative = false;

    for (QGraphicsItem* item : selectedItems())
    {
        auto manipulator = dynamic_cast<LayoutManipulator*>(item);
        if (!manipulator) continue;

        const auto size = manipulator->getWidget()->getSize();

        LayoutResizeCommand::Record rec;
        rec.path = manipulator->getWidgetPath();
        rec.oldPos = manipulator->getWidget()->getPosition();
        rec.oldSize = size;
        rec.newPos = rec.oldPos;
        records.push_back(std::move(rec));

        // For absolute-relative cycling
        if (!toRelative && (size.d_width.d_offset != 0.f || size.d_height.d_offset != 0.f))
            toRelative = true;
    }

    if (records.empty()) return;

    for (auto& rec : records)
    {
        const LayoutManipulator* manipulator = getManipulatorByPath(rec.path);
        const auto pixelSize = manipulator->getWidget()->getPixelSize();

        if (toRelative) // command id was base + 10
        {
            const auto baseSize = manipulator->getBaseSize();
            rec.newSize.d_width.d_scale = pixelSize.d_width / baseSize.d_width;
            rec.newSize.d_width.d_offset = 0.f;
            rec.newSize.d_height.d_scale = pixelSize.d_height / baseSize.d_height;
            rec.newSize.d_height.d_offset = 0.f;
        }
        else // command id was base + 11
        {
            rec.newSize.d_width.d_scale = 0.f;
            rec.newSize.d_width.d_offset = pixelSize.d_width;
            rec.newSize.d_height.d_scale = 0.f;
            rec.newSize.d_height.d_offset = pixelSize.d_height;
        }
    }

    _visualMode.getEditor().getUndoStack()->push(new LayoutResizeCommand(_visualMode, std::move(records)));
}

// command id was base + 15
void LayoutScene::roundPositionOfSelectedWidgets()
{
    std::vector<LayoutMoveCommand::Record> records;
    for (QGraphicsItem* item : selectedItems())
    {
        if (auto manipulator = dynamic_cast<LayoutManipulator*>(item))
        {
            LayoutMoveCommand::Record rec;
            rec.path = manipulator->getWidgetPath();
            rec.oldPos = manipulator->getWidget()->getPosition();
            rec.newPos = rec.oldPos;
            rec.newPos.d_x.d_offset = CEGUI::CoordConverter::alignToPixels(rec.oldPos.d_x.d_offset);
            rec.newPos.d_y.d_offset = CEGUI::CoordConverter::alignToPixels(rec.oldPos.d_y.d_offset);
            records.push_back(std::move(rec));
        }
    }

    _visualMode.getEditor().getUndoStack()->push(new LayoutMoveCommand(_visualMode, std::move(records)));
}

void LayoutScene::roundSizeOfSelectedWidgets()
{
    std::vector<LayoutResizeCommand::Record> records;
    for (QGraphicsItem* item : selectedItems())
    {
        if (auto manipulator = dynamic_cast<LayoutManipulator*>(item))
        {
            LayoutResizeCommand::Record rec;
            rec.path = manipulator->getWidgetPath();
            rec.oldPos = manipulator->getWidget()->getPosition();
            rec.newPos = rec.oldPos;
            rec.oldSize = manipulator->getWidget()->getSize();
            rec.newSize = rec.oldSize;
            rec.newSize.d_width.d_offset = CEGUI::CoordConverter::alignToPixels(rec.oldSize.d_width.d_offset);
            rec.newSize.d_height.d_offset = CEGUI::CoordConverter::alignToPixels(rec.oldSize.d_height.d_offset);
            records.push_back(std::move(rec));
        }
    }

    _visualMode.getEditor().getUndoStack()->push(new LayoutResizeCommand(_visualMode, std::move(records)));
}

void LayoutScene::alignSelectionHorizontally(CEGUI::HorizontalAlignment alignment)
{
    std::vector<LayoutHorizontalAlignCommand::Record> records;
    for (QGraphicsItem* item : selectedItems())
    {
        if (auto manipulator = dynamic_cast<LayoutManipulator*>(item))
        {
            LayoutHorizontalAlignCommand::Record rec;
            rec.path = manipulator->getWidgetPath();
            rec.oldAlignment = manipulator->getWidget()->getHorizontalAlignment();
            records.push_back(std::move(rec));
        }
    }

    _visualMode.getEditor().getUndoStack()->push(new LayoutHorizontalAlignCommand(_visualMode, std::move(records), alignment));
}

void LayoutScene::alignSelectionVertically(CEGUI::VerticalAlignment alignment)
{
    std::vector<LayoutVerticalAlignCommand::Record> records;
    for (QGraphicsItem* item : selectedItems())
    {
        if (auto manipulator = dynamic_cast<LayoutManipulator*>(item))
        {
            LayoutVerticalAlignCommand::Record rec;
            rec.path = manipulator->getWidgetPath();
            rec.oldAlignment = manipulator->getWidget()->getVerticalAlignment();
            records.push_back(std::move(rec));
        }
    }

    _visualMode.getEditor().getUndoStack()->push(new LayoutVerticalAlignCommand(_visualMode, std::move(records), alignment));
}

void LayoutScene::moveSelectedWidgetsInParentWidgetLists(int delta)
{
    QStringList paths;
    for (QGraphicsItem* item : selectedItems())
    {
        auto manipulator = dynamic_cast<LayoutManipulator*>(item);
        if (!manipulator) continue;

        auto parentManipulator = dynamic_cast<LayoutManipulator*>(item->parentItem());
        if (!parentManipulator) continue;

        auto container = dynamic_cast<CEGUI::SequentialLayoutContainer*>(parentManipulator->getWidget());
        if (!container) continue;

        int potentialPos = static_cast<int>(container->getPositionOfChild(manipulator->getWidget())) + delta;
        if (potentialPos < 0 || potentialPos >= static_cast<int>(container->getChildCount())) continue;

        paths.append(manipulator->getWidgetPath());
    }

    // TODO: We currently only support moving one widget at a time.
    //       Fixing this involves sorting the widgets by their position in
    //       the parent widget and then either working from the "right" side
    //       if delta > 0 or from the left side if delta < 0.
    if (paths.size() == 1)
        _visualMode.getEditor().getUndoStack()->push(new MoveInParentWidgetListCommand(_visualMode, std::move(paths), delta));
}

bool LayoutScene::deleteSelectedWidgets()
{
    QStringList widgetPaths;
    for (auto& item : selectedItems())
    {
        auto manip = dynamic_cast<LayoutManipulator*>(item);
        if (manip) widgetPaths.push_back(manip->getWidgetPath());
    }

    if (!widgetPaths.isEmpty())
        _visualMode.getEditor().getUndoStack()->push(new LayoutDeleteCommand(_visualMode, std::move(widgetPaths)));

    return true;
}

static void ensureParentIsExpanded(QTreeView* view, QStandardItem* treeItem)
{
    view->expand(treeItem->index());
    if (treeItem->parent())
        ensureParentIsExpanded(view, treeItem->parent());
}

void LayoutScene::slot_selectionChanged()
{
    // Collect selected widgets and anchor items

    std::set<LayoutManipulator*> selectedWidgets;
    QGraphicsItem* selectedAnchorItem = nullptr;

    auto selection = selectedItems();
    for (QGraphicsItem* item : selection)
    {
        if (auto manipulator = dynamic_cast<LayoutManipulator*>(item))
        {
            selectedWidgets.insert(manipulator);
        }
        else if (dynamic_cast<ResizingHandle*>(item))
        {
            if (auto manipulator = dynamic_cast<LayoutManipulator*>(item->parentItem()))
                selectedWidgets.insert(manipulator);
        }
        else if (item == _anchorMinX || item == _anchorMinY || item == _anchorMaxX || item == _anchorMaxY ||
                 item == _anchorMinXMinY || item == _anchorMaxXMinY || item == _anchorMinXMaxY || item == _anchorMaxXMaxY)
        {
            // Only one selected anchor item at a time is allowed
            assert(!selectedAnchorItem);
            selectedAnchorItem = item;
        }
    }

    // Update anchors state

    if (selectedWidgets.size() > 1)
    {
        // Hide anchors if we selected multiple widgets
        _anchorTarget = nullptr;
    }
    else if (selectedWidgets.size() == 0)
    {
        // If we selected anchor item, we therefore deselected an _anchorTarget,
        // but it must look as selected in a GUI so we don't change anything
        if (selectedAnchorItem) return;

        // Nothing interesting is selected, hide anchors
        _anchorTarget = nullptr;
    }
    else
    {
        // Show anchors for the only selected widget
        if (_anchorTarget == *selectedWidgets.begin()) return;
        _anchorTarget = *selectedWidgets.begin();
    }

    updateAnchorItems();

    // Update property view for our selection

    // TODO: to method (whose?)
    QtnPropertySet* propertySet = nullptr;
    for (LayoutManipulator* manipulator : selectedWidgets)
    {
        // TODO: create multi property set
        //!!!pass vector of sources & combine inside an entry point method!
        propertySet = manipulator->getPropertySet();
    }
    auto mainWindow = qobject_cast<Application*>(qApp)->getMainWindow();
    auto propertyWidget = static_cast<QtnPropertyWidget*>(mainWindow->getPropertyDockWidget()->widget());
    propertyWidget->setPropertySet(propertySet);

    // Show selection in a hierarchy tree

    if (!_ignoreSelectionChanges)
    {
        _visualMode.getHierarchyDockWidget()->ignoreSelectionChanges(true);

        _visualMode.getHierarchyDockWidget()->getTreeView()->clearSelection();

        auto treeView = _visualMode.getHierarchyDockWidget()->getTreeView();

        QStandardItem* lastTreeItem = nullptr;
        for (QGraphicsItem* item : selection)
        {
            auto manip = dynamic_cast<LayoutManipulator*>(item);
            if (manip && manip->getTreeItem())
            {
                treeView->selectionModel()->select(manip->getTreeItem()->index(), QItemSelectionModel::Select);
                ensureParentIsExpanded(treeView, manip->getTreeItem());
                lastTreeItem = manip->getTreeItem();
            }
        }

        if (lastTreeItem) treeView->scrollTo(lastTreeItem->index());

        _visualMode.getHierarchyDockWidget()->ignoreSelectionChanges(false);
    }
}

void LayoutScene::updateAnchorItems(QGraphicsItem* movedItem)
{
    const bool showAnchors = (_anchorTarget != nullptr);

    if (_anchorParentRect)
    {
        _anchorParentRect->setVisible(showAnchors);
        _anchorMinX->setVisible(showAnchors);
        _anchorMinY->setVisible(showAnchors);
        _anchorMaxX->setVisible(showAnchors);
        _anchorMaxY->setVisible(showAnchors);
        _anchorMinXMinY->setVisible(showAnchors);
        _anchorMaxXMinY->setVisible(showAnchors);
        _anchorMinXMaxY->setVisible(showAnchors);
        _anchorMaxXMaxY->setVisible(showAnchors);
    }

    if (!showAnchors || !_anchorParentRect)
    {
        _anchorMinX->setSelected(false);
        _anchorMinY->setSelected(false);
        _anchorMaxX->setSelected(false);
        _anchorMaxY->setSelected(false);
        _anchorMinXMinY->setSelected(false);
        _anchorMaxXMinY->setSelected(false);
        _anchorMinXMaxY->setSelected(false);
        _anchorMaxXMaxY->setSelected(false);
        return;
    }

    const LayoutManipulator* parentManipulator = dynamic_cast<const LayoutManipulator*>(_anchorTarget->parentItem());
    QRectF parentRect;
    if (parentManipulator)
    {
        parentRect = parentManipulator->sceneBoundingRect();
    }
    else
    {
        parentRect.setWidth(static_cast<qreal>(contextWidth));
        parentRect.setHeight(static_cast<qreal>(contextHeight));
    }

    _anchorParentRect->setRect(parentRect);

    const auto& widgetPos = _anchorTarget->getWidget()->getPosition();
    const auto& widgetSize = _anchorTarget->getWidget()->getSize();
    const qreal minX = parentRect.x() + parentRect.width() * static_cast<qreal>(widgetPos.d_x.d_scale);
    const qreal maxX = minX + parentRect.width() * static_cast<qreal>(widgetSize.d_width.d_scale);
    const qreal minY = parentRect.y() + parentRect.height() * static_cast<qreal>(widgetPos.d_y.d_scale);
    const qreal maxY = minY + parentRect.height() * static_cast<qreal>(widgetSize.d_height.d_scale);

    // Position handles without firing move events
    if (movedItem != _anchorMinX) _anchorMinX->setPosSilent(minX, 0.0);
    if (movedItem != _anchorMaxX) _anchorMaxX->setPosSilent(maxX, 0.0);
    if (movedItem != _anchorMinY) _anchorMinY->setPosSilent(0.0, minY);
    if (movedItem != _anchorMaxY) _anchorMaxY->setPosSilent(0.0, maxY);
    if (movedItem != _anchorMinXMinY) _anchorMinXMinY->setPosSilent(minX, minY);
    if (movedItem != _anchorMaxXMinY) _anchorMaxXMinY->setPosSilent(maxX, minY);
    if (movedItem != _anchorMinXMaxY) _anchorMinXMaxY->setPosSilent(minX, maxY);
    if (movedItem != _anchorMaxXMaxY) _anchorMaxXMaxY->setPosSilent(maxX, maxY);
}

// TODO: on mouse up create undo command, look at ResizingHandle
void LayoutScene::anchorHandleMoved(QGraphicsItem* item, QPointF& delta)
{
    if (!_anchorTarget || !item) return;

    // Do limiting on a pixel level, it is more convenient
    if (item == _anchorMinX)
    {
        if (_anchorMinX->pos().x() + delta.x() > _anchorMaxX->pos().x())
            delta.setX(_anchorMaxX->pos().x() - _anchorMinX->pos().x());
    }
    else if (item == _anchorMaxX)
    {
        if (_anchorMaxX->pos().x() + delta.x() < _anchorMinX->pos().x())
            delta.setX(_anchorMinX->pos().x() - _anchorMaxX->pos().x());
    }
    else if (item == _anchorMinY)
    {
        if (_anchorMinY->pos().y() + delta.y() > _anchorMaxY->pos().y())
            delta.setY(_anchorMaxY->pos().y() - _anchorMinY->pos().y());
    }
    else if (item == _anchorMaxY)
    {
        if (_anchorMaxY->pos().y() + delta.y() < _anchorMinY->pos().y())
            delta.setY(_anchorMinY->pos().y() - _anchorMaxY->pos().y());
    }
    else if (item == _anchorMinXMinY)
    {
        if (_anchorMinXMinY->pos().x() + delta.x() > _anchorMaxX->pos().x())
            delta.setX(_anchorMaxX->pos().x() - _anchorMinXMinY->pos().x());
        if (_anchorMinXMinY->pos().y() + delta.y() > _anchorMaxY->pos().y())
            delta.setY(_anchorMaxY->pos().y() - _anchorMinXMinY->pos().y());
    }
    else if (item == _anchorMaxXMinY)
    {
        if (_anchorMaxXMinY->pos().x() + delta.x() < _anchorMinX->pos().x())
            delta.setX(_anchorMinX->pos().x() - _anchorMaxXMinY->pos().x());
        if (_anchorMaxXMinY->pos().y() + delta.y() > _anchorMaxY->pos().y())
            delta.setY(_anchorMaxY->pos().y() - _anchorMaxXMinY->pos().y());
    }
    else if (item == _anchorMinXMaxY)
    {
        if (_anchorMinXMaxY->pos().x() + delta.x() > _anchorMaxX->pos().x())
            delta.setX(_anchorMaxX->pos().x() - _anchorMinXMaxY->pos().x());
        if (_anchorMinXMaxY->pos().y() + delta.y() < _anchorMinY->pos().y())
            delta.setY(_anchorMinY->pos().y() - _anchorMinXMaxY->pos().y());
    }
    else if (item == _anchorMaxXMaxY)
    {
        if (_anchorMaxXMaxY->pos().x() + delta.x() < _anchorMinX->pos().x())
            delta.setX(_anchorMinX->pos().x() - _anchorMaxXMaxY->pos().x());
        if (_anchorMaxXMaxY->pos().y() + delta.y() < _anchorMinY->pos().y())
            delta.setY(_anchorMinY->pos().y() - _anchorMaxXMaxY->pos().y());
    }

    // Early exit if nothing changed effectively
    // FIXME: integer VS float modes? float - don't round
    if (item->pos().toPoint() == (item->pos() + delta).toPoint())
        return;

    // Perform actual changes

    // CEGUIManipulator::notifyResizeProgress(QPointF newPos, QRectF newRect)
    // CEGUIManipulator::notifyResizeFinished(QPointF newPos, QRectF newRect)
    //_anchorTarget->updatePropertiesFromWidget({"Position", "Area"});

    // TODO: common code!
    /*
    // Absolute pixel deltas
    auto pixelDeltaPos = newPos - resizeOldPos;
    auto pixelDeltaSize = newRect.size() - resizeOldRect.size();

    CEGUI::UVector2 deltaPos;
    CEGUI::USize deltaSize;
    if (useAbsoluteCoordsForResize())
    {
        if (useIntegersForAbsoluteResize())
        {
            roundPointFloor(pixelDeltaPos);
            roundSizeFloor(pixelDeltaSize);
        }

        deltaPos = CEGUI::UVector2(
                    CEGUI::UDim(0.f, static_cast<float>(pixelDeltaPos.x())),
                    CEGUI::UDim(0.f, static_cast<float>(pixelDeltaPos.y())));
        deltaSize = CEGUI::USize(
                    CEGUI::UDim(0.f, static_cast<float>(pixelDeltaSize.width())),
                    CEGUI::UDim(0.f, static_cast<float>(pixelDeltaSize.height())));
    }
    else
    {
        auto baseSize = getBaseSize();
        deltaPos = CEGUI::UVector2(
                    CEGUI::UDim(static_cast<float>(pixelDeltaPos.x()) / baseSize.d_width, 0.f),
                    CEGUI::UDim(static_cast<float>(pixelDeltaPos.y()) / baseSize.d_height, 0.f));
        deltaSize = CEGUI::USize(
                    CEGUI::UDim(static_cast<float>(pixelDeltaSize.width()) / baseSize.d_width, 0.f),
                    CEGUI::UDim(static_cast<float>(pixelDeltaSize.height()) / baseSize.d_height, 0.f));
    }

    // Because the Qt manipulator is always top left aligned in the CEGUI sense,
    // we have to process the size to factor in alignments if they differ
    CEGUI::UVector2 processedDeltaPos;
    switch (_widget->getHorizontalAlignment())
    {
        case CEGUI::HorizontalAlignment::Left:
            processedDeltaPos.d_x = deltaPos.d_x; break;
        case CEGUI::HorizontalAlignment::Centre:
            processedDeltaPos.d_x = deltaPos.d_x + CEGUI::UDim(0.5f, 0.5f) * deltaSize.d_width; break;
        case CEGUI::HorizontalAlignment::Right:
            processedDeltaPos.d_x = deltaPos.d_x + deltaSize.d_width; break;
    }
    switch (_widget->getVerticalAlignment())
    {
        case CEGUI::VerticalAlignment::Top:
            processedDeltaPos.d_y = deltaPos.d_y; break;
        case CEGUI::VerticalAlignment::Centre:
            processedDeltaPos.d_y = deltaPos.d_y + CEGUI::UDim(0.5f, 0.5f) * deltaSize.d_height; break;
        case CEGUI::VerticalAlignment::Bottom:
            processedDeltaPos.d_y = deltaPos.d_y + deltaSize.d_height; break;
    }

    _widget->setPosition(_preResizePos + processedDeltaPos);
    _widget->setSize(_preResizeSize + deltaSize);

    _lastResizeNewPos = newPos;
    _lastResizeNewRect = newRect;
    */

    updateAnchorItems(item);
}

void LayoutScene::dragEnterEvent(QGraphicsSceneDragDropEvent* event)
{
    // If the root manipulator is in place the QGraphicsScene machinery will take care of drag n drop
    // the graphics items (manipulators in fact) have that implemented already
    if (rootManipulator)
        CEGUIGraphicsScene::dragEnterEvent(event);
    else
    {
        // Otherwise we should accept a new root widget to the empty layout if it's a new widget
        if (event->mimeData()->hasFormat("application/x-ceed-widget-type"))
            event->acceptProposedAction();
    }
}

void LayoutScene::dragLeaveEvent(QGraphicsSceneDragDropEvent* event)
{
    // If the root manipulator is in place the QGraphicsScene machinery will take care of drag n drop
    // the graphics items (manipulators in fact) have that implemented already
    if (rootManipulator)
        CEGUIGraphicsScene::dragEnterEvent(event);
}

void LayoutScene::dragMoveEvent(QGraphicsSceneDragDropEvent* event)
{
    // If the root manipulator is in place the QGraphicsScene machinery will take care of drag n drop
    // the graphics items (manipulators in fact) have that implemented already
    if (rootManipulator)
        CEGUIGraphicsScene::dragMoveEvent(event);
    else
    {
        // Otherwise we should accept a new root widget to the empty layout if it's a new widget
        if (event->mimeData()->hasFormat("application/x-ceed-widget-type"))
            event->acceptProposedAction();
    }
}

void LayoutScene::dropEvent(QGraphicsSceneDragDropEvent* event)
{
    // If the root manipulator is in place the QGraphicsScene machinery will take care of drag n drop
    // the graphics items (manipulators in fact) have that implemented already
    if (rootManipulator)
        CEGUIGraphicsScene::dropEvent(event);
    else
    {
        auto data = event->mimeData()->data("application/x-ceed-widget-type");
        if (data.size() > 0)
        {
            QString widgetType = data.data();
            int sepPos = widgetType.lastIndexOf('/');
            QString widgetName = (sepPos < 0) ? widgetType : widgetType.mid(sepPos + 1);
            _visualMode.getEditor().getUndoStack()->push(new LayoutCreateCommand(_visualMode, "", widgetType, widgetName));

            event->acceptProposedAction();
        }
        else
        {
            event->ignore();
        }
    }
}

void LayoutScene::keyReleaseEvent(QKeyEvent* event)
{
    bool handled = false;

    if (event->key() == Qt::Key_Delete)
    {
        handled = deleteSelectedWidgets();
    }

    if (handled)
        event->accept();
    else
        CEGUIGraphicsScene::keyReleaseEvent(event);
}

void LayoutScene::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
    CEGUIGraphicsScene::mouseReleaseEvent(event);

    // We have to "expand" the items, adding parents of resizing handles instead of the handles themselves
    std::vector<LayoutManipulator*> selection;
    for (QGraphicsItem* selectedItem : selectedItems())
    {
        if (auto manipulator = dynamic_cast<LayoutManipulator*>(selectedItem))
        {
            selection.push_back(manipulator);
            continue;
        }

        if (auto handle = dynamic_cast<ResizingHandle*>(selectedItem))
        {
            if (auto manipulator = dynamic_cast<LayoutManipulator*>(handle->parentItem()))
            {
                selection.push_back(manipulator);
                continue;
            }
        }
    }

    std::vector<LayoutMoveCommand::Record> move;
    std::vector<LayoutResizeCommand::Record> resize;

    for (LayoutManipulator* item : selection)
    {
        if (item->isMoveStarted())
        {
            LayoutMoveCommand::Record rec;
            rec.path = item->getWidgetPath();
            rec.oldPos = item->getMoveStartPosition();
            rec.newPos = item->getWidget()->getPosition();
            move.push_back(std::move(rec));

            item->resetMove();
        }

        if (item->isResizeStarted())
        {
            LayoutResizeCommand::Record rec;
            rec.path = item->getWidgetPath();
            rec.oldPos = item->getResizeStartPosition();
            rec.newPos = item->getWidget()->getPosition();
            rec.oldSize = item->getResizeStartSize();
            rec.newSize = item->getWidget()->getSize();
            resize.push_back(std::move(rec));

            item->resetResize();
        }
    }

    if (!move.empty())
        _visualMode.getEditor().getUndoStack()->push(new LayoutMoveCommand(_visualMode, std::move(move)));

    if (!resize.empty())
        _visualMode.getEditor().getUndoStack()->push(new LayoutResizeCommand(_visualMode, std::move(resize)));
}
