/*
 *  Copyright (C) 2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "RPStreamManager.h"

#include "IRetroPlayerStream.h"
#include "RetroPlayerAudio.h"
#include "RetroPlayerRendering.h"
#include "RetroPlayerVideo.h"
#include "cores/RetroPlayer/process/RPProcessInfo.h"
#include "cores/RetroPlayer/rendering/RPRenderManager.h"

using namespace KODI;
using namespace RETRO;

CRPStreamManager::CRPStreamManager(CRPRenderManager& renderManager, CRPProcessInfo& processInfo)
  : m_renderManager(renderManager),
    m_processInfo(processInfo)
{
}

void CRPStreamManager::EnableAudio(bool bEnable)
{
  if (m_audioStream != nullptr)
    m_audioStream->Enable(bEnable);
}

void CRPStreamManager::EnableVideo(bool bEnable)
{
  if (m_videoStream != nullptr)
    m_videoStream->Enable(bEnable);

  if (m_renderingStream != nullptr)
    m_renderingStream->Enable(bEnable);
}

StreamPtr CRPStreamManager::CreateStream(StreamType streamType)
{
  switch (streamType)
  {
    case StreamType::AUDIO:
    {
      // Save pointer to audio stream
      m_audioStream = new CRetroPlayerAudio(m_processInfo);

      return StreamPtr(m_audioStream);
    }
    case StreamType::VIDEO:
    case StreamType::SW_BUFFER:
    {
      // Save pointer to video stream
      m_videoStream = new CRetroPlayerVideo(m_renderManager, m_processInfo);

      return StreamPtr(m_videoStream);
    }
    case StreamType::HW_BUFFER:
    {
      // Save pointer to rendering stream
      m_renderingStream = new CRetroPlayerRendering(m_renderManager, m_processInfo);

      return StreamPtr(m_renderingStream);
    }
    default:
      break;
  }

  return StreamPtr();
}

void CRPStreamManager::CloseStream(StreamPtr stream)
{
  if (stream)
  {
    if (stream.get() == m_audioStream)
      m_audioStream = nullptr;
    else if (stream.get() == m_videoStream)
      m_videoStream = nullptr;
    else if (stream.get() == m_renderingStream)
      m_renderingStream = nullptr;

    stream->CloseStream();
  }
}

void CRPStreamManager::SetVideoFps(float fps)
{
  m_processInfo.SetVideoFps(fps);
}

bool CRPStreamManager::BeginClientFrame()
{
  return m_renderManager.BeginClientFrame();
}

void CRPStreamManager::EndClientFrame()
{
  m_renderManager.EndClientFrame();
}

HwProcedureAddress CRPStreamManager::GetHwProcedureAddress(const char* symbol)
{
  return m_processInfo.GetHwProcedureAddress(symbol);
}

bool CRPStreamManager::HasHardwareRendering() const
{
  return m_processInfo.HasHardwareRendering();
}
