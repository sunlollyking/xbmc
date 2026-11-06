/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "games/GameVideoSettings.h"

#include <string>

class CDatabase;

namespace KODI
{
namespace GAME
{
/*!
 * \ingroup games
 * \brief The table of how games are drawn, kept per game and per folder
 */
class CVideoSettingsTable
{
public:
  /*!
   * \brief Create the table's interface to the database holding it
   *
   * \param database The database this table lives in, which must outlive this
   */
  explicit CVideoSettingsTable(CDatabase& database);
  ~CVideoSettingsTable();

  /*!
   * \brief Create the table
   */
  void Create();

  /*!
   * \brief Create the table's indices
   */
  void CreateAnalytics();

  /*!
   * \brief Bring the table up to the current schema version
   *
   * \param version The schema version being upgraded from
   */
  void UpdateTables(int version);

  /*!
   * \brief Remember the video settings for a path
   *
   * \param path A game, or a folder holding games
   * \param settings The settings, or none set to stop remembering this path
   *
   * \return True if the table was changed
   */
  bool SetVideoSettings(const std::string& path, const GameVideoSettings& settings);

  /*!
   * \brief The video settings stored for exactly this path
   */
  GameVideoSettings GetVideoSettings(const std::string& path);

  /*!
   * \brief The video settings the folders above a path give it
   *
   * Each setting comes from the nearest folder that has it.
   */
  GameVideoSettings GetFolderVideoSettings(const std::string& path);

  /*!
   * \brief Remember the video filter for a path, keeping its other settings
   *
   * \param path A game, or a folder holding games
   * \param videoFilter The filter, or empty to stop remembering one
   *
   * \return True if the table was changed
   */
  bool SetVideoFilter(const std::string& path, const std::string& videoFilter);

  /*!
   * \brief The video filter stored for exactly this path
   *
   * \return The filter, or empty if this path has none
   */
  std::string GetVideoFilter(const std::string& path);

private:
  bool ClearVideoSettings(const std::string& path);

  CDatabase& m_database;
};
} // namespace GAME
} // namespace KODI
