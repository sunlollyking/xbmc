/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "games/database/GameDatabase.h"

#include <memory>

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
 * CProgramThumbLoader finds a folder's art and the .tbn a file may carry; the
 * per-type artwork a game collection holds is found here. The info tag
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
  void OnLoaderFinish() override;

  /*!
   * \brief Attach the artwork the library holds for a game or platform
   *
   * Art found beside the file wins; the library fills in the types it lacks.
   *
   * \return True if any artwork was attached
   */
  bool LoadLibraryArt(CFileItem& item);

  /*!
   * \brief Attach the artwork sitting beside a game
   *
   * Looks for "<game>-<type>.jpg" and the same as a .png, which is the naming
   * the video library has used since art stopped being one image per item.
   *
   * \return True if any artwork was attached
   */
  static bool LoadLocalArt(CFileItem& item);

private:
  std::unique_ptr<CGameDatabase> m_database;
};
} // namespace GAME
} // namespace KODI
