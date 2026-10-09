/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

class CFileItem;

namespace KODI
{
namespace GAME
{
class CGameInfoTag;

/*!
 * \ingroup games
 *
 * \brief Fill a game's info tag from the NFO file beside it
 *
 * Nothing scrapes games, so the only description a collection has is the one
 * it already carries. The file is looked for where the artwork is: beside a
 * game as <name>.nfo, and inside a folder as folder.nfo, so a platform folder
 * can describe the system it holds.
 */
class CGameInfoTagLoader
{
public:
  /*!
   * \brief Load the NFO belonging to an item into a tag
   *
   * \param item The game or folder to look beside
   * \param tag The tag to fill
   *
   * \return True if an NFO was read, false if there was none or it was unusable
   */
  static bool Load(const CFileItem& item, CGameInfoTag& tag);

  /*!
   * \brief Write a game's NFO beside the file it describes
   *
   * \return True if the file was written
   */
  static bool Save(const CFileItem& item, const CGameInfoTag& tag);

  /*!
   * \brief Whether an item has an NFO to read
   *
   * Asked before loading, because reading the tag creates one on the item and
   * anything carrying a game tag answers CFileItem::IsGame(). A listing is
   * mostly files that are not games, and they must not start claiming to be.
   *
   * \param item The game or folder to look beside
   *
   * \return True if an NFO exists for the item
   */
  static bool HasNFO(const CFileItem& item);
};
} // namespace GAME
} // namespace KODI
