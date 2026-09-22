/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include <string>

class CDatabase;

namespace KODI
{
namespace GAME
{
/*!
 * \ingroup games
 *
 * \brief The videofilter table, which remembers how a game should be drawn
 *
 * One row says "this path uses this video filter". The path is either a game
 * or a folder holding games, which is what makes this worth having: a handheld
 * wants sharp integer scaling where a home console wants something that
 * imitates a CRT, and a collection is already sorted into a folder per system.
 *
 * A game's own filter overrides the folder it sits in. See FindPathDefault().
 *
 * The table owns its schema and its queries. It reaches the database it
 * belongs to through CDatabase's public interface only, so nothing here
 * depends on how the database holds its connection.
 */
class CVideoFilterTable
{
public:
  /*!
   * \brief Create the table's interface to the database holding it
   *
   * \param database The database this table lives in, which must outlive this
   */
  explicit CVideoFilterTable(CDatabase& database);
  ~CVideoFilterTable();

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
   * \brief Remember the video filter for a path
   *
   * \param path A game, or a folder holding games
   * \param videoFilter The filter, or empty to stop remembering this path
   *
   * \return True if the table was changed
   */
  bool SetVideoFilter(const std::string& path, const std::string& videoFilter);

  /*!
   * \brief The video filter stored for exactly this path
   *
   * Does not consider the folders above it; use GetVideoFilterForGame() to
   * find what actually applies to a game.
   *
   * \return The filter, or empty if this path has none
   */
  std::string GetVideoFilter(const std::string& path);

  /*!
   * \brief The video filter that applies to a game
   *
   * The game's own filter, or failing that the nearest folder above it with
   * one.
   *
   * \return The filter, or empty if nothing applies
   */
  std::string GetVideoFilterForGame(const std::string& path);

  /*!
   * \brief Stop remembering a path's video filter
   *
   * \return True if the table was changed
   */
  bool ClearVideoFilter(const std::string& path);

private:
  CDatabase& m_database;
};
} // namespace GAME
} // namespace KODI
