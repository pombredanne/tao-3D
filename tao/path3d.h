#ifndef PATH3D_H
#define PATH3D_H
// ****************************************************************************
//  path3d.h                                                        Tao project
// ****************************************************************************
//
//   File Description:
//
//     Representation of paths in 3D
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
//  (C) 2010 Taodyne SAS
// ****************************************************************************

#include "coords3d.h"
#include "shapes.h"
#include "tree.h"
#include "tao_tree.h"
#include <GL/glew.h>

struct QPainterPath;

TAO_BEGIN

struct ControlPoint;
struct FrameManipulator;
struct GraphicPath : Shape
// ----------------------------------------------------------------------------
//    An arbitrary graphic path
// ----------------------------------------------------------------------------
{
    GraphicPath();
    ~GraphicPath();

    virtual void        Draw(Layout *where);
    virtual void        DrawSelection(Layout *where);
    virtual void        Identify(Layout *where);
    virtual void        Draw(Layout *where, GLenum tessel);
    virtual void        Draw(const Vector3 &offset, GLenum mode, GLenum tessel);
    virtual Box3        Bounds(Layout *layout);

    // Absolute coordinates
    GraphicPath&        moveTo(Point3 dst);
    GraphicPath&        lineTo(Point3 dst);
    GraphicPath&        curveTo(Point3 control, Point3 dst);
    GraphicPath&        curveTo(Point3 c1, Point3 c2, Point3 dst);
    GraphicPath&        close();

    // Relative coordinates
    GraphicPath&        moveTo(Vector3 dst)     { return moveTo(position+dst); }
    GraphicPath&        lineTo(Vector3 dst)     { return lineTo(position+dst); }

    // Qt path conversion
    GraphicPath&        addQtPath(QPainterPath &path, scale sy = 1);
    static bool         extractQtPath(GraphicPath &in, QPainterPath &out);
    bool                extractQtPath(QPainterPath &path);
    static void         Draw(Layout *where, QPainterPath &path,
                             GLenum tessel = 0, scale sy = 1);

    // Other operations
    void                clear();
    void                AddControl(XL::Tree *self, Real *x,Real *y,Real *z);

public:
    enum Kind           { MOVE_TO, LINE_TO, CURVE_TO, CURVE_CONTROL };
    enum EndpointStyle  { NONE, 
                          ARROWHEAD, TRIANGLE, POINTER, DIAMOND, CIRCLE,
                          SQUARE, BAR, CUP, FLETCHING,
                          HOLLOW_CIRCLE, HOLLOW_SQUARE };
    struct Element
    {
        Element(Kind k, const Point3 &p): kind(k), position(p) {}
        Kind    kind;
        Point3  position;
    };
    typedef std::vector<Element> path_elements;
    typedef std::vector<ControlPoint *> control_points;
    struct VertexData
    {
        VertexData(const Point3& v, const Point3& t): vertex(v), texture(t) {}
        Vector3  vertex;
        Vector3  texture;
    };
    typedef std::vector<VertexData>   Vertices;
    typedef std::vector<VertexData *> DynamicVertices;
    struct PolygonData
    {
        PolygonData() {}
        ~PolygonData();
        Vertices        vertices;
        DynamicVertices allocated;
    };

public:
    path_elements       elements;
    control_points      controls;
    Point3              start, position;
    Box3                bounds;
    EndpointStyle       startStyle, endStyle;
    static scale        default_steps;
};


struct GraphicPathInfo : XL::Info
// ----------------------------------------------------------------------------
//    Information about a given GraphicPath
// ----------------------------------------------------------------------------
{
    typedef GraphicPathInfo *data_t;

    GraphicPathInfo(GraphicPath *path);

    std::vector<Point3> controls;
    Box3                b0;
};


struct TesselatedPath : GraphicPath
// ----------------------------------------------------------------------------
//   Like a graphic path, but with explicit tesselation
// ----------------------------------------------------------------------------
{
    TesselatedPath(GLuint tesselation): tesselation(tesselation) {}
    void Draw(Layout *where);
    GLuint tesselation;
};

TAO_END

#endif // PATH3D_H
