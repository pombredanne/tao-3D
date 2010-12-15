// ****************************************************************************
//  xl_source_edit.cpp                                             Tao project
// ****************************************************************************
//
//   File Description:
//
//     XLSourceEdit implementation.
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


#include "xl_source_edit.h"
#include "xl_highlighter.h"
#include <QScrollBar>
#include <QEvent>
#include <QKeyEvent>

namespace Tao {

XLSourceEdit::XLSourceEdit(QWidget *parent)
    : QTextEdit(parent)
// ----------------------------------------------------------------------------
//   Create a source editor
// ----------------------------------------------------------------------------
{
    QFont font("unifont");
    font.setPixelSize(16);
    setFont(font);
    highlighter = new XLHighlighter(document());
}


void XLSourceEdit::setPlainTextKeepCursor(const QString &txt)
// ----------------------------------------------------------------------------
//   Update the text, try not to change the scrollbars and cursor positions
// ----------------------------------------------------------------------------
{
    QScrollBar * hsb = horizontalScrollBar();
    QScrollBar * vsb = verticalScrollBar();
    int x = hsb->value();
    int y = vsb->value();
    int pos = textCursor().position();

    setPlainText(txt);

    hsb->setValue(x);
    vsb->setValue(y);
    QTextCursor cursor(document());
    int last = document()->characterCount();
    if (pos < last)
    {
        cursor.setPosition(pos);
        setTextCursor(cursor);
    }
    update();
}


void XLSourceEdit::setXLNames(const QStringList &names)
// ----------------------------------------------------------------------------
//   Define the list of XL names for syntax highlighting
// ----------------------------------------------------------------------------
{
    highlighter->setXLNames(names);
}


void XLSourceEdit::setSelectedRanges(const XL::stream_ranges &ranges)
// ----------------------------------------------------------------------------
//   Tell the highlighter the ranges of text that correspond to selected items
// ----------------------------------------------------------------------------
{
    highlighter->setSelectedRanges(ranges);
}


bool XLSourceEdit::event(QEvent *e)
// ----------------------------------------------------------------------------
//   Process events sent to the text edit
// ----------------------------------------------------------------------------
{
    // Special handling of Tab/Shit+Tab keys has to be done here, because when
    // keyPressEvent is called it is too late (Qt already has taken special
    // actions such as changing focus)
    if (e->type() == QEvent::KeyPress)
    {
        bool done = false;
        QKeyEvent *ke = (QKeyEvent*)e;
        if (ke->key() == Qt::Key_Tab)
        {
            if (ke->modifiers() & Qt::ShiftModifier)
                done = handleShiftTabKey(ke);
            else
                done = handleTabKey(ke);
        }
        if (done)
            return done;
    }
    return QTextEdit::event(e);
}


bool XLSourceEdit::handleTabKey(QKeyEvent *)
// ----------------------------------------------------------------------------
//   Replace tab with 4 spaces
// ----------------------------------------------------------------------------
{
    textCursor().insertText(QString(4, ' '));
    return true;
}


bool XLSourceEdit::handleShiftTabKey(QKeyEvent *)
// ----------------------------------------------------------------------------
//   Insert a real tab
// ----------------------------------------------------------------------------
{
    textCursor().insertText("\t");
    return true;
}


void XLSourceEdit::keyPressEvent(QKeyEvent *e)
// ----------------------------------------------------------------------------
//   Editor receives key press
// ----------------------------------------------------------------------------
{
    if (highlighter->hasSelectedObjects())
        highlighter->clearSelectedRanges();
    QTextEdit::keyPressEvent(e);
}

}