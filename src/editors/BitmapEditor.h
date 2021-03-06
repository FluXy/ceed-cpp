#ifndef BITMAPEDITOR_H
#define BITMAPEDITOR_H

#include "src/editors/EditorBase.h"
#include "src/ui/widgets/BitmapEditorWidget.h"

// A simple external bitmap editor starter/image viewer

class BitmapEditor : public EditorBase
{
public:

    BitmapEditor(const QString& filePath);

    virtual void initialize() override;

    virtual QWidget* getWidget() override { return &widget; }

protected:

    BitmapEditorWidget widget;
};

class BitmapEditorFactory : public EditorFactoryBase
{
public:

    virtual QString getFileTypesDescription() const override;
    virtual QStringList getFileExtensions() const override;
    virtual EditorBasePtr create(const QString& filePath) const override;
};

#endif // BITMAPEDITOR_H
