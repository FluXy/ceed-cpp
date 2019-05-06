#include "src/editors/layout/LayoutPreviewerMode.h"
#include "src/editors/layout/LayoutVisualMode.h"
#include "src/editors/layout/LayoutEditor.h"
#include "src/ui/CEGUIWidget.h"
#include "src/ui/CEGUIGraphicsScene.h"
#include "qboxlayout.h"
#include <CEGUI/Window.h>
#include <CEGUI/WindowManager.h>

LayoutPreviewerMode::LayoutPreviewerMode(LayoutEditor& editor, QWidget* parent)
    : QWidget(parent)
    , IEditMode(editor)
{
    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    setLayout(layout);

    ceguiWidget = new CEGUIWidget(this);
    layout->addWidget(ceguiWidget);
    ceguiWidget->setScene(new CEGUIGraphicsScene());
    ceguiWidget->setViewFeatures(false, false, true);
    ceguiWidget->setInputEnabled(true);
}

void LayoutPreviewerMode::activate()
{
    IEditMode::activate();

    assert(!rootWidget);

    // Lets clone so we don't affect the layout at all
    auto currentRootWidget = static_cast<LayoutEditor&>(_editor).getVisualMode()->getRootWidget();
    rootWidget = currentRootWidget ? currentRootWidget->clone() : nullptr;
    ceguiWidget->getScene()->getCEGUIContext()->setRootWindow(rootWidget);
    rootWidget->setCursorPassThroughEnabled(true);

    // FIXME: need default cursor for each look'n'feel or one super-default cursor
    //ceguiWidget->getScene()->getCEGUIContext()->getCursor().setDefaultImage("TaharezLook/MouseArrow");
}

bool LayoutPreviewerMode::deactivate()
{
    if (rootWidget)
    {
        CEGUI::WindowManager::getSingleton().destroyWindow(rootWidget);
        rootWidget = nullptr;
    }

    return IEditMode::deactivate();
}
