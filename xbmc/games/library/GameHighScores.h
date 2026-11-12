/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "games/library/HighScoreDecoder.h"

#include <string>
#include <vector>

namespace KODI
{
namespace GAME
{
/*!
 * \ingroup games
 * \brief The high scores an arcade game saved, read when it closes
 *
 * Arcade emulators keep a game's high score table in a file of the game's
 * own layout, "<set>.hi" in their save folder. How to read each game's is
 * described by resource.games.hiscores, an add-on of hi2txt's descriptions,
 * so a game whose layout it doesn't describe has no scores to show.
 */
class CGameHighScores
{
public:
  //! The add-on holding a description of each game's high score file
  static constexpr const char* DESCRIPTIONS_ADDON = "resource.games.hiscores";

  /*!
   * \brief Read the scores a game client saved for a game, and keep them with
   *        the game in the library
   *
   * \param gamePath The game's file, as the library has it
   * \param gameClient The add-on that played it
   */
  static void Record(const std::string& gamePath, const std::string& gameClient);

  /*!
   * \brief Read a high score file with the description of a game
   *
   * \param description The game's description
   * \param dtd hi2txt's DTD, which names the characters descriptions use
   * \param load Loads another game's description, for a clone
   * \param fileName The file's name, such as "galaga.hi"
   * \param data The file
   * \param[out] table The scores, as the game shows them
   */
  static bool Decode(const std::string& description,
                     const std::string& dtd,
                     const CHighScoreDecoder::DescriptionLoader& load,
                     const std::string& fileName,
                     const std::vector<uint8_t>& data,
                     HighScoreTable& table);

  //! \brief The best score and who set it, e.g. "54300 · ABC"
  static std::string Best(const HighScoreTable& table);

  //! \brief The table as lines of text, headed by its columns
  static std::string Text(const HighScoreTable& table);

  //! \brief The table as JSON, for the library to keep
  static std::string Serialize(const HighScoreTable& table);

  //! \brief A table kept by Serialize(), or an empty one
  static HighScoreTable Deserialize(const std::string& json);
};
} // namespace GAME
} // namespace KODI
