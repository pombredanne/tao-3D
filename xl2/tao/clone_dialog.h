#ifndef CLONE_DIALOG_H
#define CLONE_DIALOG_H
// ****************************************************************************
//  clone_dialog.h                                                 Tao project
// ****************************************************************************
//
//   File Description:
//
//    The class to display the "Clone" dialog box
//
//
//
//
//
//
//
//
// ****************************************************************************
// This document is released under the GNU General Public License.
// See http://www.gnu.org/copyleft/gpl.html and Matthew 25:22 for details
//  (C) 2010 Jerome Forissier <jerome@taodyne.com>
//  (C) 2010 Taodyne SAS
// ****************************************************************************

#include "ui_clone_dialog.h"
#include "repository.h"
#include <QDialog>
#include <QPushButton>

namespace Tao {

struct Repository;

class CloneDialog : public QDialog, private Ui::CloneDialog
{
    Q_OBJECT

public:
    CloneDialog(Repository *repo, QWidget *parent = 0);

public slots:
    virtual void accept();
    virtual void reject();
    virtual void on_browseButton_clicked();
    virtual void endClone(void *id);

private:
    Repository  *repo;
    bool         done;
    QPushButton *okButton, *cancelButton;
    Process     *proc;
};

}

#endif // CLONE_DIALOG_H
