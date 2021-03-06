#include "src/util/SettingsEntry.h"
#include "src/util/SettingsSection.h"
#include "src/util/SettingsCategory.h"
#include "src/util/Settings.h"
//#include "src/Application.h" // an alternative way to obtain QSettings without category pointer
#include "qsettings.h"

SettingsEntry::SettingsEntry(SettingsSection& section, const QString& name, const QVariant& defaultValue, const QString& label,
                             const QString& help, const QString& widgetHint, bool changeRequiresRestart, int sortingWeight,
                             SettingOptionList&& optionList)
    : _section(section)
    , _name(name)
    , _label(label.isEmpty() ? name : label)
    , _help(help)
    , _widgetHint(widgetHint)
    , _value(defaultValue)
    , _editedValue(defaultValue)
    , _defaultValue(defaultValue)
    , _optionList(std::move(optionList))
    , _sortingWeight(sortingWeight)
    , _changeRequiresRestart(changeRequiresRestart)
{
}

SettingsEntry::~SettingsEntry()
{
}

// Retrieves a unique path in the qsettings tree, this can be used by persistence providers for example
QString SettingsEntry::getPath() const
{
    return _section.getPath() + "/" + _name;
}

bool SettingsEntry::sanitizeValue(QVariant& val) const
{
    // Exactly the same type, can use as is
    if (val.type() == _defaultValue.type())
        return true;

    // Compatible type is converted to our type
    // NB: failed convert() results in an empty val of requested type
    return (val.convert(_defaultValue.type()));
}

void SettingsEntry::setValue(const QVariant& val, bool storeImmediately)
{
    const QVariant prevValue = _value;

    QVariant copy = val;
    if (!sanitizeValue(copy)) return;

    _value = copy;
    _editedValue = copy;

    if (storeImmediately) store();

    if (prevValue != _value)
        emit valueChanged(_value);
}

void SettingsEntry::setEditedValue(const QVariant& val)
{
    QVariant copy = val;
    if (!sanitizeValue(copy)) return;
    _editedValue = copy;
}

void SettingsEntry::applyChanges()
{
    if (_value != _editedValue)
    {
        setValue(_editedValue);

        if (_changeRequiresRestart)
        {
            //qobject_cast<Application*>(qApp)->getSettings().markRequiresRestart();
            _section.getCategory().getSettings().markRequiresRestart();
        }
    }
}

void SettingsEntry::load()
{
    //QSettings* qsettings = qobject_cast<Application*>(qApp)->getSettings().getQSettings();
    QSettings* qsettings = _section.getCategory().getSettings().getQSettings();
    QVariant val = qsettings->value(getPath());
    if (val.isNull()) val = _defaultValue;

/*
        # http://bugs.pyside.org/show_bug.cgi?id=345
        if self.widgetHint == "checkbox":
            if persistedValue == "false":
                persistedValue = False
            elif persistedValue == "true":
                persistedValue = True
*/

    setValue(val, false);
}

void SettingsEntry::store()
{
    //QSettings* qsettings = qobject_cast<Application*>(qApp)->getSettings().getQSettings();
    QSettings* qsettings = _section.getCategory().getSettings().getQSettings();
    qsettings->setValue(getPath(), _value);
}
