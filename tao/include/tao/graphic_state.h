#ifndef GRAPHIC_STATE_H
#define GRAPHIC_STATE_H
// ****************************************************************************
//  graphic_state.h                                                Tao project
// ****************************************************************************
//
//   File Description:
//
//    Graphic state interface for native modules
//
//
//
//
//
//
//
// ****************************************************************************
// This file may be used in accordance with the terms and conditions contained
// in the Tao Presentations license agreement, or alternatively, in a signed
// license agreement between you and Taodyne SAS.
//  (C) 2012 Baptiste Soulisse <soulisse.baptiste@taodyne.com>
//  (C) 2012 Taodyne SAS
// ****************************************************************************

#include "coords3d.h"
#include "matrix.h"
#include "tao_gl.h"

TAO_BEGIN


struct GraphicSave
// ----------------------------------------------------------------------------
//   Used to save/restore a graphic state
// ----------------------------------------------------------------------------
{
    GraphicSave()               {}
    virtual ~GraphicSave()      {}
};


struct GraphicState
// ----------------------------------------------------------------------------
//   Interface to manage graphic states
// ----------------------------------------------------------------------------
{
    GraphicState()              {}
    virtual ~GraphicState()     {}

    // Saving and restoring state
    virtual GraphicSave *       Save() = 0;
    virtual void                Restore(GraphicSave *saved) = 0;

    // Apply pending state changes
    virtual void                Sync(uint64 which = ~0ULL) = 0;
    virtual void                Invalidate(uint64 which = ~0ULL) = 0;

    // Return attributes of state
    virtual uint   MaxTextureCoords() = 0;
    virtual uint   MaxTextureUnits() = 0;
    virtual uint   MaxTextureSize() = 0;
    virtual text   Vendor() = 0;
    virtual text   Renderer() = 0;
    virtual text   Version() = 0;
    virtual coord* ModelViewMatrix() = 0;
    virtual coord* ProjectionMatrix() = 0;
    virtual int*   Viewport() = 0;
    virtual void   Get(GLenum pname, GLboolean * params) = 0;
    virtual void   Get(GLenum pname, GLfloat * params) = 0;
    virtual void   Get(GLenum pname, GLint * params) = 0;

    // Matrix management
    virtual void   MatrixMode(GLenum mode) = 0;
    virtual void   LoadMatrix() = 0;
    virtual void   LoadIdentity() = 0;

    // Transformations
    virtual void Translate(double x, double y, double z) = 0;
    virtual void Rotate(double a, double x, double y, double z)  = 0;
    virtual void Scale(double x, double y, double z) = 0;

    // Camera management
    virtual bool Project(coord objx, coord objy, coord objz,
                         const coord* mv, const coord* proj,
                         const int* viewport,
                         coord *winx, coord *winy, coord *winz) = 0;
    virtual bool UnProject(coord winx, coord winy, coord winz,
                           const coord* mv, const coord* proj,
                           const int* viewport,
                           coord *objx, coord *objy, coord *objz) = 0;
    virtual void PickMatrix(float x, float y, float width, float height,
                            int viewport[4]) = 0;
    virtual void Frustum(float left, float right, float bottom, float top,
                         float nearZ, float farZ) = 0;
    virtual void Perspective(float fovy, float aspect, float nearZ, float farZ) = 0;
    virtual void Ortho(float left, float right, float bottom,
                       float top, float nearZ, float farZ) = 0;
    virtual void Ortho2D(float left, float right, float bottom, float top) = 0;
    virtual void LookAt(float eyeX, float eyeY, float eyeZ,
                        float centerX, float centerY, float centerZ,
                        float upX, float upY, float upZ) = 0;
    virtual void LookAt(Vector3 eye, Vector3 center, Vector3 up) = 0;

    // Drawing functions
    virtual void DrawBuffer(GLenum mode) = 0;
    virtual void Begin(GLenum mode) = 0;
    virtual void End() = 0;
    virtual void Vertex(coord x, coord y, coord z = 0, coord w = 1.0) = 0;
    virtual void Vertex3v(const coord* v) = 0;
    virtual void Normal(coord nx, coord ny, coord nz) = 0;
    virtual void TexCoord(coord s, coord t) = 0;
    virtual void MultiTexCoord3v(GLenum target, const coord *array) = 0;
    virtual void EnableClientState(GLenum cap) = 0;
    virtual void DisableClientState(GLenum cap) = 0;
    virtual void ClientActiveTexture(GLenum tex) = 0;
    virtual void DrawElements(GLenum  mode, int count, GLenum type, const GLvoid *  indices) = 0;
    virtual void DrawArrays(GLenum mode, int first, int count) = 0;
    virtual void VertexPointer(int size, GLenum type, int stride,
                               const void* pointer) = 0;
    virtual void NormalPointer(GLenum type, int stride,
                               const void* pointer) = 0;
    virtual void TexCoordPointer(int size, GLenum type, int stride,
                                 const void* pointer) = 0;
    virtual void ColorPointer(int size, GLenum type, int stride,
                              const void* pointer) = 0;
    virtual void NewList(uint list, GLenum mode) = 0;
    virtual void EndList() = 0;
    virtual uint GenLists(uint range) = 0;
    virtual void DeleteLists(uint list, uint range) = 0;
    virtual void CallList(uint list) = 0;
    virtual void CallLists(uint size, GLenum type, const void* lists) = 0;
    virtual void ListBase(uint base) = 0;
    virtual void Bitmap(uint  width,  uint  height, coord  xorig,
                        coord  yorig,  coord  xmove, coord  ymove,
                        const uchar *  bitmap) = 0;
    virtual void DrawPixels(GLsizei width, GLsizei height, GLenum format,
                            GLenum type, const GLvoid *data) = 0;

    // Attributes management
    virtual void Viewport(int x, int y, int w, int h) = 0;
    virtual void RasterPos(coord x, coord y, coord z = 0, coord w = 1) = 0;
    virtual void WindowPos(coord x, coord y, coord z = 0, coord w = 1) = 0;
    virtual void PixelStorei(GLenum pname,  int param) = 0;
    virtual void PointSize(coord size) = 0;
    virtual void Color(float r, float g, float b, float a = 1.0) = 0;
    virtual void Materialfv(GLenum face, GLenum pname, const GLfloat *val) = 0;
    virtual void ClearColor(float r, float g, float b, float a = 1.0) = 0;
    virtual void Clear(GLuint mask) = 0;
    virtual void LineWidth(float width) = 0;
    virtual void LineStipple(GLint factor, GLushort pattern) = 0;
    virtual void CullFace(GLenum mode) = 0;
    virtual void DepthMask(GLboolean flag) = 0;
    virtual void DepthFunc(GLenum func) = 0;
    virtual void ShadeModel(GLenum mode) = 0;
    virtual void Hint(GLenum target, GLenum mode) = 0;
    virtual void Enable(GLenum cap) = 0;
    virtual void Disable(GLenum cap) = 0;

    // Blend
    virtual void BlendFunc(GLenum sfactor, GLenum dfactor) = 0;
    virtual void BlendFuncSeparate(GLenum sRgb, GLenum dRgb,
                                   GLenum sAlpha, GLenum dAlpha) = 0;
    virtual void BlendEquation(GLenum mode) = 0;

    // Alpha
    virtual void AlphaFunc(GLenum func, float ref) = 0;

    // Stencil
    virtual void ClearStencil(GLint s) = 0;
    virtual void StencilFunc(GLenum func, GLint ref, GLuint mask) = 0;
    virtual void StencilOp(GLenum sfail, GLenum dpfail, GLenum dppass) = 0;

    // Clipping plane
    virtual void ClipPlane(GLenum plane, const GLdouble *equation) = 0;

    // Selection
    virtual int  RenderMode(GLenum mode) = 0;
    virtual void SelectBuffer(int size, uint* buffer) = 0;
    virtual void InitNames() = 0;
    virtual void LoadName(uint name) = 0;
    virtual void PushName(uint name) = 0;
    virtual void PopName() = 0;

    // Shaders
    virtual void UseProgram(uint prg) = 0;
    virtual void GetProgram(uint prg, GLenum pname, int* params) = 0;
    virtual void GetUniformIndices(uint prg, GLsizei count, const char** names, GLuint* indices) = 0;
    virtual void GetActiveUniform(uint prg, uint id, uint bufSize, GLsizei* length,
                                  GLsizei* size, GLenum* type, char *name) = 0;

    virtual int  GetAttribLocation(uint program, const char* name) = 0;
    virtual void VertexAttrib1fv(uint id, const float *v) = 0;
    virtual void VertexAttrib2fv(uint id, const float *v) = 0;
    virtual void VertexAttrib3fv(uint id, const float *v) = 0;
    virtual void VertexAttrib4fv(uint id, const float *v) = 0;

    virtual int  GetUniformLocation(uint program, const char* name) = 0;
    virtual void Uniform(uint id, float v) = 0;
    virtual void Uniform(uint id, float v0, float v1) = 0;
    virtual void Uniform(uint id, float v0, float v1, float v2) = 0;
    virtual void Uniform(uint id, float v0, float v1, float v2, float v3) = 0;
    virtual void Uniform(uint id, int v) = 0;
    virtual void Uniform(uint id, int v0, int v1) = 0;
    virtual void Uniform(uint id, int v0, int v1, int v2) = 0;
    virtual void Uniform(uint id, int v0, int v1, int v2, int v3) = 0;
    virtual void Uniform(uint id, uint v) = 0;
    virtual void Uniform(uint id, uint v0, uint v1) = 0;
    virtual void Uniform(uint id, uint v0, uint v1, uint v2) = 0;
    virtual void Uniform(uint id, uint v0, uint v1, uint v2, uint v3) = 0;
    virtual void Uniform1fv(uint id, GLsizei size, const float* v) = 0;
    virtual void Uniform2fv(uint id, GLsizei size, const float* v) = 0;
    virtual void Uniform3fv(uint id, GLsizei size, const float* v) = 0;
    virtual void Uniform4fv(uint id, GLsizei size, const float* v) = 0;
    virtual void Uniform1iv(uint id, GLsizei size, const int* v) = 0;
    virtual void Uniform2iv(uint id, GLsizei size, const int* v) = 0;
    virtual void Uniform3iv(uint id, GLsizei size, const int* v) = 0;
    virtual void Uniform4iv(uint id, GLsizei size, const int* v) = 0;
    virtual void UniformMatrix2fv(uint id, GLsizei size,
                                  bool transp, const float* m) = 0;
    virtual void UniformMatrix3fv(uint id, GLsizei size,
                                  bool transp, const float* m) = 0;
    virtual void UniformMatrix4fv(uint id, GLsizei size,
                                  bool transp, const float* m) = 0;


    // Textures
    virtual void ActiveTexture(GLenum id) = 0;
    virtual void GenTextures(uint n, GLuint *  textures) = 0;
    virtual void DeleteTextures(uint n, GLuint *  textures) = 0;
    virtual void BindTexture(GLenum type, GLuint id) = 0;
    virtual void TexParameter(GLenum type, GLenum pname, GLint param) = 0;
    virtual void TexEnv(GLenum type, GLenum pname, GLint param) = 0;
    virtual void TexGen(GLenum coord, GLenum pname, GLint param) = 0;
    virtual void TexImage2D(GLenum target, GLint level, GLint internalformat,
                            GLsizei width, GLsizei height, GLint border,
                            GLenum format, GLenum type,
                            const GLvoid *pixels ) = 0;
    virtual void CompressedTexImage2D(GLenum target, GLint level,
                                      GLenum internalformat,
                                      GLsizei width, GLsizei height,
                                      GLint border, GLsizei imgSize,
                                      const GLvoid *data) = 0;
    virtual void TexImage3D(GLenum target, GLint level, GLint internalformat,
                            GLsizei width, GLsizei height, GLsizei depth, GLint border,
                            GLenum format, GLenum type,
                            const GLvoid *pixels ) = 0;

    virtual void TextureSize(uint width, uint height, uint depth = 0) = 0;
    virtual uint TextureWidth() = 0;
    virtual uint TextureHeight() = 0;
    virtual uint TextureDepth() = 0;
    virtual uint TextureType() = 0;
    virtual uint TextureMode() = 0;
    virtual uint TextureID() = 0;
    virtual void ActivateTextureUnits(uint64 mask) = 0;
    virtual uint ActiveTextureUnitIndex() = 0;
    virtual uint ActiveTextureUnitsCount() = 0;
    virtual uint64 ActiveTextureUnits() = 0;
    virtual void HasPixelBlur(bool enable) = 0;
    virtual void GenerateMipmap(GLenum target) = 0;

    // Lighting
    virtual void Light(GLenum light, GLenum pname, const float* params) = 0;
};

TAO_END

#endif // GRAPHIC_STATE_H
