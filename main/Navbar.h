#ifndef NAVBAR_H
#define NAVBAR_H

#include <QWidget>
#include "globals/Globals.h"
#include "globals/Filter.h"

namespace Ui {
class Navbar;
}

class Navbar : public QWidget
{
    Q_OBJECT

public:
    explicit Navbar(QWidget *parent = 0);
    ~Navbar();
    void setActionSearchEnabled(bool enabled);
    void setActionSaveEnabled(bool enabled);
    void setActionSaveAllEnabled(bool enabled);
    void setActionReloadEnabled(bool enabled);
    void setActionRenameEnabled(bool enabled);
    void setReloadToolTip(QString toolTip);

public slots:
    void setActiveWidget(MainWidgets widget);
    void setFilterWidgetEnabled(bool enabled);

signals:
    void sigSearch();
    void sigSave();
    void sigSaveAll();
    void sigReload();
    void sigRename();
    void sigSettings();
    void sigSync();
    void sigExport();
    void sigAbout();
    void sigLike();
    void sigFilterChanged(QList<Filter*>, QString);

private:
    Ui::Navbar *ui;
};

#endif // NAVBAR_H