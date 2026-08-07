/*
 *  Copyright (C) 2017-2024 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "RPProcessInfoEGL.h"

#include "ServiceBroker.h"
#include "windowing/WinSystem.h"
#include "windowing/linux/WinSystemEGL.h"

#include <EGL/egl.h>

using namespace KODI;
using namespace RETRO;

CRPProcessInfoEGL::CRPProcessInfoEGL(std::string platformName)
  : CRPProcessInfo(std::move(platformName))
{
}

HwProcedureAddress CRPProcessInfoEGL::GetHwProcedureAddress(const char* symbol)
{
  return static_cast<HwProcedureAddress>(eglGetProcAddress(symbol));
}

bool CRPProcessInfoEGL::HasHardwareRendering() const
{
  // The buffer pool a client renders into is an EGL context shared with the
  // one the window system is drawing with. Not every display stack has one:
  // X11 falls back to GLX where EGL is unavailable, and there is nothing to
  // share from.
  auto* winSystem =
      dynamic_cast<KODI::WINDOWING::LINUX::CWinSystemEGL*>(CServiceBroker::GetWinSystem());
  if (winSystem == nullptr)
    return false;

  return winSystem->GetEGLDisplay() != EGL_NO_DISPLAY;
}
