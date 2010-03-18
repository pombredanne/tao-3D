// ****************************************************************************
//  widget.cpp							   Tao project
// ****************************************************************************
//
//   File Description:
//
//     The main widget used to display some Tao stuff
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
//  (C) 1992-2010 Christophe de Dinechin <christophe@taodyne.com>
//  (C) 2010 Lionel Schaffhauser <lionel@taodyne.com>
//  (C) 2010 Taodyne SAS
// ****************************************************************************

#include "widget.h"
#include "tao.h"
#include "main.h"
#include "runtime.h"
#include "opcodes.h"
#include "gl_keepers.h"
#include "frame.h"
#include "texture.h"
#include "svg.h"
#include "widget-surface.h"
#include "window.h"
#include "treeholder.h"
#include "apply-changes.h"
#include "activity.h"
#include "selection.h"
#include "shapename.h"
#include "treeholder.h"
#include "menuinfo.h"
#include "repository.h"
#include "application.h"

#include <QtGui/QImage>
#include <cmath>
#include <QFont>
#include <iostream>
#include <QVariant>
#include <QtWebKit>
#include <sys/time.h>
#include <sys/stat.h>


TAO_BEGIN

// ============================================================================
//
//   Widget life management
//
// ============================================================================

Widget::Widget(Window *parent, XL::SourceFile *sf)
// ----------------------------------------------------------------------------
//    Create the GL widget
// ----------------------------------------------------------------------------
    : QGLWidget(QGLFormat(QGL::SampleBuffers|QGL::AlphaChannel), parent),
      xlProgram(sf), timer(this), idleTimer(this), currentMenu(NULL),
      frame(NULL), mainFrame(NULL), activities(NULL),
      tmin(~0ULL), tmax(0), tsum(0), tcount(0),
      nextSave(now()), nextCommit(nextSave), nextSync(nextSave),
      page_start_time(CurrentTime()), event(NULL), focusWidget(NULL),
      whatsNew("")
{
    // Make sure we don't fill background with crap
    setAutoFillBackground(false);

    // Make this the current context for OpenGL
    makeCurrent();
    mainFrame = new Frame;

    // Prepare the timers
    connect(&timer, SIGNAL(timeout()), this, SLOT(updateGL()));
    connect(&idleTimer, SIGNAL(timeout()), this, SLOT(dawdle()));
    idleTimer.start(0);

    // Configure the proxies for URLs
    QNetworkProxyFactory::setUseSystemConfiguration(true);

    // Receive notifications for focus
    connect(qApp, SIGNAL(focusChanged (QWidget *, QWidget *)),
            this,  SLOT(appFocusChanged(QWidget *, QWidget *)));
    setFocusPolicy(Qt::StrongFocus);

    // Prepare the menubar
    currentMenuBar = parent->menuBar();
    connect(parent->menuBar(),  SIGNAL(triggered(QAction*)),
            this,               SLOT(userMenu(QAction*)));
}


Widget::~Widget()
// ----------------------------------------------------------------------------
//   Destroy the widget
// ----------------------------------------------------------------------------
{
    delete mainFrame;
}



// ============================================================================
//
//   Widget basic events (painting, mause, ...)
//
// ============================================================================

void Widget::initializeGL()
// ----------------------------------------------------------------------------
//    Called once per rendering to setup the GL environment
// ----------------------------------------------------------------------------
{
}


void Widget::resizeGL(int width, int height)
// ----------------------------------------------------------------------------
//   Called when the size changes
// ----------------------------------------------------------------------------
{
    mainFrame->Resize(width, height);
    tmax = tsum = tcount = 0;
    tmin = ~tmax;
}


void Widget::paintGL()
// ----------------------------------------------------------------------------
//    Repaint the contents of the window
// ----------------------------------------------------------------------------
{
    draw();
    glShowErrors();
}


static void printWidget(QWidget *w)
// ----------------------------------------------------------------------------
//   Print a widget for debugging purpose
// ----------------------------------------------------------------------------
{
    printf("%p", w);
    if (w)
        printf(" (%s)", w->metaObject()->className());
}



void Widget::appFocusChanged(QWidget *prev, QWidget *next)
// ----------------------------------------------------------------------------
//   Notifications when focus changes
// ----------------------------------------------------------------------------
{
#if 0
    printf("Focus "); printWidget(prev); printf ("->"); printWidget(next);
    const QObjectList &children = this->children();
    QObjectList::const_iterator it;
    printf("\nChildren:");
    for (it = children.begin(); it != children.end(); it++)
    {
        printf(" ");
        printWidget((QWidget *) *it);
    }
    printf("\n");
#endif
}


void Widget::updateProgram(XL::SourceFile *source)
// ----------------------------------------------------------------------------
//   Change the XL program, clean up stuff along the way
// ----------------------------------------------------------------------------
{
    xlProgram = source;
    refreshProgram();
}


void Widget::refreshProgram()
// ----------------------------------------------------------------------------
//   Check if any of the source files we depend on changed
// ----------------------------------------------------------------------------
{
    if (!xlProgram)
        return;

    Repository *repository = TaoApp->Library();
    Tree *prog = xlProgram->tree.tree;

    // Loop on imported files
    import_set iset;
    if (ImportedFilesChanged(prog, iset, false))
    {
        import_set::iterator it;
        bool needBigHammer = false;
        for (it = iset.begin(); it != iset.end(); it++)
        {
            XL::SourceFile &sf = **it;
            text fname = sf.name;
            struct stat st;
            stat (fname.c_str(), &st);

            if (st.st_mtime > sf.modified)
            {
                IFTRACE(filesync)
                    std::cerr << "File " << fname << " changed\n";

                Tree *replacement = NULL;
                if (repository)
                {
                    replacement = repository->read(fname);
                }
                else
                {
                    XL::Syntax syntax(XL::MAIN->syntax);
                    XL::Positions &positions = XL::MAIN->positions;
                    XL::Errors &errors = XL::MAIN->errors;
                    XL::Parser parser(fname.c_str(), syntax, positions, errors);
                    XL::Tree *replacement = parser.Parse();
                }

                if (!replacement)
                {
                    // Uh oh, file went away?
                    needBigHammer = true;
                }
                else
                {
                    // Check if we can simply change some parameters in file
                    ApplyChanges changes(replacement);
                    if (!sf.tree.tree->Do(changes))
                        needBigHammer = true;

                    // Record new modification time
                    sf.modified = st.st_mtime;

                    IFTRACE(filesync)
                    {
                        if (needBigHammer)
                            std::cerr << "Need to reload everything.\n";
                        else
                            std::cerr << "Surgical replacement worked\n";
                    }
                } // Replacement checked
            } // If file modified
        } // For all files

        // If we were not successful with simple changes, reload everything...
        if (needBigHammer)
        {
            for (it = iset.begin(); it != iset.end(); it++)
            {
                XL::SourceFile &sf = **it;
                text fname = sf.name;
                XL::MAIN->LoadFile(fname);
            }
        }
    }

    // Perform a good old garbage collection to clean things up
    XL::Context::context->CollectGarbage();
}


void Widget::markChanged(text reason)
// ----------------------------------------------------------------------------
//    Record that the program changed
// ----------------------------------------------------------------------------
{
    if (whatsNew.find(reason) == whatsNew.npos)
    {
        if (whatsNew.length())
            whatsNew += "\n";
        whatsNew += reason;
    }
    if (xlProgram)
    {
        if (Tree *prog = xlProgram->tree.tree)
        {
            import_set done;
            ImportedFilesChanged(prog, done, true);
        }
    }
}


void Widget::requestFocus(QWidget *widget)
// ----------------------------------------------------------------------------
//   Some other widget request the focus
// ----------------------------------------------------------------------------
{
    if (!focusWidget)
    {
        focusWidget = widget;
        glGetDoublev(GL_PROJECTION_MATRIX, focusProjection);
        glGetDoublev(GL_MODELVIEW_MATRIX, focusModel);
        glGetIntegerv(GL_VIEWPORT, focusViewport);

        QFocusEvent focusIn(QEvent::FocusIn, Qt::ActiveWindowFocusReason);
        QObject *fin = focusWidget;
        fin->event(&focusIn);
    }
}


