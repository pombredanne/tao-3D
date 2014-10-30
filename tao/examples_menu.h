#ifndef EXAMPLES_MENU_H
#define EXAMPLES_MENU_H
// ****************************************************************************
//  examples_menu.h                                                Tao project
// ****************************************************************************
//
//   File Description:
//
//    Create a menu with several entries that open examples
//
//
//
//
//
//
//
//
// ****************************************************************************
// This software is licensed under the GNU General Public License v3.
// See file COPYING for details.
//  (C) 2012 Jerome Forissier <jerome@taodyne.com>
//  (C) 2012 Taodyne SAS
// ****************************************************************************


#include <QMenu>

namespace Tao {

class ExamplesMenu : public QMenu
// ----------------------------------------------------------------------------
//   Menu with entries to open Tao examples
// ----------------------------------------------------------------------------
{
    Q_OBJECT

public:
    ExamplesMenu(QString caption, QWidget *parent = 0);
    virtual ~ExamplesMenu();
    void addExample(QString caption, QString path, QString tip = "");

signals:
    void openDocument(QString path);

protected slots:
    void actionTriggered();
};

}

#endif // EXAMPLES_MENU_H
