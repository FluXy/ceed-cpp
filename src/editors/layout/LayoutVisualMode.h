#ifndef LAYOUTVISUALMODE_H
#define LAYOUTVISUALMODE_H

#include "src/editors/MultiModeEditor.h"
#include "qwidget.h"

// This is the layout visual editing mode

namespace CEGUI
{
    class Window;
}

class LayoutEditor;
class LayoutScene;
class LayoutManipulator;
class CEGUIWidget;
class CreateWidgetDockWidget;
class WidgetHierarchyDockWidget;
class QDockWidget;
class QMenu;
class QToolBar;
class QAction;

class LayoutVisualMode : public QWidget, public IEditMode
{
public:

    LayoutVisualMode(LayoutEditor& editor);

    void initialize(CEGUI::Window* rootWidget);
    void setRootWidget(CEGUI::Window* widget);
    void setRootWidgetManipulator(LayoutManipulator* manipulator);
    CEGUI::Window* getRootWidget() const;
    void rebuildEditorMenu(QMenu* editorMenu);
    void setActionsEnabled(bool enabled);

    bool cut();
    bool copy();
    bool paste();
    bool deleteSelected();
    void zoomIn();
    void zoomOut();
    void zoomReset();

    LayoutScene* getScene() const { return scene; }
    CreateWidgetDockWidget* getCreateWidgetDockWidget() const { return createWidgetDockWidget; }
    WidgetHierarchyDockWidget* getHierarchyDockWidget() const { return hierarchyDockWidget; }
    QToolBar* getToolBar() const { return toolBar; }
    QAction* getAbsoluteModeAction() const { return actionAbsoluteMode; }
    const QBrush& getSnapGridBrush() const;

    bool isAbsoluteMode() const;
    bool isAbsoluteIntegerMode() const;
    bool isSnapGridEnabled() const;

protected:

    void setupActions();
    void setupToolBar();

    void focusPropertyInspectorFilterBox();

    virtual void showEvent(QShowEvent* event) override;
    virtual void hideEvent(QHideEvent* event) override;

    mutable QBrush snapGridBrush;
    mutable bool snapGridBrushValid = false;

    LayoutScene* scene = nullptr;
    CEGUIWidget* ceguiWidget = nullptr;
    CreateWidgetDockWidget* createWidgetDockWidget = nullptr;
    WidgetHierarchyDockWidget* hierarchyDockWidget = nullptr;
    QToolBar* toolBar = nullptr;
    QMenu* _editorMenu = nullptr; // Not owned, just stored when we have control over its content

    QAction* actionAlignHLeft = nullptr;
    QAction* actionAlignHCenter = nullptr;
    QAction* actionAlignHRight = nullptr;
    QAction* actionAlignVTop = nullptr;
    QAction* actionAlignVCenter = nullptr;
    QAction* actionAlignVBottom = nullptr;
    QAction* actionNormalizePosition = nullptr;
    QAction* actionNormalizeSize = nullptr;
    QAction* actionRoundPosition = nullptr;
    QAction* actionRoundSize = nullptr;
    QAction* actionMoveBackward = nullptr;
    QAction* actionMoveForward = nullptr;
    QAction* actionAbsoluteMode = nullptr;
    QAction* actionAbsoluteIntegerMode = nullptr;
    QAction* actionSnapGrid = nullptr;
};

#endif // LAYOUTVISUALMODE_H