Point3 Widget::unproject (coord x, coord y, coord z)
// ----------------------------------------------------------------------------
//   Convert mouse clicks into 3D planar coordinates for the focus object
// ----------------------------------------------------------------------------
{
    // Get 3D coordinates for the near plane based on window coordinates
    GLdouble x3dn, y3dn, z3dn;
    gluUnProject(x, y, 0.0,
                 focusModel, focusProjection, focusViewport,
                 &x3dn, &y3dn, &z3dn);

    // Same with far-plane 3D coordinates
    GLdouble x3df, y3df, z3df;
    gluUnProject(x, y, 1.0,
                 focusModel, focusProjection, focusViewport,
                 &x3df, &y3df, &z3df);

    GLfloat zDistance = z3dn - z3df;
    if (zDistance == 0.0)
        zDistance = 1.0;
    GLfloat ratio = (z3dn - z) / zDistance;
    GLfloat x3d = x3dn + ratio * (x3df - x3dn);
    GLfloat y3d = y3dn + ratio * (y3df - y3dn);

    return Point3(x3d, y3d, z);
}


void Widget::setup(double w, double h, Box *picking)
// ----------------------------------------------------------------------------
//   Setup an initial environment for drawing
// ----------------------------------------------------------------------------
{
    // Setup viewport
    glViewport(0, 0, w, h);

    // Setup the projection matrix
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    // Restrict the picking area if any is given as input
    if (picking)
    {
        GLint viewport[4] = { 0, 0, w, h };
        Box b = *picking;
        b.Normalize();
        Vector size = b.upper - b.lower;
        Point center = b.lower + size / 2;
        gluPickMatrix(center.x, center.y, size.x+1, size.y+1, viewport);
    }

    // Setup the frustrum for the projection
    double zNear = 1000.0, zFar = 40000.0;
    double eyeX = 0.0, eyeY = 0.0, eyeZ = 1000.0;
    double centerX = 0.0, centerY = 0.0, centerZ = 0.0;
    double upX = 0.0, upY = 1.0, upZ = 0.0;
    glFrustum (-w/2, w/2, -h/2, h/2, zNear, zFar);
    gluLookAt(eyeX, eyeY, eyeZ, centerX, centerY, centerZ, upX, upY, upZ);
    glTranslatef(0.0, 0.0, -zNear);
    glScalef(2.0, 2.0, 2.0);

    // Setup the model view matrix so that 1.0 unit = 1px
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // Reset default GL parameters
    setupGL();

    // Initial state
    state.polygonMode = GL_POLYGON;
    state.frameWidth = w;
    state.frameHeight = h;
    state.charFormat = QTextCharFormat();
    state.charFormat.setForeground(Qt::black);
    state.charFormat.setBackground(Qt::white);
    state.selectable = true;
}


void Widget::setupGL()
// ----------------------------------------------------------------------------
//   Setup default GL parameters
// ----------------------------------------------------------------------------
{
    // Setup other
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_DEPTH_TEST);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_TEXTURE_RECTANGLE_ARB);
    glDisable(GL_CULL_FACE);
}


void Widget::dawdle()
// ----------------------------------------------------------------------------
//   Operations to do when idle (in the background)
// ----------------------------------------------------------------------------
{
    // Run all activities, which will get them a chance to update refresh
    for (Activity *a = activities; a; a = a->next)
        if (a->Idle())
            break;

    // We will only auto-save and commit if we have a valid repository
    Repository *repository     = TaoApp->Library();
    XL::Main   *xlr            = XL::MAIN;
    bool        savedSomething = false;

    // Check if there's something to save
    ulonglong tick = now();
    longlong saveDelay = longlong(nextSave - tick);
    if (repository && saveDelay < 0)
    {
        XL::source_files::iterator it;
        for (it = xlr->files.begin(); it != xlr->files.end(); it++)
        {
            XL::SourceFile &sf = (*it).second;
            text fname = sf.name;
            if (sf.changed)
            {
                if (repository->write(fname, sf.tree.tree))
                {
                    // Mark the tree as no longer changed
                    sf.changed = false;

                    // Record that we need to commit it sometime soon
                    repository->change(fname);
                    IFTRACE(filesync)
                        std::cerr << "Changedfile " << fname << "\n";
                        
                    // Record time when file was changed
                    struct stat st;
                    stat (fname.c_str(), &st);
                    sf.modified = st.st_mtime;
                }
            }
        }

        // Record when we will save file again
        nextSave = tick + xlr->options.save_interval * 1000;
    }

    // Check if there's something to commit
    longlong commitDelay = longlong (nextCommit - tick);
    if (savedSomething && commitDelay < 0)
    {
        // If we saved anything, then commit changes
        IFTRACE(filesync)
            std::cerr << "Commit: " << whatsNew << "\n";
        if (repository->commit(whatsNew))
        {            
            whatsNew = "";            
            nextCommit = tick + xlr->options.commit_interval * 1000;
            savedSomething = false;
        }
    }

    // Check if there's something to reload
    longlong syncDelay = longlong(nextSync - tick);
    if (syncDelay < 0)
    {
        refreshProgram();
        syncDelay = tick + xlr->options.sync_interval * 1000;
    }
}


