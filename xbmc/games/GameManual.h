/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include <map>
#include <set>
#include <string>

namespace KODI::GAME
{

/*!
 * \ingroup games
 *
 * \brief How a game's manual is recognised
 *
 * A manual is a PDF or a comic archive (.cbz, .cbr) that sits beside its game,
 * or in a "manuals" folder next to it. The two are matched by name, ignoring
 * the parenthesised region and revision tags that ROM naming conventions add:
 *
 *     Sonic The Hedgehog 2 (World) (Rev A).md
 *     manuals/Sonic The Hedgehog 2.pdf
 *
 * Titles are not compared and nothing is fuzzily scored, because a manual
 * that is merely close is worse than no manual - the player would read the
 * wrong game's instructions without being told.
 */
class CGameManual
{
public:
  /*!
   * \brief Reduce a name so that two spellings of the same title compare equal
   *
   * Lowercases, and drops parenthesised, bracketed and braced tags along with
   * the punctuation and spacing around them, so that "Sonic The Hedgehog 2
   * (World) (Rev A)" and "Sonic The Hedgehog 2" both reduce to the same thing.
   *
   * Exposed for the benefit of tests.
   *
   * \param name A filename with no extension
   */
  static std::string NormaliseName(const std::string& name);
};

/*!
 * \ingroup games
 *
 * \brief Which games in a listing have a manual sitting beside them
 *
 * The manuals in a real collection carry different region and revision tags
 * from the games they belong to, so settling it needs the folder's listing.
 * Each folder is read once here and remembered, which makes a listing cost one
 * read per folder rather than one per game.
 *
 * Held for as long as the listing being built and then discarded, so a manual
 * added while Kodi is running is picked up the next time the folder is opened.
 */
class CManualIndex
{
public:
  /*!
   * \brief Whether a manual belonging to this game is on disk
   *
   * \param gamePath The game's own file path
   */
  bool HasManual(const std::string& gamePath);

private:
  //! The reduced names of the manuals in a folder, reading it if not yet seen
  const std::set<std::string>& Names(const std::string& folder);

  std::map<std::string, std::set<std::string>> m_folders;
};

} // namespace KODI::GAME
