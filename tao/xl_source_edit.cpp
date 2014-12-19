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
// This software is licensed under the GNU General Public License v3.
// See file COPYING for details.
//  (C) 2010 Jerome Forissier <jerome@taodyne.com>
//  (C) 2010 Taodyne SAS
// ****************************************************************************


#include "xl_source_edit.h"
#include "xl_highlighter.h"
#include "tao_utf8.h"
#include "assistant.h"
#include "context.h"
#include <QScrollBar>
#include <QEvent>
#include <QKeyEvent>
#include <QFileInfo>

namespace Tao {

XLSourceEdit::XLSourceEdit(QWidget *parent)
// ----------------------------------------------------------------------------
//   Create a source editor
// ----------------------------------------------------------------------------
    : QTextEdit(parent)
{
    QFont font("unifont");
    font.setPixelSize(16);
    setFont(font);
    highlighter = new XLHighlighter(document());
    setLineWrapMode(QTextEdit::NoWrap);

    renderer = new XL::Renderer(rendererOut);
    QFileInfo stylesheet("xl:git.stylesheet");
    QFileInfo syntax("xl:xl.syntax");
    QString sspath(stylesheet.absoluteFilePath());
    QString sypath(syntax.absoluteFilePath());
    renderer->SelectStyleSheet(+sspath, +sypath);
}


XLSourceEdit::~XLSourceEdit()
// ----------------------------------------------------------------------------
//   Destroy source editor
// ----------------------------------------------------------------------------
{
    delete highlighter;
    delete renderer;
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


void XLSourceEdit::highlightNames(int index, std::set<text> &set)
// ----------------------------------------------------------------------------
//   Define the list of XL names for syntax highlighting
// ----------------------------------------------------------------------------
{
    highlighter->highlightNames(index, set);
}


void XLSourceEdit::setSelectedRanges(const XL::stream_ranges &ranges)
// ----------------------------------------------------------------------------
//   Tell the highlighter the ranges of text that correspond to selected items
// ----------------------------------------------------------------------------
{
    highlighter->setSelectedRanges(ranges);
}


void XLSourceEdit::render(XL::Tree_p prog, std::set<XL::Tree_p> *selected)
// ----------------------------------------------------------------------------
//   Show prog source
// ----------------------------------------------------------------------------
{
    if (!prog)
        return clear();

    text txt = "";
    rendererOut.str(txt);

    // Tell renderer how to highlight selected items
    std::set<XL::Tree_p >::iterator i;
    renderer->highlights.clear();
    if (selected)
        for (i = selected->begin(); i != selected->end(); i++)
            renderer->highlights[*i] = "selected";
    renderer->RenderFile(prog);

    XL::stream_ranges sel;
    if (renderer->highlighted.count("selected"))
        sel = renderer->highlighted["selected"];
    setSelectedRanges(sel);

    txt = rendererOut.str();
    setPlainTextKeepCursor(+txt);
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
        else if (ke->key() == Qt::Key_F1)
        {
            done = handleF1Key(ke);
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


bool XLSourceEdit::handleF1Key(QKeyEvent *)
// ----------------------------------------------------------------------------
//   Open help about the word under the cursor
// ----------------------------------------------------------------------------
{
    QTextCursor tc = textCursor();
    tc.select(QTextCursor::WordUnderCursor);
    QString keyword = tc.selectedText();
    if (!keyword.isEmpty())
        Assistant::instance()->showKeywordHelp(keyword);
    return true;
}


void XLSourceEdit::keyPressEvent(QKeyEvent *e)
// ----------------------------------------------------------------------------
//   Editor receives key press
// ----------------------------------------------------------------------------
{
    if (e->key() == Qt::Key_Escape)
        return;
    if (highlighter->hasSelectedObjects())
        highlighter->clearSelectedRanges();
    QTextEdit::keyPressEvent(e);
}

}
