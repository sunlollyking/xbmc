/*
 *      Copyright (C) 2017 Team Kodi
 *      http://kodi.tv
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this Program; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */

#include "RenderBufferPoolFBO.h"

#include "RenderBufferFBO.h"
#include "ServiceBroker.h"
#include "cores/RetroPlayer/rendering/RenderContext.h"
#include "cores/RetroPlayer/rendering/VideoRenderers/RPRendererFBO.h"

#include "utils/StringUtils.h"
#include "utils/log.h"
#include "windowing/WinSystem.h"
#include "windowing/linux/WinSystemEGL.h"

#include <string>
#include <vector>

using namespace KODI;
using namespace RETRO;

CRenderBufferPoolFBO::CRenderBufferPoolFBO(CRenderContext& context) : m_context(context)
{
}

CRenderBufferPoolFBO::~CRenderBufferPoolFBO()
{
  // The context is torn down by DestroyContext(), which the stream calls on the
  // game loop thread while the context is still current. By the time the pool
  // is destroyed there is normally nothing left to do, and nothing that can be
  // done: this runs on whichever thread drops the last reference, where the
  // context is not current and deleting its objects would be undefined.
  if (m_eglContext != EGL_NO_CONTEXT)
    CLog::Log(LOGWARNING, "RetroPlayer[RENDER]: FBO context outlived its stream, leaking it");
}

bool CRenderBufferPoolFBO::IsCompatible(const CRenderVideoSettings& renderSettings) const
{
  return CRPRendererFBO::SupportsScalingMethod(renderSettings.GetScalingMethod());
}

bool CRenderBufferPoolFBO::ConfigureInternal()
{
  // This pool only serves hardware-rendered streams, which carry no CPU-side
  // pixel format. Software streams declare a real format and must be left to
  // the DMA and sysmem pools.
  return m_format == AV_PIX_FMT_NONE;
}

IRenderBuffer* CRenderBufferPoolFBO::CreateRenderBuffer(void* header /* = nullptr */)
{
  if (m_eglContext == EGL_NO_CONTEXT)
  {
    CLog::Log(LOGERROR, "RetroPlayer[RENDER]: No shared context; the stream must create one first");
    return nullptr;
  }

  // Framebuffer objects are not shared between contexts, so a buffer built
  // outside the client's context would be useless to it. This also keeps the
  // rendering thread from quietly allocating one in Kodi's context.
  if (m_clientFrameDepth == 0)
  {
    CLog::Log(LOGERROR,
              "RetroPlayer[RENDER]: Refusing to build a buffer outside the client's context");
    return nullptr;
  }

  return new CRenderBufferFBO(m_context, m_contextProperties.depth,
                              m_contextProperties.stencil,
                              m_contextProperties.bottomLeftOrigin);
}

bool CRenderBufferPoolFBO::CreateContext(const HwContextProperties& properties)
{
  // Idempotent, so reopening a stream on the same pool is harmless
  if (m_eglContext != EGL_NO_CONTEXT)
    return true;

  // Remembered so buffers can be built with the attachments the client wants
  m_contextProperties = properties;

  auto winSystem =
      dynamic_cast<KODI::WINDOWING::LINUX::CWinSystemEGL*>(CServiceBroker::GetWinSystem());
  if (winSystem == nullptr)
  {
    CLog::Log(LOGERROR, "RetroPlayer[RENDER]: Window system does not use EGL");
    return false;
  }

  m_eglDisplay = winSystem->GetEGLDisplay();

  if (m_eglDisplay == EGL_NO_DISPLAY)
  {
    CLog::Log(LOGERROR, "failed to get EGL display");
    return false;
  }

  if (!eglInitialize(m_eglDisplay, nullptr, nullptr))
  {
    CLog::Log(LOGERROR, "failed to initialize EGL display");
    return false;
  }

// clang-format off

  // Ask EGL for whichever API this build renders with. The pool, the buffers
  // and the renderer are common to both; only the context differs.
#if defined(HAS_GLES)
  eglBindAPI(EGL_OPENGL_ES_API);
#else
  eglBindAPI(EGL_OPENGL_API);
#endif

  EGLint attribs[] =
  {
#if defined(HAS_GLES)
    // ES3 rather than ES2: the framebuffer objects, the depth and stencil
    // attachments and the sampling this pool relies on are all core in ES3.
    EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
#else
    EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
#endif
    // The context is only ever made current without a surface, so the surface
    // type is a formality -- but eglChooseConfig defaults it to EGL_WINDOW_BIT
    // and matches on it either way, so it has to name something the platform
    // really offers. Window is the one every platform Kodi runs on provides.
    // Pbuffer is not: on GBM, configs come from the GBM formats and advertise
    // window only, so asking for a pbuffer matches nothing at all and every
    // hardware core fails to get a context.
    EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
    EGL_RED_SIZE,   8,
    EGL_GREEN_SIZE, 8,
    EGL_BLUE_SIZE,  8,
    EGL_ALPHA_SIZE, 8,
    EGL_NONE
  };

  EGLint neglconfigs;
  if (!eglChooseConfig(m_eglDisplay, attribs, &m_eglConfig, 1, &neglconfigs))
  {
    CLog::Log(LOGERROR, "Failed to query number of EGL configs");
    return false;
  }

  if (neglconfigs <= 0)
  {
    CLog::Log(LOGERROR, "No suitable EGL configs found");
    return false;
  }

  // clang-format on

  // Honour the context the core asked for. A core written against legacy
  // OpenGL will not run in a core profile - its shaders and fixed-function
  // calls are simply absent there - so requesting the wrong profile leaves the
  // core unable to build its resources, with no obvious symptom beyond a core
  // that never draws.
  std::vector<EGLint> contextAttribs;

  if (properties.versionMajor != 0)
  {
    contextAttribs.push_back(EGL_CONTEXT_MAJOR_VERSION_KHR);
    contextAttribs.push_back(static_cast<EGLint>(properties.versionMajor));
    contextAttribs.push_back(EGL_CONTEXT_MINOR_VERSION_KHR);
    contextAttribs.push_back(static_cast<EGLint>(properties.versionMinor));
  }

  if (!properties.embedded)
  {
    contextAttribs.push_back(EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR);
    contextAttribs.push_back(properties.coreProfile
                                 ? EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR
                                 : EGL_CONTEXT_OPENGL_COMPATIBILITY_PROFILE_BIT_KHR);
  }

  contextAttribs.push_back(EGL_NONE);

  // Describes what was asked for, so a rejection says which request the driver
  // could not meet
  std::string contextName = properties.embedded ? "OpenGL ES"
                            : properties.coreProfile ? "OpenGL core profile"
                                                     : "OpenGL compatibility profile";
  if (properties.versionMajor != 0)
    contextName += StringUtils::Format(" {}.{}", properties.versionMajor, properties.versionMinor);

  // This is the version and profile check: rather than testing the request
  // against a hardcoded table, ask the driver for it and let it refuse. A
  // refusal fails the stream cleanly, while the client can still fall back to
  // software rendering.
  m_eglContext = eglCreateContext(m_eglDisplay, m_eglConfig, winSystem->GetEGLContext(),
                                  contextAttribs.data());
  if (m_eglContext == EGL_NO_CONTEXT)
  {
    CLog::Log(LOGERROR,
              "RetroPlayer[RENDER]: Game client asked for a {} context, which this system cannot "
              "provide (EGL error {:#x})",
              contextName, eglGetError());
    return false;
  }

  CLog::Log(LOGDEBUG, "RetroPlayer[RENDER]: Created shared {} context for the game client",
            contextName);

  // Deliberately not made current here. This runs on whichever thread opened
  // the stream, which for some clients is Kodi's own rendering thread, and a
  // context binding is per-thread: taking that thread over would cost Kodi the
  // window surface it presents with. BeginClientFrame() binds it around the
  // client's work instead, on the thread doing that work.
  return true;
}

