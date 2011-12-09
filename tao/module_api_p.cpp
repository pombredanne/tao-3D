// ****************************************************************************
//  module_api_p.cpp                                               Tao project
// ****************************************************************************
//
//   File Description:
//
//    Implementation of interface with native modules
//
//
//
//
//
//
//
//
// ****************************************************************************
// This software is property of Taodyne SAS - Confidential
// Ce logiciel est la propriété de Taodyne SAS - Confidentiel
//  (C) 2010 Jerome Forissier <jerome@taodyne.com>
//  (C) 2010 Taodyne SAS
// ****************************************************************************

#include "tao.h"
#include "module_api_p.h"
#include "module_renderer.h"
#include "widget.h"
#include "display_driver.h"
#include "frame.h"
#include "licence.h"
#include "info_trash_can.h"

TAO_BEGIN

ModuleApiPrivate::ModuleApiPrivate()
// ------------------------------------------------------------------------
//   Set function pointers for all functions exported to modules
// ------------------------------------------------------------------------
{
    scheduleRender   = ModuleRenderer::ScheduleRender;
    refreshOn        = Widget::refreshOn;
    currentTime      = Widget::currentTimeAPI;
    addToLayout      = ModuleRenderer::AddToLayout;
    AddToLayout2     = ModuleRenderer::AddToLayout2;
    addControlBox    = Widget::addControlBox;

    isGLExtensionAvailable = Widget::isGLExtensionAvailable;

    //Drawing paramaters
    SetTextures      = ModuleRenderer::SetTextures;
    BindTexture      = ModuleRenderer::BindTexture;
    BindTexture2D    = ModuleRenderer::BindTexture2D;
    EnableTexCoords  = ModuleRenderer::EnableTexCoords;
    DisableTexCoords = ModuleRenderer::DisableTexCoords;
    TextureUnits     = ModuleRenderer::TextureUnits;
    SetTextureUnits  = ModuleRenderer::SetTextureUnits;
    SetFillColor     = ModuleRenderer::SetFillColor;
    SetLineColor     = ModuleRenderer::SetLineColor;
    SetShader        = ModuleRenderer::SetShader;
    HasPixelBlur     = ModuleRenderer::HasPixelBlur;
    EnabledLights    = ModuleRenderer::EnabledLights;
    ModelMatrix      = ModuleRenderer::ModelMatrix;

    deferredDelete   = InfoTrashCan::DeferredDelete;
    makeGLContextCurrent = Widget::makeGLContextCurrent;

    // Display module API
    registerDisplayFunction      = DisplayDriver::registerDisplayFunction;
    registerDisplayFunctionAlias = DisplayDriver::registerDisplayFunctionAlias;
    setGlClearColor              = DisplayDriver::setGlClearColor;
    setupGl                      = DisplayDriver::setupGl;
    showGlErrors                 = DisplayDriver::showGlErrors;
    setStereo                    = DisplayDriver::setStereo;
    setProjectionMatrix          = DisplayDriver::setProjectionMatrix;
    setModelViewMatrix           = DisplayDriver::setModelViewMatrix;
    drawScene                    = DisplayDriver::drawScene;
    drawSelection                = DisplayDriver::drawSelection;
    drawActivities               = DisplayDriver::drawActivities;
    getCamera                    = DisplayDriver::getCamera;
    renderHeight                 = DisplayDriver::renderHeight;
    renderWidth                  = DisplayDriver::renderWidth;
    zNear                        = DisplayDriver::zNear;
    zFar                         = DisplayDriver::zFar;
    zoom                         = DisplayDriver::zoom;
    eyeSeparation                = DisplayDriver::eyeSeparation;
    doMouseTracking              = DisplayDriver::doMouseTracking;
    setMouseTrackingViewport     = DisplayDriver::setMouseTrackingViewport;
    setWatermarkText             = Widget::setWatermarkTextAPI;
    drawWatermark                = Widget::drawWatermarkAPI;
    _2dPlusDepthBackground       = Widget::_2dPlusDepthBackgroundAPI;

    // Framebuffer API
    newFrameBufferObject       = FrameInfo::newFrameBufferObject;
    deleteFrameBufferObject    = FrameInfo::deleteFrameBufferObject;
    resizeFrameBufferObject    = FrameInfo::resizeFrameBufferObject;
    bindFrameBufferObject      = FrameInfo::bindFrameBufferObject;
    releaseFrameBufferObject   = FrameInfo::releaseFrameBufferObject;
    frameBufferObjectToTexture = FrameInfo::frameBufferObjectToTexture;
    frameBufferAttachmentToTexture = FrameInfo::frameBufferAttachmentToTexture;

    // License checking
    hasLicense = Licences::Has;
    checkLicense = Licences::Check;
    blink      = Widget::blink;

    // Current document info
    currentDocumentFolder =  Widget::currentDocumentFolder;
}

TAO_END
