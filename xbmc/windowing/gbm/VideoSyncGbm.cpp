/*
 *  Copyright (C) 2005-2021 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "VideoSyncGbm.h"

#include "ServiceBroker.h"
#include "cores/VideoPlayer/VideoReferenceClock.h"
#include "threads/Thread.h"
#include "utils/TimeUtils.h"
#include "utils/log.h"
#include "windowing/GraphicContext.h"
#include "windowing/WinSystem.h"
#include "windowing/gbm/WinSystemGbm.h"

#include <assert.h>
#include <chrono>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

namespace
{
/*!
 * \brief Express a DRM vblank timestamp in the reference clock's time base
 *
 * DRM reports vblank timestamps on CLOCK_MONOTONIC while CurrentHostCounter(),
 * and therefore CVideoReferenceClock, uses CLOCK_MONOTONIC_RAW. Read both
 * clocks here rather than applying an offset sampled earlier: the elapsed time
 * since the vblank then cancels, and the two clocks do not tick at the same
 * rate so any offset kept from before is already out of date.
 */
uint64_t MonotonicToHostCounter(uint64_t ns)
{
  struct timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
    return CurrentHostCounter();

  const uint64_t now = static_cast<uint64_t>(ts.tv_sec) * 1000000000ULL + ts.tv_nsec;
  return CurrentHostCounter() - (now - ns);
}

uint32_t GetCrtcId(CWinSystemBase* winSystem)
{
  auto winSystemGbm = dynamic_cast<KODI::WINDOWING::GBM::CWinSystemGbm*>(winSystem);
  const auto drm = winSystemGbm ? winSystemGbm->GetDrm() : nullptr;
  const auto crtc = drm ? drm->GetCrtc() : nullptr;
  return crtc ? crtc->GetCrtcId() : 0;
}
} // namespace

CVideoSyncGbm::CVideoSyncGbm(CVideoReferenceClock* clock)
  : CVideoSync(clock), m_winSystem(CServiceBroker::GetWinSystem())
{
  if (!m_winSystem)
    throw std::runtime_error("window system not available");
}

bool CVideoSyncGbm::Setup()
{
  m_abort = false;
  m_winSystem->Register(this);
  CLog::Log(LOGDEBUG, "CVideoSyncGbm::{} setting up", __FUNCTION__);

  auto winSystemGbm = dynamic_cast<KODI::WINDOWING::GBM::CWinSystemGbm*>(m_winSystem);
  if (!winSystemGbm)
  {
    CLog::Log(LOGWARNING, "CVideoSyncGbm::{}: failed to get winSystem", __FUNCTION__);
    return false;
  }

  auto drm = winSystemGbm->GetDrm();
  if (!drm)
  {
    CLog::Log(LOGWARNING, "CVideoSyncGbm::{}: failed to get drm", __FUNCTION__);
    return false;
  }

  auto crtc = drm->GetCrtc();
  if (!crtc)
  {
    CLog::Log(LOGWARNING, "CVideoSyncGbm::{}: failed to get crtc", __FUNCTION__);
    return false;
  }

  uint64_t ns = 0;
  m_crtcId = crtc->GetCrtcId();
  m_fd = drm->GetFileDescriptor();
  int s = drmCrtcGetSequence(m_fd, m_crtcId, &m_sequence, &ns);
  if (s != 0)
  {
    CLog::Log(LOGWARNING, "CVideoSyncGbm::{}: drmCrtcGetSequence failed ({})", __FUNCTION__, s);
    return false;
  }

  CLog::Log(LOGINFO, "CVideoSyncGbm::{}: opened (fd:{} crtc:{} seq:{} ns:{})", __FUNCTION__, m_fd,
            m_crtcId, m_sequence, ns);
  return true;
}

void CVideoSyncGbm::Run(CEvent& stopEvent)
{
  /* This shouldn't be very busy and timing is important so increase priority */
  CThread::GetCurrentThread()->SetPriority(ThreadPriority::ABOVE_NORMAL);

  if (m_fd < 0)
  {
    CLog::Log(LOGWARNING, "CVideoSyncGbm::{}: failed to open device ({})", __FUNCTION__, m_fd);
    return;
  }
  CLog::Log(LOGDEBUG, "CVideoSyncGbm::{}: started {}", __FUNCTION__, m_fd);

  bool failing = false;
  bool rebound = false;
  while (!stopEvent.Signaled() && !m_abort)
  {
    uint64_t sequence = 0, ns = 0;
    usleep(1000);
    int s = drmCrtcGetSequence(m_fd, m_crtcId, &sequence, &ns);
    if (s != 0)
    {
      // Vblank is off while the CRTC is reconfigured, as when the output
      // switches to HDR, so wait for it to come back. The modeset may also
      // have moved the output to another CRTC.
      if (!failing)
        CLog::Log(LOGWARNING, "CVideoSyncGbm::{}: drmCrtcGetSequence failed ({}), waiting",
                  __FUNCTION__, s);
      failing = true;
      if (stopEvent.Wait(std::chrono::milliseconds(100)))
        break;

      const uint32_t crtcId = GetCrtcId(m_winSystem);
      if (crtcId != 0 && crtcId != m_crtcId)
      {
        CLog::Log(LOGINFO, "CVideoSyncGbm::{}: following the output to crtc {}", __FUNCTION__,
                  crtcId);
        m_crtcId = crtcId;
        rebound = true;
      }
      continue;
    }

    if (failing)
    {
      CLog::Log(LOGINFO, "CVideoSyncGbm::{}: drmCrtcGetSequence works again", __FUNCTION__);
      failing = false;
    }

    // Another CRTC counts its own vblanks, so start counting from here
    if (rebound)
    {
      m_sequence = sequence;
      rebound = false;
      continue;
    }

    if (sequence == m_sequence)
      continue;

    // Zero is how the kernel marks a timestamp it could not determine
    // e.g. during a modeset
    if (ns == 0)
      continue;

    m_refClock->UpdateClock(sequence - m_sequence, MonotonicToHostCounter(ns));
    m_sequence = sequence;
  }
}

void CVideoSyncGbm::Cleanup()
{
  CLog::Log(LOGDEBUG, "CVideoSyncGbm::{}: cleaning up", __FUNCTION__);
  m_winSystem->Unregister(this);
}

float CVideoSyncGbm::GetFps()
{
  m_fps = m_winSystem->GetGfxContext().GetFPS();
  CLog::Log(LOGDEBUG, "CVideoSyncGbm::{}: fps:{}", __FUNCTION__, m_fps);
  return m_fps;
}

void CVideoSyncGbm::OnResetDisplay()
{
  m_abort = true;
}

void CVideoSyncGbm::RefreshChanged()
{
  if (m_fps != m_winSystem->GetGfxContext().GetFPS())
    m_abort = true;
}