bool CRenderBufferPoolFBO::BeginClientFrame()
{
  if (m_eglContext == EGL_NO_CONTEXT)
    return false;

  const std::thread::id thisThread = std::this_thread::get_id();

  if (m_clientFrameDepth > 0)
  {
    // Nested, which is normal: allocating buffers binds the context too, and
    // that happens inside the client's frame for some clients.
    if (m_clientThread != thisThread)
    {
      CLog::Log(LOGERROR, "RetroPlayer[RENDER]: Client context is in use by another thread");
      return false;
    }

    ++m_clientFrameDepth;
    return true;
  }

  m_prevDisplay = eglGetCurrentDisplay();
  m_prevDraw = eglGetCurrentSurface(EGL_DRAW);
  m_prevRead = eglGetCurrentSurface(EGL_READ);
  m_prevContext = eglGetCurrentContext();

  if (!eglMakeCurrent(m_eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, m_eglContext))
  {
    CLog::Log(LOGERROR, "RetroPlayer[RENDER]: Failed to make the client's context current, EGL "
                        "error {:#x}", eglGetError());
    return false;
  }

  m_clientThread = thisThread;
  m_clientFrameDepth = 1;

  return true;
}

void CRenderBufferPoolFBO::EndClientFrame()
{
  if (m_clientFrameDepth == 0)
    return;

  if (--m_clientFrameDepth > 0)
    return;

  // Give the thread back exactly what it had, so Kodi keeps its surface if this
  // happened to be its rendering thread
  if (m_prevContext != EGL_NO_CONTEXT && m_prevDisplay != EGL_NO_DISPLAY)
    eglMakeCurrent(m_prevDisplay, m_prevDraw, m_prevRead, m_prevContext);
  else
    eglMakeCurrent(m_eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

  m_clientThread = std::thread::id();
  m_prevDisplay = EGL_NO_DISPLAY;
  m_prevDraw = EGL_NO_SURFACE;
  m_prevRead = EGL_NO_SURFACE;
  m_prevContext = EGL_NO_CONTEXT;
}

void CRenderBufferPoolFBO::DestroyContext()
{
  // DestroyContext() is broadcast to every pool, including those that never
  // created a context, so there is usually nothing to do here.
  if (m_eglContext == EGL_NO_CONTEXT)
    return;

  CLog::Log(LOGDEBUG, "RetroPlayer[RENDER]: Destroying shared FBO context");

  // Deleting this context's objects needs it current
  const bool bBound = BeginClientFrame();

  // Buffers own framebuffers and textures belonging to this context, so they
  // have to go while it is still current.
  Flush();

  if (bBound)
    EndClientFrame();

  eglDestroyContext(m_eglDisplay, m_eglContext);

  m_eglContext = EGL_NO_CONTEXT;
  m_eglDisplay = EGL_NO_DISPLAY;
}
