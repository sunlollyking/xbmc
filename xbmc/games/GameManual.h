/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include <string>

class CFileItem;

namespace KODI::GAME
{

/*!
 * \ingroup games
 *
 * \brief Finds the manual that belongs to a game
 *
 * A manual is a PDF sitting next to the game file, with exactly the same name:
 *
 *     Sonic The Hedgehog (USA).md
 *     Sonic The Hedgehog (USA).pdf
 *
 * The match is deliberately exact. Nothing is normalised, no region tag is
 * stripped and no titles are compared, because a manual that looks close
 * enough is worse than no manual - the player would be reading the wrong
 * revision without being told.
 *
 * This is kept behind a small interface so that where a manual comes from can
 * change later, once games have a database to hang metadata off, without the
 * viewer or the OSD needing to know.
 */
class CGameManual
{
public:
  /*!
   * \brief Get the manual belonging to a game
   *
   * \param gamePath The game's path, in any form Kodi can resolve
   *
   * \return The manual's path, or an empty string if the game has none
   */
  static std::string GetManualPath(const std::string& gamePath);

  /*!
   * \brief Get the manual belonging to a game
   *
   * \param item The game
   *
   * \return The manual's path, or an empty string if the game has none
   */
  static std::string GetManualPath(const CFileItem& item);

  /*!
   * \brief Derive where a game's manual would be, without checking for it
   *
   * Separated from the lookup so that the path can be reasoned about - and
   * tested - without touching the filesystem, which matters when the game
   * lives on a network share.
   *
   * \param gamePath The game's path
   *
   * \return The path the manual would have, or empty if one can't be derived
   */
  static std::string BuildManualPath(const std::string& gamePath);
};

} // namespace KODI::GAME
