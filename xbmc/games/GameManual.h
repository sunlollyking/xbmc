/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include <string>
#include <vector>

class CFileItem;

namespace KODI::GAME
{

/*!
 * \ingroup games
 *
 * \brief Finds the manual that belongs to a game
 *
 * A manual is a PDF or a comic archive (.cbz, .cbr) that belongs to a game
 * file. It is looked for beside the game, and in a "manuals" folder next to it:
 *
 *     Sonic The Hedgehog (USA).md
 *     Sonic The Hedgehog (USA).pdf
 *     manuals/Sonic The Hedgehog (USA).cbz
 *
 * An exact name match is preferred and costs nothing but a few existence
 * checks. Only when that fails is the folder listed and a looser match tried,
 * ignoring the parenthesised region and revision tags that ROM naming
 * conventions add:
 *
 *     Sonic The Hedgehog 2 (World) (Rev A).md
 *     Sonic The Hedgehog 2.pdf
 *
 * The looser match stops there. Titles are not compared and nothing is
 * fuzzily scored, because a manual that is merely close is worse than no
 * manual - the player would read the wrong game's instructions without being
 * told.
 */
class CGameManual
{
public:
  /*!
   * \brief Get the manual belonging to a game
   *
   * Touches the filesystem, and may list a directory, so this does not belong
   * on the GUI thread when the game is on a network share.
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
   * \brief Derive where a game's manual could be, without checking for any
   *
   * Separated from the lookup so that the candidates can be reasoned about -
   * and tested - without touching the filesystem, which matters when the game
   * lives on a network share.
   *
   * \param gamePath The game's path
   *
   * \return The paths a manual could have, best first, or empty if none can
   *         be derived
   */
  static std::vector<std::string> BuildManualPaths(const std::string& gamePath);

  /*!
   * \brief Where a manual fetched for a game should be written
   *
   * The "manuals" folder beside the game, which is one of the places the
   * lookup already searches, so a fetched manual is found the same way one
   * put there by hand would be.
   *
   * A games folder is often read only - a share mounted for playing, or a
   * disc - so the caller is told whether it can be written to and can fall
   * back to somewhere in the profile.
   *
   * \param gamePath The game's path
   * \param[out] writable Whether the folder can be created and written to
   *
   * \return The path to write to, or empty if none can be derived
   */
  static std::string GetDownloadPath(const std::string& gamePath, bool& writable);

  /*!
   * \brief Reduce a name so that two spellings of the same title compare equal
   *
   * Lowercases, and drops parenthesised and bracketed tags along with the
   * punctuation and spacing around them, so that "Sonic The Hedgehog 2 (World)
   * (Rev A)" and "Sonic The Hedgehog 2" both reduce to the same thing.
   *
   * Exposed for the benefit of tests.
   *
   * \param name A filename with no extension
   */
  static std::string NormaliseName(const std::string& name);
};

} // namespace KODI::GAME
