// ****************************************************************************
//  branch_selection_combobox.cpp                                  Tao project
// ****************************************************************************
//
//   File Description:
//
//    A class to display a combobox that allows users to:
//      * select an existing repository branch
//      * rename a branch
//      * delete a branch
//      * create a new branch
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

#include "branch_selection_combobox.h"
#include "repository.h"
#include "application.h"
#include "tao_utf8.h"
#include <QInputDialog>
#include <QMessageBox>
#include <QCompleter>

namespace Tao {

BranchSelectionComboBox::BranchSelectionComboBox(QWidget *parent)
// ----------------------------------------------------------------------------
//    Create a branch selection combo box
// ----------------------------------------------------------------------------
    : QComboBox(parent), repo(NULL), filter(BSF_AllBranches)
{
    connect(this, SIGNAL(activated(QString)),
            this, SLOT(on_activated(QString)));
    setEnabled(false);
}


void BranchSelectionComboBox::setRepository(Repository *repo, QString sel)
// ----------------------------------------------------------------------------
//    Associate a repository to the combo box and update the UI accordingly
// ----------------------------------------------------------------------------
{
    if (!repo)
        return;
    this->repo = repo;
    populateAndSelect(sel);
    setEnabled(true);
}


QString BranchSelectionComboBox::branch()
// ----------------------------------------------------------------------------
//    The name of the currently selected branch (empty string if "<No branch>")
// ----------------------------------------------------------------------------
{
    QString result;
    if (currentIndex())
        result = currentText();
    return result;
}


void BranchSelectionComboBox::setFilter(Filter filter)
// ----------------------------------------------------------------------------
//    Select which branches should appear in the combobox
// ----------------------------------------------------------------------------
{
    this->filter = filter;
}


void BranchSelectionComboBox::refresh()
// ----------------------------------------------------------------------------
//    Refresh UI (call e.g., when current branch has changed externally)
// ----------------------------------------------------------------------------
{
    populateAndSelect();
}


bool BranchSelectionComboBox::populate()
// ----------------------------------------------------------------------------
//    Read branches from repository, fill the combo box
// ----------------------------------------------------------------------------
{
    clear();
    addItem(tr("<No branch>"), CIK_None);
    QStringList branches = repo->branches();
    int i = 1;
    foreach (QString branch, branches)
    {
        bool match = (filter == BSF_AllBranches);
        if (!match)
        {
            bool remote = repo->isRemoteBranch(+branch);
            if (remote)
                match = (filter & BSF_RemoteBranches);
            else
                match = (filter & BSF_LocalBranches);

            if (!match)
                continue;
        }
        addItem(branch);
        setItemData(i++, CIK_Name);
    }
    insertSeparator(count());
    addItem(tr("New branch..."), CIK_AddNew);

    return true;
}


bool BranchSelectionComboBox::populateAndSelect(QString sel)
// ----------------------------------------------------------------------------
//    Re-fill combo box and select a name, or current branch if sel == ""
// ----------------------------------------------------------------------------
{
    // The contents of the combo box depends on the newly selected item.
    // The combo is split in two sections:
    // (1) the list of branches, including "<No branch>", and
    // (2) a list of actions (new/rename/delete).
    // If "<No branch>" is selected, only the "New" action is present ; if a
    // name is chosen, the delete and rename actions are added, too.
    // Signal noneSelected() or branchSelected() is emitted if selection has
    // changed.

    if (!populate())
        return false;

    if (sel.isEmpty())
    {
        sel = +repo->branch();
        if (sel == "")
        {
            bool doEmit = (sel != prevSelected);
            prevSelected = sel;
            if (doEmit)
                emit noneSelected();
            return true;
        }
    }

    int index = findText(sel);
    if (index == -1)
        return false;

    int kind = itemData(index).toInt();
    if (kind != CIK_None && kind != CIK_Name)
        return false;

    bool doEmit = (sel != prevSelected);
    prevSelected = sel;
    if (kind == CIK_Name)
    {
        addItem(tr("Rename %1...").arg(sel), CIK_Rename);
        addItem(tr("Delete %1...").arg(sel), CIK_Delete);
        index = findText(sel);
        if (index == -1)
            return false;
        setCurrentIndex(index);
        if (doEmit)
            emit branchSelected(sel);
    }
    else if (kind == CIK_None)
    {
        if (doEmit)
            emit noneSelected();
    }

    return true;
}


void BranchSelectionComboBox::on_activated(QString selected)
// ----------------------------------------------------------------------------
//    Process the selection by the user of an item of the combo box
// ----------------------------------------------------------------------------
{
    if (selected == prevSelected)
        return;

    int index = currentIndex();
    int kind  = itemData(index).toInt();

    switch (kind)
    {
    case CIK_None:
        populateAndSelect();
        break;

    case CIK_Name:
        populateAndSelect(selected);
        break;

    case CIK_AddNew:
        populateAndSelect(addNewBranch());
        break;

    case CIK_Rename:
        populateAndSelect(renameBranch());
        break;

    case CIK_Delete:
        repo->checkout("master_tao_undo");  // REVISIT
        repo->delBranch(prevSelected);
        populateAndSelect();
        break;
    }
}


QString BranchSelectionComboBox::addNewBranch()
// ----------------------------------------------------------------------------
//    Prompt user for name of a new branch, create both task and undo branch
// ----------------------------------------------------------------------------
{
    QString name;

again:
    QString result = QInputDialog::getText(this, tr("New branch"),
                                           tr("Enter the name of the new "
                                              "branch:"), QLineEdit::Normal,
                                           name);
    name = result;
    if (name.replace(" ", "") != result)
    {
        QMessageBox::warning(this, tr("Invalid name"),
                             tr("Branch name cannot contain spaces"));
        goto again;
    }

    QString task, undo;
    if (!result.isEmpty())
    {
        task = +repo->taskBranch(+result);
        undo = +repo->undoBranch(+result);
        repo->addBranch(task);
        repo->addBranch(undo);
    }

    return undo;
}


QString BranchSelectionComboBox::renameBranch()
// ----------------------------------------------------------------------------
//    Prompt user for a new name and rename previously selected remote
// ----------------------------------------------------------------------------
{
    if (prevSelected.isEmpty())
        return "";

    QString result;
    result = QInputDialog::getText(this, tr("Rename branch"),
                                   tr("Enter the new name for %1:")
                                   .arg(prevSelected));
    if (!result.isEmpty())
        repo->renBranch(prevSelected, result);

    return result;
}

}
