#ifndef GL_KEEPERS_H
#define GL_KEEPERS_H
// ****************************************************************************
//  gl_state_keepr.h                                                Tao project
// ****************************************************************************
//
//   File Description:
//
//   Helper classes to save and restore OpenGL selected attributes and/or the
//   current matrix.
//
//   The mask used to indicates which groups of state variables to save on the
//   attribute stack is the same than the one of glPushAttrib.
//
//
//
//
// ****************************************************************************
// This software is property of Taodyne SAS - Confidential
// Ce logiciel est la propriété de Taodyne SAS - Confidentiel
//  (C) 2010 Lionel Schaffhauser <lionel@taodyne.com>
//  (C) 2010 Christophe de Dinechin <ddd@taodyne.com>
//  (C) 2010 Taodyne SAS
// ****************************************************************************

#include "tao.h"
#include "widget.h"
#include "tao_gl.h"
#include "application.h"
#include "opengl_save.h"

TAO_BEGIN

struct GLAttribKeeper : OpenGLSave
// ----------------------------------------------------------------------------
//   Save and restore selected OpenGL attributes
// ----------------------------------------------------------------------------
//   By default, all attributes are saved (GL_ALL_ATTRIB_BITS)
{
    GLAttribKeeper(GLbitfield bits = GL_ALL_ATTRIB_BITS)
        : OpenGLSave(~(SAVE_mvMatrix | SAVE_projMatrix | SAVE_viewport))
    { (void) bits; }
};


struct GLMatrixKeeper : OpenGLSave
// ----------------------------------------------------------------------------
//   Save and restore the current matrix
// ----------------------------------------------------------------------------
//   Caller is responsible for current matrix mode (model or projection view)
{
    GLMatrixKeeper(bool save=true)
        : OpenGLSave(save ? (SAVE_mvMatrix | SAVE_projMatrix) : 0)
    { }
};


struct GLStateKeeper : OpenGLSave
// ----------------------------------------------------------------------------
//   Save and restore both selected attributes and the current matrix
// ----------------------------------------------------------------------------
{
    GLStateKeeper(GLbitfield bits = GL_ALL_ATTRIB_BITS, bool save = true):
        OpenGLSave(save ? ~0U : ~(SAVE_mvMatrix | SAVE_projMatrix))
    { (void) bits; }
};


struct GLAllStateKeeper : OpenGLSave
// ----------------------------------------------------------------------------
//   Save and restore both selected attributes and the current matrices
// ----------------------------------------------------------------------------
{
    GLAllStateKeeper(GLbitfield bits = GL_ALL_ATTRIB_BITS,
                     bool saveModel = true,
                     bool saveProjection = true,
                     uint64 saveTextureMatrix = ~0UL)
        : OpenGLSave((saveModel         ? ~0U : ~SAVE_mvMatrix) &
                     (saveProjection    ? ~0U : ~SAVE_projMatrix))
    { (void) bits; (void) saveTextureMatrix; }
};

TAO_END

#endif // GL_KEEPERS_H
