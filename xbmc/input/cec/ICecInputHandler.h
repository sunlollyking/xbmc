/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

class CKey;

namespace KODI
{
namespace CEC
{
class ICecInputHandler
{
public:
  virtual ~ICecInputHandler() = default;

  /*!
   * \brief Try to get a keypress from a CEC peripheral
   */
  virtual bool GetCecKey(CKey& key) = 0;
};
} // namespace CEC
} // namespace KODI
