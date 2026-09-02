/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "ThumbLoader.h"

namespace KODI
{
namespace GAME
{
/*!
 * \ingroup games
 *
 * \brief Loads the artwork and the info tag a game collection carries
 *
 * The artwork is whatever CProgramThumbLoader finds beside the file, which is
 * the same local artwork every other file listing in Kodi shows. The info tag
 * is read here too, on the same background thread, because the only other
 * place it is filled is for the game being played, and a list needs it before
 * anything is playing.
 */
class CGameThumbLoader : public CProgramThumbLoader
{
public:
  CGameThumbLoader() = default;
  ~CGameThumbLoader() override = default;

  // Implementation of CBackgroundInfoLoader
  bool LoadItemCached(CFileItem* item) override;
};
} // namespace GAME
} // namespace KODI
