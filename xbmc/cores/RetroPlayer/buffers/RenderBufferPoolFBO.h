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
#pragma once

#include "cores/RetroPlayer/buffers/BaseRenderBufferPool.h"

#include <thread>

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include "system_gl.h"

namespace KODI
{
namespace RETRO
{
class CRenderContext;

class CRenderBufferPoolFBO : public CBaseRenderBufferPool
{
public:
  CRenderBufferPoolFBO(CRenderContext& context);
  ~CRenderBufferPoolFBO() override;

  // implementation of IRenderBufferPool via CRenderBufferPoolSysMem
  bool IsCompatible(const CRenderVideoSettings& renderSettings) const override;

  // implementation of CBaseRenderBufferPool via CRenderBufferPoolSysMem
  IRenderBuffer* CreateRenderBuffer(void* header = nullptr) override;
  bool ConfigureInternal() override;

  bool CreateContext(const HwContextProperties& properties) override;
  bool BeginClientFrame() override;
  void EndClientFrame() override;
  void DestroyContext() override;

protected:

  // Construction parameters
  CRenderContext& m_context;

  // Configuration parameters
  HwContextProperties m_contextProperties;

  // A context is current on one thread at a time, and the thread a client
  // renders on is whichever one it happens to use. Track who holds it, and what
  // they had before, so it can be handed back.
  unsigned int m_clientFrameDepth{0};
  std::thread::id m_clientThread;
  EGLDisplay m_prevDisplay{EGL_NO_DISPLAY};
  EGLSurface m_prevDraw{EGL_NO_SURFACE};
  EGLSurface m_prevRead{EGL_NO_SURFACE};
  EGLContext m_prevContext{EGL_NO_CONTEXT};
  EGLDisplay m_eglDisplay = EGL_NO_DISPLAY;
  EGLConfig m_eglConfig;
  EGLContext m_eglContext = EGL_NO_CONTEXT;

private:
};
} // namespace RETRO
} // namespace KODI
