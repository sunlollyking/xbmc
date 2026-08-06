/*
 *  Copyright (C) 2017-2024 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "RetroPlayerRendering.h"

#include "cores/RetroPlayer/process/RPProcessInfo.h"
#include "cores/RetroPlayer/rendering/RPRenderManager.h"
#include "cores/RetroPlayer/rendering/RenderTranslator.h"
#include "utils/log.h"

using namespace KODI;
using namespace RETRO;

CRetroPlayerRendering::CRetroPlayerRendering(CRPRenderManager& renderManager,
                                             CRPProcessInfo& processInfo)
  : m_renderManager(renderManager),
    m_processInfo(processInfo)
{
  CLog::Log(LOGDEBUG, "RetroPlayer[RENDERING]: Initializing rendering");
}

CRetroPlayerRendering::~CRetroPlayerRendering()
{
  CLog::Log(LOGDEBUG, "RetroPlayer[RENDERING]: Deinitializing rendering");

  CloseStream();
  m_renderManager.Deinitialize();
}

bool CRetroPlayerRendering::OpenStream(const StreamProperties& properties)
{
  m_hwProperties =
      std::make_unique<HwFramebufferProperties>(static_cast<const HwFramebufferProperties&>(properties));

  CLog::Log(LOGDEBUG, "RetroPlayer[RENDERING]: Opening rendering stream");

  // game_stream_hw_framebuffer_properties carries no geometry, so the frame
  // size is unknown until the core asks for a framebuffer. Configuration is
  // deferred to the first GetStreamBuffer() call, which is handed the size the
  // core wants to render at.
  m_bOpen = true;

  return true;
}

void CRetroPlayerRendering::CloseStream()
{
  if (!m_bOpen)
    return;

  CLog::Log(LOGDEBUG, "RetroPlayer[RENDERING]: Closing rendering stream");

  // Release the shared context and the framebuffer it owns. The pools defer
  // the actual teardown to the game loop thread, which is the only thread
  // allowed to touch that context.
  m_renderManager.DestroyContext();

  m_width = 0;
  m_height = 0;
  m_hwProperties.reset();
  m_bOpen = false;
}

bool CRetroPlayerRendering::GetStreamBuffer(unsigned int width,
                                            unsigned int height,
                                            StreamBuffer& buffer)
{
  if (!m_bOpen)
    return false;

  HwFramebufferBuffer& hwBuffer = static_cast<HwFramebufferBuffer&>(buffer);

  if (!Configure(width, height))
  {
    hwBuffer.framebuffer = 0;
    return false;
  }

  hwBuffer.framebuffer = m_renderManager.GetCurrentFramebuffer(width, height);

  // Returning a framebuffer of 0 would send the core's drawing to the default
  // framebuffer, so report failure instead and let it try again next frame.
  return hwBuffer.framebuffer != 0;
}

bool CRetroPlayerRendering::Configure(unsigned int width, unsigned int height)
{
  if (width == 0 || height == 0)
  {
    CLog::Log(LOGERROR, "RetroPlayer[RENDERING]: Core asked for a {}x{} framebuffer", width, height);
    return false;
  }

  // Already configured at this size
  if (m_width == width && m_height == height)
    return true;

  // Hardware-rendered frames live in a GPU framebuffer and are never read back
  // to system memory, so they carry no pixel format. The FBO buffer pool keys
  // off this to tell itself apart from the software pools.
  const AVPixelFormat pixelFormat = AV_PIX_FMT_NONE;
  const float displayAspectRatio = 0.0f; // 0.0f means square pixels

  CLog::Log(LOGDEBUG, "RetroPlayer[RENDERING]: Configuring rendering stream - width {}, height {}",
            width, height);

  m_processInfo.SetVideoPixelFormat(pixelFormat);
  m_processInfo.SetVideoDimensions(width, height);

  if (!m_renderManager.Configure(pixelFormat, width, height, displayAspectRatio, width, height))
  {
    CLog::Log(LOGERROR, "RetroPlayer[RENDERING]: Failed to configure render manager");
    return false;
  }

  if (!m_renderManager.Create(width, height))
  {
    CLog::Log(LOGERROR, "RetroPlayer[RENDERING]: Failed to create rendering resources");
    return false;
  }

  CLog::Log(LOGDEBUG, "RetroPlayer[RENDERING]: Render manager configured");

  m_width = width;
  m_height = height;

  return true;
}

void CRetroPlayerRendering::AddStreamData(const StreamPacket& packet)
{
  // This is left here in case anything gets added to the api in the future
  [[maybe_unused]] const HwFramebufferPacket& hwPacket =
      static_cast<const HwFramebufferPacket&>(packet);

  m_renderManager.RenderFrame();
}
