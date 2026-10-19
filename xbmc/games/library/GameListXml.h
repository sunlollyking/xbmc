/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include <map>
#include <string>
#include <vector>

namespace KODI
{
namespace GAME
{
/*!
 * \ingroup games
 * \brief What one <game> element of a gamelist.xml says
 */
struct GameListEntry
{
  std::string path; // absolute, as resolved against the folder
  std::string title;
  std::string overview;
  std::string developer;
  std::string publisher;
  std::vector<std::string> genres;
  std::string releaseDate; // YYYY-MM-DD
  int year{0};
  int playersMin{0};
  int playersMax{0};
  float rating{0.0f}; // 0..10, converted from the 0..1 the file uses
  std::string region; // written in full
  std::vector<std::string> languages;
  bool favourite{false};
  bool hidden{false};
  bool kidGame{false};
  int playCount{0};
  std::string lastPlayed;
  std::string trailer;
  std::map<std::string, std::string> art; // Kodi art types to absolute paths
};

/*!
 * \ingroup games
 *
 * \brief Read the gamelist.xml an EmulationStation-style front end leaves
 *
 * Batocera, RetroBat, ES-DE and EmulationStation all keep one of these beside
 * a platform's games, holding what their own scrape found and what the player
 * has done since: names, descriptions, art, favourites and play counts. A
 * collection that arrives with one has already been curated, so the library
 * reads it and lets it stand over what a scraper says.
 *
 * Paths inside the file are relative to the folder holding it, in the "./"
 * form those front ends write.
 */
class CGameListXml
{
public:
  /*!
   * \brief Read the gamelist.xml of a folder, if it has one
   *
   * \param folder The folder being scanned
   * \param entries Filled with one entry per game, keyed by absolute path
   *
   * \return True when a file was read, false when there is none or it is unreadable
   */
  static bool Load(const std::string& folder, std::map<std::string, GameListEntry>& entries);
};
} // namespace GAME
} // namespace KODI