void Widget::draw()
// ----------------------------------------------------------------------------
//    Redraw the widget
// ----------------------------------------------------------------------------
{
    // Timing
    ulonglong t = now();
    event = NULL;

    // Setup the initial drawing environment
    double w = width(), h = height();
    setup(w, h);

    // Clear the background
    glClearColor (1.0, 1.0, 1.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // If there is a program, we need to run it
    runProgram();

    // Timing
    elapsed(t);

    // Render all activities, e.g. the selection rectangle
    glDisable(GL_DEPTH_TEST);
    for (Activity *a = activities; a; a = a->next)
        a->Display();
}


void Widget::runProgram()
// ----------------------------------------------------------------------------
//   Run the current XL program
// ----------------------------------------------------------------------------
{
    double w = width(), h = height();

    // Reset the selection id for the various elements being drawn
    id = 0;    
    focusWidget = NULL;
    state.paintDevice = this;

    // Run the XL program associated with this widget
    current = this;
    QTextOption alignCenter(Qt::AlignCenter);
    TextFlow mainFlow(alignCenter);
    XL::LocalSave<TextFlow *> saveFlow(state.flow, &mainFlow);
    XL::LocalSave<Frame *> saveFrame (frame, mainFrame);

    try
    {
        if (xlProgram && xlProgram->tree.tree)
            xl_evaluate(xlProgram->tree.tree);
    }
    catch (XL::Error &e)
    {
        xlProgram = NULL;
        QMessageBox::warning(this, tr("Runtime error"),
                             tr("Error executing the program:\n%1")
                             .arg(QString::fromStdString(e.Message())));
    }
    catch(...)
    {
        xlProgram = NULL;
        QMessageBox::warning(this, tr("Runtime error"),
                             tr("Unknown error executing the program"));
    }

    // After we are done, flush the frame and over-paint it
    mainFrame->Paint(-w/2, -h/2, w, h);

    // Once we are done, do a garbage collection
    XL::Context::context->CollectGarbage();

    // Remember how many elements are drawn on the page, plus arbitrary buffer
    if (id > capacity)
        capacity = id + 100;
}


ulonglong Widget::now()
// ----------------------------------------------------------------------------
//    Return the current time in microseconds
// ----------------------------------------------------------------------------
{
    // Timing
    struct timeval tv;
    gettimeofday(&tv, NULL);
    ulonglong t = tv.tv_sec * 1000000ULL + tv.tv_usec;
    return t;
}


ulonglong Widget::elapsed(ulonglong since, bool stats, bool show)
// ----------------------------------------------------------------------------
//    Record how much time passed since last measurement
// ----------------------------------------------------------------------------
{
    ulonglong t = now() - since;
    if (t == 0)
        t = 1; // Because windows lies

    if (stats)
    {
        if (tmin > t) tmin = t;
        if (tmax < t) tmax = t;
        tsum += t;
        tcount++;
    }

    if (show)
    {
        char buffer[80];
        snprintf(buffer, sizeof(buffer),
                 "Duration=%llu-%llu (~%f) %5.2f-%5.2f FPS (~%5.2f)",
                 tmin, tmax, double(tsum )/ tcount,
                 (100000000ULL / tmax)*0.01,
                 (100000000ULL / tmin)*0.01,
                 (100000000ULL * tcount / tsum) * 0.01);
        Window *window = (Window *) parentWidget();
        window->statusBar()->showMessage(QString(buffer));
    }

    return t;
}


bool Widget::forwardEvent(QEvent *event)
// ----------------------------------------------------------------------------
//   Forward event to the focus proxy if there is any
// ----------------------------------------------------------------------------
{
    if (QObject *focus = focusWidget)
        return focus->event(event);
    return false;
}

void Widget::userMenu(QAction *p_action)
// ----------------------------------------------------------------------------
//   user menu slot activation
// ----------------------------------------------------------------------------
{
    if (!p_action)
        return;

    IFTRACE(menus)
    {
        std::cout << "Action " << p_action->objectName()
                << " (" << p_action->text() << ") activated\n";
    }
    QVariant var =  p_action->data();
    if (!var.isValid())
        return;

    TreeHolder t = var.value<TreeHolder >();
    xlProgram->tree.tree = new XL::Infix("\n", xlProgram->tree.tree, t.tree);

    markChanged("Menu clicked, added program element");
}


bool Widget::forwardEvent(QMouseEvent *event)
// ----------------------------------------------------------------------------
//   Forward event to the focus proxy if there is any, adjusting coordinates
// ----------------------------------------------------------------------------
{
    if (QObject *focus = focusWidget)
    {
        int x = event->x();
        int y = event->y();
        int w = focusWidget->width();
        int h = focusWidget->height();
        int hh = height();

        Point3 u = unproject(x, hh-y, 0);
        Point3 v = unproject(x, y, 0);
        QMouseEvent local(event->type(), QPoint(u.x + w/2, h/2 - u.y),
                          event->button(), event->buttons(),
                          event->modifiers());
        return focus->event(&local);
    }
    return false;
}


void Widget::keyPressEvent(QKeyEvent *event)
// ----------------------------------------------------------------------------
//   A key is pressed
// ----------------------------------------------------------------------------
{
    EventSave save(this->event, event);
    
    // Check if there is an activity that deals with it
    uint key = (uint) event->key();
    bool found = false;
    for (Activity *a = activities; !found && a; a = a->next)
        found = a->Key(key, true);

    if (!found)
        forwardEvent(event);
}


void Widget::keyReleaseEvent(QKeyEvent *event)
// ----------------------------------------------------------------------------
//   A key is released
// ----------------------------------------------------------------------------
{
    EventSave save(this->event, event);

    // Check if there is an activity that deals with it
    uint key = (uint) event->key();
    bool found = false;
    for (Activity *a = activities; !found && a; a = a->next)
        found = a->Key(key, false);

    if (!found)
        forwardEvent(event);
}

void Widget::mousePressEvent(QMouseEvent *event)
// ----------------------------------------------------------------------------
//   Mouse button click
// ----------------------------------------------------------------------------
{
    if (event->button() ==  Qt::RightButton)
    {
    
        QMenu * contextMenu = NULL;
        switch (event->modifiers())
        {
        case Qt::NoModifier :
            {
                contextMenu = parent()->findChild<QMenu*>(CONTEXT_MENU);
                break;
            }
        case Qt::ShiftModifier :
            {
                contextMenu = parent()->findChild<QMenu*>(SHIFT_CONTEXT_MENU);
                break;
            }
        case Qt::ControlModifier :
            {
                contextMenu = parent()->findChild<QMenu*>(CONTROL_CONTEXT_MENU);
                break;
            }
        case Qt::AltModifier :
            {
                contextMenu = parent()->findChild<QMenu*>(ALT_CONTEXT_MENU);
                break;
            }
        case Qt::MetaModifier :
            {
                contextMenu = parent()->findChild<QMenu*>(META_CONTEXT_MENU);
                break;
            }
        default : return;
        }

        if (!contextMenu) return;

        contextMenu->exec(event->globalPos());
        return;
    } 
    else 
    {
        EventSave save(this->event, event);

        uint button = (uint) event->button();
        int x = event->x();
        int y = event->y();

        // Check if there is an activity that deals with it
        bool found = false;
        for (Activity *a = activities; !found && a; a = a->next)
            found = a->Click(button, true, x, y);

        if (!found)
        {
            Selection *s = new Selection(this);
            s->Click(button, true, x, y);
            forwardEvent(event);
        }
    }
}


void Widget::mouseReleaseEvent(QMouseEvent *event)
// ----------------------------------------------------------------------------
//   Mouse button is released
// ----------------------------------------------------------------------------
{
    EventSave save(this->event, event);

    uint button = (uint) event->button();
    int x = event->x();
    int y = event->y();

    // Check if there is an activity that deals with it
    bool found = false;
    for (Activity *a = activities; !found && a; a = a->next)
        found = a->Click(button, false, x, y);

    if (!found)
        forwardEvent(event);
}


void Widget::mouseMoveEvent(QMouseEvent *event)
// ----------------------------------------------------------------------------
//    Mouse move
// ----------------------------------------------------------------------------
{
    EventSave save(this->event, event);
    bool active = event->buttons() != Qt::NoButton;
    int x = event->x();
    int y = event->y();

    // Check if there is an activity that deals with it
    bool found = false;
    for (Activity *a = activities; !found && a; a = a->next)
        found = a->MouseMove(x, y, active);

    if (!found)
        forwardEvent(event);
}


void Widget::mouseDoubleClickEvent(QMouseEvent *event)
// ----------------------------------------------------------------------------
//   Mouse double click
// ----------------------------------------------------------------------------
{
    EventSave save(this->event, event);
    forwardEvent(event);
}


void Widget::wheelEvent(QWheelEvent *event)
// ----------------------------------------------------------------------------
//   Mouse wheel
// ----------------------------------------------------------------------------
{
    EventSave save(this->event, event);
    forwardEvent(event);
}


void Widget::timerEvent(QTimerEvent *event)
// ----------------------------------------------------------------------------
//    Timer expired
// ----------------------------------------------------------------------------
{
    EventSave save(this->event, event);
    forwardEvent(event);
}



// ============================================================================
//
//   XLR runtime entry points
//
// ============================================================================

Widget *Widget::current = NULL;

typedef XL::Tree Tree;


Tree *Widget::status(Tree *self, text caption)
// ----------------------------------------------------------------------------
//   Set the status line of the window
// ----------------------------------------------------------------------------
{
    Window *window = (Window *) parentWidget();
    window->statusBar()->showMessage(QString::fromStdString(caption));
    return XL::xl_true;
}


Tree *Widget::rotate(Tree *self, double ra, double rx, double ry, double rz)
// ----------------------------------------------------------------------------
//    Rotation along an arbitrary axis
// ----------------------------------------------------------------------------
{
    glRotatef(ra, rx, ry, rz);
    return XL::xl_true;
}


Tree *Widget::translate(Tree *self, double rx, double ry, double rz)
// ----------------------------------------------------------------------------
//     Translation along three axes
// ----------------------------------------------------------------------------
{
    glTranslatef(rx, ry, rz);
    return XL::xl_true;
}


Tree *Widget::scale(Tree *self, double sx, double sy, double sz)
// ----------------------------------------------------------------------------
//     Scaling along three axes
// ----------------------------------------------------------------------------
{
    glScalef(sx, sy, sz);
    return XL::xl_true;
}


Tree *Widget::refresh(Tree *self, double delay)
// ----------------------------------------------------------------------------
//    Refresh after the given number of seconds
// ----------------------------------------------------------------------------
{
    timer.setSingleShot(true);
    timer.start(1000 * delay);
    return XL::xl_true;
}


Tree *Widget::locally(Tree *self, Tree *child)
// ----------------------------------------------------------------------------
//   Evaluate the child tree while preserving the OpenGL context
// ----------------------------------------------------------------------------
{
    GLAndWidgetKeeper save(this);
    Tree *result = xl_evaluate(child);
    return result;
}


Tree *Widget::pagesize(Tree *self, uint w, uint h)
// ----------------------------------------------------------------------------
//    Set the bit size for the page textures
// ----------------------------------------------------------------------------
{
    // Little practical point in ever creating textures bigger than viewport
    if (w > width())    w = width();
    if (h > height())   h = height();
    state.frameWidth = w;
    state.frameHeight = h;
    return XL::xl_true;
}


Tree *Widget::page(Tree *self, Tree *p)
// ----------------------------------------------------------------------------
//  Evaluate the tree in a frame with the given size
// ----------------------------------------------------------------------------
{
    uint w = state.frameWidth, h = state.frameHeight;
    Frame *cairo = self->GetInfo<Frame>();
    FrameInfo *frame = self->GetInfo<FrameInfo>();
    if (!frame)
    {
        frame = new FrameInfo(w,h);
        self->SetInfo<FrameInfo> (frame);
    }

    Tree *result = NULL;

    // Do to a bug in the NVIDIA kernel driver on MacOSX, we need to avoid the
    // following code when in GL_SELECT mode or die.
    if (!event)
    {
        glPushAttrib(GL_ALL_ATTRIB_BITS);
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();

        frame->resize(w,h);

        frame->begin();
        {
            // Clear the background and setup initial state
            setup(w, h);

            if (!cairo)
            {
                cairo = new Frame;
                self->SetInfo<Frame>(cairo);
            }

            XL::LocalSave<QPaintDevice *> sv(state.paintDevice,
                                             frame->render_fbo);
            XL::LocalSave<Frame *> svc(this->frame, cairo);
            result = xl_evaluate(p);
        }
        cairo->Paint(-w/2, -h/2, w, h);
        frame->end();

        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();
        glPopAttrib();

        frame->bind();
    }

    return result;
}


XL::Integer *Widget::page_width(Tree *self)
// ----------------------------------------------------------------------------
//   Return the width of the page
// ----------------------------------------------------------------------------
{
    return new Integer(width());
}


XL::Integer *Widget::page_height(Tree *self)
// ----------------------------------------------------------------------------
//   Return the height of the page
// ----------------------------------------------------------------------------
{
    return new Integer(height());
}


Tree *Widget::time(Tree *self)
// ----------------------------------------------------------------------------
//   Return a fractional time, including milliseconds
// ----------------------------------------------------------------------------
{
    return new XL::Real(CurrentTime());
}


Tree *Widget::page_time(Tree *self)
// ----------------------------------------------------------------------------
//   Return a fractional time, including milliseconds
// ----------------------------------------------------------------------------
{
    return new XL::Real(CurrentTime() - page_start_time);
}


XL::Name *Widget::selectable(Tree *self, bool new_selectable)
// ----------------------------------------------------------------------------
//   Return current selectable state, and set new one
// ----------------------------------------------------------------------------
{
    Name *result = state.selectable ? XL::xl_true : XL::xl_false;
    state.selectable = new_selectable;
    return result;
}


GLuint Widget::shapeId()
// ----------------------------------------------------------------------------
//   Return an identifier for the shape in selections
// ----------------------------------------------------------------------------
//   We assign an identifier only if we are selectable and if we are not
//   rendering in an off-screen buffer of some sort
{
    return state.selectable && state.paintDevice == this
        ? ++id
        : 0;
}


void Widget::select()
// ----------------------------------------------------------------------------
//    Select the current shape if we are in selectable state
// ----------------------------------------------------------------------------
{
    if (state.selectable && state.paintDevice == this)
        selection.insert(id);
}


bool Widget::selected()
// ----------------------------------------------------------------------------
//   Test if the current shape is selected
// ----------------------------------------------------------------------------
{
    return state.selectable && state.paintDevice == this
        ? selection.count(id) > 0
        : false;
}


void Widget::drawSelection(const Box3 &bounds)
// ----------------------------------------------------------------------------
//    Draw a 3D selection with the given coordinates
// ----------------------------------------------------------------------------
{
    GLAttribKeeper save(GL_TEXTURE_BIT | GL_CURRENT_BIT | GL_ENABLE_BIT);

    Point3 c = bounds.Center();
    coord xc = c.x;
    coord yc = c.y;
    coord zc = c.z;

    coord w = bounds.Width(); 
    coord h = bounds.Height(); 
    coord d = bounds.Depth();

    // Compute the box around the item
    coord r = w;
    if (r > h) r = h;
    if (r > d) r = d;
    r /= 4;
    if (r < 15.0)
        r = 15.0;

    coord xl = bounds.lower.x - r;
    coord xu = bounds.upper.x + r;
    coord yl = bounds.lower.y - r;
    coord yu = bounds.upper.y + r;
    coord zl = bounds.lower.z - r;
    coord zu = bounds.upper.z + r;

    setupGL();
    glDisable(GL_DEPTH_TEST);

    glBegin(GL_TRIANGLE_FAN);
    glColor4f(1.0, 1.0, 1.0, 0.1);    glVertex3f(xc, yc, zu);
    glColor4f(1.0, 0.0, 0.0, 0.4);    glVertex3f(xl, yl, zc);
    glColor4f(0.0, 0.0, 1.0, 0.4);    glVertex3f(xl, yu, zc);
    glColor4f(0.0, 1.0, 1.0, 0.4);    glVertex3f(xu, yu, zc);
    glColor4f(1.0, 1.0, 0.0, 0.4);    glVertex3f(xu, yl, zc);
    glColor4f(1.0, 0.0, 0.0, 0.4);    glVertex3f(xl, yl, zc);
    glEnd();

    glBegin(GL_TRIANGLE_FAN);
    glColor4f(0.0, 0.0, 0.0, 0.4);    glVertex3f(xc, yc, zl);
    glColor4f(1.0, 0.0, 0.0, 0.4);    glVertex3f(xl, yl, zc);
    glColor4f(0.0, 0.0, 1.0, 0.4);    glVertex3f(xl, yu, zc);
    glColor4f(0.0, 1.0, 1.0, 0.4);    glVertex3f(xu, yu, zc);
    glColor4f(1.0, 1.0, 0.0, 0.4);    glVertex3f(xu, yl, zc);
    glColor4f(1.0, 0.0, 0.0, 0.4);    glVertex3f(xl, yl, zc);
    glEnd();
}


void Widget::drawSelection(const Box &bounds)
// ----------------------------------------------------------------------------
//    Draw a 2D selection with the given box
// ----------------------------------------------------------------------------
{
    GLAttribKeeper save(GL_CURRENT_BIT | GL_LINE_BIT);
    glLineWidth (3.0);
    glColor4f(1.0, 0.0, 0.0, 0.5);
    glBegin(GL_LINE_LOOP);
    {
        glVertex2f(bounds.lower.x, bounds.lower.y);
        glVertex2f(bounds.lower.x, bounds.upper.y);
        glVertex2f(bounds.upper.x, bounds.upper.y);
        glVertex2f(bounds.upper.x, bounds.lower.y);
    }
    glEnd();
}


void Widget::loadName(bool load)
// ----------------------------------------------------------------------------
//   Load a name on the GL stack
// ----------------------------------------------------------------------------
{
    if (state.selectable && load)
        glLoadName(shapeId());
    else
        glLoadName(0);
}


Tree *Widget::filled(Tree *self)
// ----------------------------------------------------------------------------
//   Select filled polygon mode
// ----------------------------------------------------------------------------
{
    state.polygonMode = GL_POLYGON;
    return XL::xl_true;
}


Tree *Widget::hollow(Tree *self)
// ----------------------------------------------------------------------------
//   Select hollow polygon mode
// ----------------------------------------------------------------------------
{
    state.polygonMode = GL_LINE_LOOP;
    return XL::xl_true;
}


Tree *Widget::linewidth(Tree *self, double lw)
// ----------------------------------------------------------------------------
//    Select the line width for OpenGL
// ----------------------------------------------------------------------------
{
    glLineWidth(lw);
    return XL::xl_true;
}


Tree *Widget::color(Tree *self, double r, double g, double b, double a)
// ----------------------------------------------------------------------------
//    Set the RGBA color
// ----------------------------------------------------------------------------
{
    glColor4f(r,g,b,a);
    frame->Color(r,g,b,a);
    return XL::xl_true;
}


Tree *Widget::textColor(Tree *self,
                        double r, double g, double b, double a,
                        bool isFg)
// ----------------------------------------------------------------------------
//    Set the RGBA color
// ----------------------------------------------------------------------------
{
      // Set color for text layout
    const double amp=255.9;
    QColor qcolor(floor(amp*r),floor(amp*g),floor(amp*b),floor(amp*a));

    if (isFg)
    {
        state.charFormat.setForeground(qcolor);
    } else {
        state.charFormat.setBackground(qcolor);
    }

    // For Cairo
    GLStateKeeper save;
    frame->Color(r,g,b,a);

    return XL::xl_true;
}


Tree *Widget::polygon(Tree *self, Tree *child)
// ----------------------------------------------------------------------------
//   Evaluate the child tree within a polygon
// ----------------------------------------------------------------------------
{
    GLAndWidgetKeeper save(this);
    glBegin(state.polygonMode);
    xl_evaluate(child);
    glEnd();
    return XL::xl_true;
}


Tree *Widget::vertex(Tree *self, double x, double y, double z)
// ----------------------------------------------------------------------------
//     GL vertex
// ----------------------------------------------------------------------------
{
    glVertex3f(x, y, z);
    return XL::xl_true;
}



// ============================================================================
//
//    Texture management
//
// ============================================================================

Tree *Widget::texture(Tree *self, text img)
// ----------------------------------------------------------------------------
//     Build a GL texture out of an image file
// ----------------------------------------------------------------------------
{
    if (img == "")
    {
        glDisable(GL_TEXTURE_2D);
        return XL::xl_true;
    }

    ImageTextureInfo *rinfo = self->GetInfo<ImageTextureInfo>();
    if (!rinfo)
    {
        rinfo = new ImageTextureInfo(this);
        self->SetInfo<ImageTextureInfo>(rinfo);
    }
    rinfo->bind(img);
    return XL::xl_true;
}


Tree *Widget::svg(Tree *self, text img)
// ----------------------------------------------------------------------------
//    Draw an image in SVG format
// ----------------------------------------------------------------------------
//    The image may be animated, in which case we will get repaintNeeded()
//    signals that we send to our 'draw()' so that we redraw as needed.
{
    SvgRendererInfo *rinfo = self->GetInfo<SvgRendererInfo>();
    if (!rinfo)
    {
        rinfo = new SvgRendererInfo(this);
        self->SetInfo<SvgRendererInfo>(rinfo);
    }
    rinfo->bind(img);
    return XL::xl_true;
}


Tree *Widget::texCoord(Tree *self, double x, double y)
// ----------------------------------------------------------------------------
//     GL texture coordinate
// ----------------------------------------------------------------------------
{
    glTexCoord2f(x, y);
    return XL::xl_true;
}


Tree *Widget::sphere(Tree *self,
                     double x, double y, double z,
                     double r, int nslices, int nstacks)
// ----------------------------------------------------------------------------
//     GL sphere
// ----------------------------------------------------------------------------
{
    Box3 bounds(x-r, y-r, z-r, 2*r, 2*r, 2*r);
    ShapeSelection name(this, bounds);

    GLUquadric *q = gluNewQuadric();
    gluQuadricTexture (q, true);
    glPushMatrix();
    glTranslatef(x,y,z);
    glRotatef(-90.0, 1.0, 0.0, 0.0);
    gluSphere(q, r, nslices, nstacks);
    glPopMatrix();
    gluDeleteQuadric(q);
    return XL::xl_true;
}


void Widget::widgetVertex(double x, double y, double tx, double ty)
// ----------------------------------------------------------------------------
//   A vertex, including texture coordinate
// ----------------------------------------------------------------------------
{
    texCoord(NULL, tx, ty);
    vertex(NULL, x, y, 0.0);
}


void Widget::circularVertex(double cx, double cy, double r,
                            double x, double y,
                            double tx0, double ty0, double tx1, double ty1)
// ----------------------------------------------------------------------------
//   A circular vertex, including texture coordinate
// ----------------------------------------------------------------------------
//   x range between -1 and 1, y between -1 and 1
//   cx and cy are the center of the circle, r its radius
{
    widgetVertex(cx + r * x,
                 cy + r * y,
                 tx0 + ((x + 1.0) / 2.0 * (tx1 - tx0)),
                 ty0 + ((y + 1.0) / 2.0 * (ty1 - ty0)));
}


void Widget::circularSectorN(double cx, double cy, double r,
                            double tx0, double ty0, double tx1, double ty1,
                            int sq, int nq)
// ----------------------------------------------------------------------------
//     Draw a circular sector of N/4th of a circle
// ----------------------------------------------------------------------------
//   We use a reduced Bresenham-like algorithm for circles (midpoint circle)
//
//   For now the sector is limited to multiples of 1/4th of circle. For
//   example, an angle of 280 will draw 3/4 of a circle.
{
    // The two first values configure how precise the circle is
    int step = 10;                // Triangles generated every <step> points
    double grid = 1.0 / 500.0;    // Tolerance for points on the circle
    double error, x, y, s;


    for (int q = 0; q < nq; q++)
    {
    error = -1.0;
    x = 1.0;
    y = 0;
    s = step;

    while (x > 0)
    {
        if (++s >= step)
        {
        s = 0;
        switch ((sq + q) % 4)
        {
            case 3:
                circularVertex(cx, cy, r,  y, -x, tx0, ty0, tx1, ty1);
                break;
            case 2:
                circularVertex(cx, cy, r, -x, -y, tx0, ty0, tx1, ty1);
                break;
            case 1:
                circularVertex(cx, cy, r, -y,  x, tx0, ty0, tx1, ty1);
                break;
            case 0:
                circularVertex(cx, cy, r,  x,  y, tx0, ty0, tx1, ty1);
                break;
        }
        }
        if (error >= 0)
        {
            x -= grid;
            error -= x + x;
        }
        else
        {
            y += grid;
            error += y + y;
        }
    }
    }
    switch ((sq + nq) % 4)
    {
        case 3:
            circularVertex(cx, cy, r,  0, -1, tx0, ty0, tx1, ty1);
            break;
        case 2:
            circularVertex(cx, cy, r, -1,  0, tx0, ty0, tx1, ty1);
            break;
        case 1:
            circularVertex(cx, cy, r,  0,  1, tx0, ty0, tx1, ty1);
            break;
        case 0:
            circularVertex(cx, cy, r,  1,  0, tx0, ty0, tx1, ty1);
            break;
    }
}


Tree *Widget::circle(Tree *self, double cx, double cy, double r)
// ----------------------------------------------------------------------------
//     GL circle centered around (cx,cy), radius r
// ----------------------------------------------------------------------------
{
    ShapeSelection name(this, cx-r, cy-r, 2*r, 2*r);

    glBegin(state.polygonMode);
    circularSectorN(cx, cy, r, 0, 0, 1, 1, 0, 4);
    glEnd();

    return XL::xl_true;
}


Tree *Widget::circularSector(Tree *self,
                         double cx, double cy, double r,
                         double a, double b)
// ----------------------------------------------------------------------------
//     GL circular sector centered around (cx,cy), radius r and two angles a, b
// ----------------------------------------------------------------------------
{
    ShapeSelection name(this, cx-r, cy-r, 2*r, 2*r);

    while (b < a)
    {
        b += 360;
    }
    int nq = int((b-a) / 90);                   // Number of quadrants to draw
    if (nq > 4)
    {
        nq = 4;
    }

    while (a < 0)
    {
        a += 360;
    }
    int sq = (int(a / 90) % 4);                 // Starting quadrant

    glBegin(state.polygonMode);
    circularVertex(cx, cy, r, 0, 0, 0, 0, 1, 1);    // The center
    circularSectorN(cx, cy, r, 0, 0, 1, 1, sq, nq);
    glEnd();

    return XL::xl_true;
 }



Tree *Widget::roundedRectangle(Tree *self,
                               double cx, double cy,
                               double w, double h, double r)
// ----------------------------------------------------------------------------
//     GL rounded rectangle with radius r for the rounded corners
// ----------------------------------------------------------------------------
{
    ShapeSelection name(this, cx-w/2, cy-h/2, w, h);

    if (r <= 0) return rectangle(self, cx, cy, w, h);
    if (r > w/2) r = w/2;
    if (r > h/2) r = h/2;

    double x0  = cx-w/2;
    double x0r = x0+r;
    double x1  = cx+w/2;
    double x1r = x1-r;

    double y0  = cy-h/2;
    double y0r = y0+r;
    double y1  = cy+h/2;
    double y1r = y1-r;

    double tx0  = 0;
    double tx0r = 0+r/w;
    double tx0d = 0+2*r/w;
    double tx1  = 1;
    double tx1r = 1-r/w;
    double tx1d = 1-2*r/w;

    double ty0  = 0;
    double ty0r = 0+r/h;
    double ty0d = 0+2*r/h;
    double ty1  = 1;
    double ty1r = 1-r/h;
    double ty1d = 1-2*r/h;

    glBegin(state.polygonMode);
    {
        widgetVertex(x1, y1r, tx1, ty1r);

        circularSectorN(x1r, y1r, r, tx1d, ty1d, tx1, ty1, 0, 1);

        widgetVertex(x1r, y1, tx1r, ty1);
        widgetVertex(x0r, y1, tx0r, ty1);

        circularSectorN(x0r, y1r, r, tx0, ty1d, tx0d, ty1, 1, 1);

        widgetVertex(x0, y1r, tx0, ty1r);
        widgetVertex(x0, y0r, tx0, ty0r);

        circularSectorN(x0r, y0r, r, tx0, ty0, tx0d, ty0d, 2, 1);

        widgetVertex(x0r, y0, tx0r, ty0);
        widgetVertex(x1r, y0, tx1r, ty0);

        circularSectorN(x1r, y0r, r, tx1d, ty0, tx1, ty0d, 3, 1);

        widgetVertex(x1, y0r, tx1, ty0r);
    }
    glEnd();

    return XL::xl_true;
}


Tree *Widget::rectangle(Tree *self, double cx, double cy, double w, double h)
// ----------------------------------------------------------------------------
//     GL rectangle centered around (cx,cy), width w, height h
// ----------------------------------------------------------------------------
{
    ShapeSelection name(this, cx-w/2, cy-h/2, w, h);

    glBegin(state.polygonMode);
    {
        widgetVertex(cx-w/2, cy-h/2, 0, 0);
        widgetVertex(cx+w/2, cy-h/2, 1, 0);
        widgetVertex(cx+w/2, cy+h/2, 1, 1);
        widgetVertex(cx-w/2, cy+h/2, 0, 1);
    }
    glEnd();

    return XL::xl_true;
}


Tree *Widget::regularStarPolygon(Tree *self, double cx, double cy, double r,
                                 int p, int q)
// ----------------------------------------------------------------------------
//     GL regular p-side star polygon {p/q} centered around (cx,cy), radius r
// ----------------------------------------------------------------------------
{
    ShapeSelection name(this, cx-r, cy-r, 2*r, 2*r);

    if (p < 2 || q < 1 || q > (p-1)/2)
        return XL::xl_false;

    double R_r = cos( q*M_PI/p ) / cos( (q-1)*M_PI/p );
    double R = r * R_r;

    GLuint mode = state.polygonMode;
    if (mode == GL_POLYGON)
    {
        mode = GL_TRIANGLE_FAN; // GL_POLYGON does not work here
    }

    glBegin(mode);
    {
        if (mode == GL_TRIANGLE_FAN)
        {
            circularVertex(cx, cy, r, 0, 0, 0, 0, 1, 1);    // The center
        }

        for (int i = 0; i < p; i++)
        {
            circularVertex(cx, cy, r,
                    cos( i * 2*M_PI/p + M_PI_2),
                    sin( i * 2*M_PI/p + M_PI_2),
                    0, 0, 1, 1);

            circularVertex(cx, cy, R,
                    cos( i * 2*M_PI/p + M_PI_2 + M_PI/p),
                    sin( i * 2*M_PI/p + M_PI_2 + M_PI/p),
                    (1-R_r)/2, (1-R_r)/2, (1+R_r)/2, (1+R_r)/2);
        }

        if (mode == GL_TRIANGLE_FAN)
        {
            circularVertex(cx, cy, r, 0, 1, 0, 0, 1, 1);    // Closing the star
        }
    }
    glEnd();

    return XL::xl_true;
}


XL::Real *Widget::fromCm(Tree *self, double cm)
// ----------------------------------------------------------------------------
//   Convert from cm to pixels
// ----------------------------------------------------------------------------
{
    XL_RREAL(cm * logicalDpiX() * (1.0 / 2.54));
}


XL::Real *Widget::fromMm(Tree *self, double mm)
// ----------------------------------------------------------------------------
//   Convert from mm to pixels
// ----------------------------------------------------------------------------
{
    XL_RREAL(mm * logicalDpiX() * (0.1 / 2.54));
}


XL::Real *Widget::fromIn(Tree *self, double in)
// ----------------------------------------------------------------------------
//   Convert from inch to pixels
// ----------------------------------------------------------------------------
{
    XL_RREAL(in * logicalDpiX());
}


XL::Real *Widget::fromPt(Tree *self, double pt)
// ----------------------------------------------------------------------------
//   Convert from pt to pixels
// ----------------------------------------------------------------------------
{
    XL_RREAL(pt * logicalDpiX() * (1.0 / 72.0));
}


XL::Real *Widget::fromPx(Tree *self, double px)
// ----------------------------------------------------------------------------
//   Convert from pixel to pixels (trivial)
// ----------------------------------------------------------------------------
{
    XL_RREAL(px);
}


Tree *Widget::font(Tree *self, text description)
// ----------------------------------------------------------------------------
//   Select a font family
// ----------------------------------------------------------------------------
{
    QFont font = state.charFormat.font();
    font.fromString((QString::fromStdString(description)));
    state.charFormat.setFont(font);
    GLStateKeeper save;
    frame->Font(description);
    return XL::xl_true;
}


Tree *Widget::fontSize(Tree *self, double size)
// ----------------------------------------------------------------------------
//   Select a font size
// ----------------------------------------------------------------------------
{
    state.charFormat.setFontPointSize(size);
    GLStateKeeper save;
    frame->FontSize(size);
    return XL::xl_true;
}


Tree *Widget::fontPlain(Tree *self)
// ----------------------------------------------------------------------------
//   Select whether this is italic or not
// ----------------------------------------------------------------------------
{
    state.charFormat.setFontItalic(false);
    state.charFormat.setFontWeight(QFont::Normal);
    state.charFormat.setFontUnderline(false);
    state.charFormat.setFontOverline(false);
    state.charFormat.setFontStrikeOut(false);
    return XL::xl_true;
}


Tree *Widget::fontItalic(Tree *self, bool italic)
// ----------------------------------------------------------------------------
//   Select whether this is italic or not
// ----------------------------------------------------------------------------
{
    state.charFormat.setFontItalic(italic);
    return XL::xl_true;
}


Tree *Widget::fontBold(Tree *self, bool bold)
// ----------------------------------------------------------------------------
//   Select whether the font is bold or not
// ----------------------------------------------------------------------------
{
    state.charFormat.setFontWeight( bold ? QFont::Bold : QFont::Normal);
    return XL::xl_true;
}


Tree *Widget::fontUnderline(Tree *self, bool underline)
// ----------------------------------------------------------------------------
//    Select whether we underline a font
// ----------------------------------------------------------------------------
{
    state.charFormat.setFontUnderline(underline);
    return XL::xl_true;
}


Tree *Widget::fontOverline(Tree *self, bool overline)
// ----------------------------------------------------------------------------
//    Select whether we draw an overline
// ----------------------------------------------------------------------------
{
    state.charFormat.setFontOverline(overline);
    return XL::xl_true;
}


Tree *Widget::fontStrikeout(Tree *self, bool strikeout)
// ----------------------------------------------------------------------------
//    Select whether we strikeout a font
// ----------------------------------------------------------------------------
{
    state.charFormat.setFontStrikeOut(strikeout);
    return XL::xl_true;
}


Tree *Widget::fontStretch(Tree *self, int stretch)
// ----------------------------------------------------------------------------
//    Set font streching factor
// ----------------------------------------------------------------------------
{
    //state.charFormat.setFontStretch(stretch);
    return XL::xl_true;
}


Tree *Widget::align(Tree *self, int align)
// ----------------------------------------------------------------------------
//   Set text alignment
// ----------------------------------------------------------------------------
{
    Qt::Alignment old = state.flow->paragraphOption.alignment();
    if (align & Qt::AlignHorizontal_Mask)
        old &= ~Qt::AlignHorizontal_Mask;
    if (align & Qt::AlignVertical_Mask)
        old &= ~Qt::AlignVertical_Mask;
    align |= old;
    state.flow->paragraphOption.setAlignment(Qt::Alignment(align));
    return XL::xl_true;
}


Tree *Widget::textSpan(Tree *self, text content)
// ----------------------------------------------------------------------------
//   Insert a block of text with the current definition of font, color, ...
// ----------------------------------------------------------------------------
{
    state.flow->addText(QString::fromUtf8(content.c_str(), content.length()),
                        state.charFormat);
    return XL::xl_true;
}


Tree *Widget::flow(Tree *self)
// ----------------------------------------------------------------------------
//   Create a new text flow
// ----------------------------------------------------------------------------
{
    TextFlow *thisFlow = self->GetInfo<TextFlow>();
    if (!thisFlow)
    {
        thisFlow = new TextFlow(state.flow->paragraphOption);
        self->SetInfo<TextFlow> (thisFlow);
    }
    state.flow = thisFlow;

    return XL::xl_true;
}


Tree *Widget::frameTexture(Tree *self, double w, double h)
// ----------------------------------------------------------------------------
//   Make a texture out of the current text layout
// ----------------------------------------------------------------------------
{
    if (w < 16) w = 16;
    if (h < 16) h = 16;

    // Get or build the current frame if we don't have one
    FrameInfo *frame = self->GetInfo<FrameInfo>();
    if (!frame)
    {
        frame = new FrameInfo(w,h);
        self->SetInfo<FrameInfo> (frame);
    }

    if (1)
    {
        GLStateKeeper save;

        frame->resize(w,h);
        frame->begin();
        {
            // Clear the background and setup initial state
            setup(w, h);
            XL::LocalSave<QPaintDevice *> sv(state.paintDevice,
                                             frame->render_fbo);

            TextFlow *flow = state.flow;
            qreal lineY = 0, lineHeight = 0, topY = flow->topLineY;

            flow->setText(flow->completeText);
            flow->setAdditionalFormats(flow->formats);
            flow->setTextOption(flow->paragraphOption);
            flow->beginLayout();

            while(true)
            {
                // Create a new line
                QTextLine line = flow->createLine();
                if (!line.isValid())
                    break;
                line.setLineWidth(w);
                line.setPosition(QPoint(0, lineY - topY));
                lineHeight = line.height();
                lineY += lineHeight;
            }

            flow->topLineY = lineY;
            flow->endLayout();

            QPainter painter(state.paintDevice);
            painter.setRenderHint(QPainter::Antialiasing, true);
            painter.setRenderHint(QPainter::HighQualityAntialiasing, true);
            painter.setRenderHint(QPainter::TextAntialiasing, true);
            flow->draw(&painter, QPoint(0,0));
            flow->clear();
        }
        frame->end();
    } // GLSateKeeper

    // Bind the resulting texture
    frame->bind();

    return XL::xl_true;
}


Tree *Widget::framePaint(Tree *self, double x, double y, double w, double h)
// ----------------------------------------------------------------------------
//   Draw a frame with the current text flow
// ----------------------------------------------------------------------------
{
    GLAttribKeeper save(GL_TEXTURE_BIT);
    frameTexture(self, w, h);

    // Draw a rectangle with the resulting texture
    ShapeSelection name(this, x-w/2, y-h/2, w, h);
    glBegin(GL_QUADS);
    {
        widgetVertex(x-w/2, y-h/2, 0, 0);
        widgetVertex(x+w/2, y-h/2, 1, 0);
        widgetVertex(x+w/2, y+h/2, 1, 1);
        widgetVertex(x-w/2, y+h/2, 0, 1);
    }
    glEnd();

    return XL::xl_true;
}


Tree *Widget::urlTexture(Tree *self, double w, double h,
                         Text *url, Integer *progress)
// ----------------------------------------------------------------------------
//   Make a texture out of a given URL
// ----------------------------------------------------------------------------
{
    if (w < 16) w = 16;
    if (h < 16) h = 16;

    // Get or build the current frame if we don't have one
    WebViewSurface *surface = url->GetInfo<WebViewSurface>();
    if (!surface)
    {
        surface = new WebViewSurface(this, w,h);
        url->SetInfo<WebViewSurface> (surface);
    }

    // Resize to requested size, and bind texture
    surface->resize(w,h);
    surface->bind(url, progress);

    return XL::xl_true;
}


Tree *Widget::urlPaint(Tree *self,
                       double x, double y, double w, double h,
                       Text *url, Integer *progress)
// ----------------------------------------------------------------------------
//   Draw a URL in the curent frame
// ----------------------------------------------------------------------------
{
    GLAttribKeeper save(GL_TEXTURE_BIT);
    ShapeName name(this);
    urlTexture(self, w, h, url, progress);

    // Draw a rectangle with the resulting texture
    glBegin(GL_QUADS);
    {
        widgetVertex(x-w/2, y-h/2, 0, 0);
        widgetVertex(x+w/2, y-h/2, 1, 0);
        widgetVertex(x+w/2, y+h/2, 1, 1);
        widgetVertex(x-w/2, y+h/2, 0, 1);
    }
    glEnd();
    if (selected())
        drawSelection(Box(x-w/2, y-h/2, w, h));

    return XL::xl_true;
}


Tree *Widget::lineEditTexture(Tree *self, double w, double h, Text *txt)
// ----------------------------------------------------------------------------
//   Make a texture out of a given line editor
// ----------------------------------------------------------------------------
{
    if (w < 16) w = 16;
    if (h < 16) h = 16;

    // Get or build the current frame if we don't have one
    LineEditSurface *surface = txt->GetInfo<LineEditSurface>();
    if (!surface)
    {
        surface = new LineEditSurface(this, w,h);
        txt->SetInfo<LineEditSurface> (surface);
    }

    // Resize to requested size, and bind texture
    surface->resize(w,h);
    surface->bind(txt);

    return XL::xl_true;
}


Tree *Widget::lineEdit(Tree *self,
                       double x, double y,
                       double w, double h, Text *txt)
// ----------------------------------------------------------------------------
//   Draw a line editor in the curent frame
// ----------------------------------------------------------------------------
{
    GLAttribKeeper save(GL_TEXTURE_BIT);
    ShapeName name(this);
    lineEditTexture(self, w, h, txt);

    // Draw a rectangle with the resulting texture
    glBegin(GL_QUADS);
    {
        widgetVertex(x-w/2, y-h/2, 0, 0);
        widgetVertex(x+w/2, y-h/2, 1, 0);
        widgetVertex(x+w/2, y+h/2, 1, 1);
        widgetVertex(x-w/2, y+h/2, 0, 1);
    }
    glEnd();

    if (selected())
        drawSelection(Box(x-w/2, y-h/2, w, h));

    return XL::xl_true;
}


Tree *Widget::qtrectangle(Tree *self, double x, double y, double w, double h)
// ----------------------------------------------------------------------------
//    Draw a rectangle using the Qt primitive
// ----------------------------------------------------------------------------
{
    ShapeSelection name(this, x, y, w, h);

    QPainter painter(state.paintDevice);
    QPen pen(QColor(Qt::red));
    pen.setWidth(4);
    painter.setPen(pen);
    painter.drawRect(QRectF(x,y,w,h));

    return XL::xl_true;
}


Tree *Widget::qttext(Tree *self, double x, double y, text s)
// ----------------------------------------------------------------------------
//    Draw a text using the Qt text primitive
// ----------------------------------------------------------------------------
{
    ShapeName name(this);

    QPainter painter(state.paintDevice);
    setAutoFillBackground(false);
    if (selected())
        painter.setPen(Qt::darkRed);
    else
        painter.setPen(Qt::darkBlue);

    QFont font("Arial");
    font.setPointSizeF(24);
    painter.setFont(font);
    painter.drawText(QPointF(x,y), Utf8(s));

    return XL::xl_true;
}


Tree *Widget::KmoveTo(Tree *self, double x, double y)
// ----------------------------------------------------------------------------
//   Move to the given Cairo coordinates
// ----------------------------------------------------------------------------
{
    frame->MoveTo(x,y);
    return XL::xl_true;
}


Tree *Widget::Ktext(Tree *self, text s)
// ----------------------------------------------------------------------------
//    Text at the current cursor position
// ----------------------------------------------------------------------------
{
    ShapeName name(this);
    frame->Text(s);
    return XL::xl_true;
}


Tree *Widget::KlayoutText(Tree *self, text s)
// ----------------------------------------------------------------------------
//    Text layout with Pango at the current cursor position
// ----------------------------------------------------------------------------
{
    ShapeName name(this);
    frame->LayoutText(s);
    return XL::xl_true;
}


Tree *Widget::KlayoutMarkup(Tree *self, text s)
// ----------------------------------------------------------------------------
//    Text layout with markup using Pango at the current cursor position
// ----------------------------------------------------------------------------
{
    ShapeName name(this);
    frame->LayoutMarkup(s);
    return XL::xl_true;
}


Tree *Widget::Krectangle(Tree *self, double x, double y, double w, double h)
// ----------------------------------------------------------------------------
//    Draw a rectangle using Cairo
// ----------------------------------------------------------------------------
{
    ShapeName name(this);
    frame->Rectangle(x, y, w, h);
    return XL::xl_true;
}


Tree *Widget::Kstroke(Tree *self)
// ----------------------------------------------------------------------------
//    Stroke the current path
// ----------------------------------------------------------------------------
{
    ShapeName name(this);
    frame->Stroke();
    return XL::xl_true;
}


Tree *Widget::Kclear(Tree *self)
// ----------------------------------------------------------------------------
//    Clear the current frame
// ----------------------------------------------------------------------------
{
    frame->Clear();
    return XL::xl_true;
}



// ============================================================================
//
//   Menu management
//
// ============================================================================
// * Menu name philosophy :
// * The full name is used to register menus and menu items against the menubar.
//   Those names are not displayed.
// * Menu created by the XL programmer must be differentiated from the originals
//   ones because they have to be recreated or modified at each loop of XL.
//   When top menus are deleted they recursively delete their children (sub
//   menus and menu items), so we have to take care of sub menu at deletion
//   time. Regarding those constraints, main menus are prefixed with _TOP_MENU_,
//   sub menus are prefixed by _SUB_MENU_. Then each menu item and sub menu are
//   prefixed by the "current menu" name (this current menu may itself be a
//   submenu). Each part of the name are separated by a /.
//
// * Menu and menu items lifecycle :
//   Menus are created when the xl program is executed the first time.
//   Menus display text can be modified at each execution.
//   Menus are destroyed when the xl program is invalidated.
//   At save time, the old xl program is invalidated and the new one is executed
//      for the first time.
// ============================================================================

Tree *Widget::menuItem(Tree *self, text s, Tree *t)
// ----------------------------------------------------------------------------
//   Create a menu item
// ----------------------------------------------------------------------------
{
    if (!currentMenu)
        return XL::xl_false;

    QString fullName = currentMenu->objectName() +
                      "/" +
                      QString::fromStdString(s);

    if (parent()->findChild<QAction*>(fullName))
    {
        IFTRACE(menus)
        {
            std::cout<< "MenuItem " << s
                     << " found in current MenuBar with fullname "
                     << fullName.toStdString() << "\n";
            std::cout.flush();
        }
        return XL::xl_true;
    }

    // Get or build the current Menu if we don't have one
    MenuInfo *menuInfo = self->GetInfo<MenuInfo>();

    // Store a copy of the tree in the QAction.
    XL::TreeClone cloner;
    XL::Tree *copy = t->Do(cloner);
    QVariant var = QVariant::fromValue(TreeHolder(copy));

    if (menuInfo)
    {
        // The name of the menuItem has changed.
        IFTRACE(menus)
        {
            std::cout << "menuInfo found, old name is "
                      << menuInfo->fullName << " new name is "
                      << fullName.toStdString() << "\n";
            std::cout.flush();
        }
        menuInfo->action->setText(QString::fromStdString(s));
        menuInfo->action->setObjectName(fullName);
        menuInfo->action->setData(var);
        menuInfo->fullName = fullName.toStdString();
        return XL::xl_true;
    }

    menuInfo = new MenuInfo(currentMenu, fullName.toStdString());
    self->SetInfo<MenuInfo> (menuInfo);

    IFTRACE(menus)
    {
        std::cout << "menuItem creation with name "
                  << fullName.toStdString() << "\n";
        std::cout.flush();
    }

    QAction * p_action = currentMenu->addAction(QString::fromStdString(s));
    menuInfo->action = p_action;
    p_action->setData(var);
    p_action->setObjectName(fullName);

    return XL::xl_true;
}

Tree *Widget::menu(Tree *self, text s, bool isSubMenu)
// ----------------------------------------------------------------------------
// Add the menu to the current menu bar or create the contextual menu
// ----------------------------------------------------------------------------
{
    QMenu * tmp = NULL;
    bool isContextMenu = false;

    //---------------------------------
    // Build the full name of the menu
    //---------------------------------
    // Uses the current menu name, the given string and the isSubmenu.
    QString fullname = QString::fromStdString(s);
    if (isSubMenu && currentMenu)
    {
        fullname.prepend(currentMenu->objectName() +'/');
        fullname.replace(TOPMENU, SUBMENU);

    } 
    else if (fullname.startsWith(CONTEXT_MENU))
    {
        isContextMenu = true;
    }
    else 
    {
        fullname.prepend( TOPMENU );
    }

    // Get or build the current Menu if we don't have one
    MenuInfo *menuInfo = self->GetInfo<MenuInfo>();


    //---------------------------------------------------
    // If the menu is registered, no need to recreate it.
    //---------------------------------------------------
    // This is used at reload time, recreate the MenuInfo if required.
    if (tmp = parent()->findChild<QMenu*>(fullname))
    {
        currentMenu = tmp;
        if (!menuInfo)
        {
            menuInfo = new MenuInfo(isContextMenu ? NULL : currentMenuBar,
                                    currentMenu,
                                    fullname.toStdString());
            self->SetInfo<MenuInfo> (menuInfo);
            menuInfo->action = currentMenu->menuAction();

        }
        return XL::xl_true;
    }

    //-------------------------------
    // The menu is not yet registered.
    //-------------------------------
    // The name may have change but not the content
    // (in the loop of the XL program execution).
    if (menuInfo)
    {
        // The menu exists : update its info
        currentMenu = menuInfo->menu;
        menuInfo->action->setText(QString::fromStdString(s));
        menuInfo->menu->setObjectName(fullname);
        menuInfo->fullName = fullname.toStdString();
        return XL::xl_true;
    }

    //----------------------------------------
    // The menu is not existing. Creating it.
    //----------------------------------------
    if (isContextMenu)
    {
        currentMenu = new QMenu((Window*)parent());
        connect(currentMenu, SIGNAL(triggered(QAction*)),
                this,        SLOT(userMenu(QAction*)));

    }
    else if (isSubMenu)
        currentMenu = currentMenu->addMenu(QString::fromStdString(s));
    else
        currentMenu = currentMenuBar->addMenu(QString::fromStdString(s));

    currentMenu->setObjectName(fullname);

    menuInfo = new MenuInfo(isContextMenu ? NULL : currentMenuBar,
                            currentMenu,
                            fullname.toStdString());
    self->SetInfo<MenuInfo> (menuInfo);
    menuInfo->action = currentMenu->menuAction();

    return XL::xl_true;
}

TAO_END
