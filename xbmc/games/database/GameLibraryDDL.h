/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

class CDatabase;

namespace KODI
{
namespace GAME
{
/*!
 * \ingroup games
 *
 * \brief The schema of the game library
 *
 * The library is a dozen tables that only make sense together, so unlike the
 * per-path tables beside it they share one definition: what the tables are,
 * what indexes, views and triggers they need, and how an older database is
 * brought forward.
 *
 * Shipped columns are never renamed or retyped. A change is a new column, a
 * new table or a new view, added here and in UpdateTables() for the version it
 * arrived in, so a database written by one version reads in the next and a
 * shared MySQL database keeps working for every client on it.
 */
class CGameLibraryDDL
{
public:
  /*!
   * \brief The schema version the library first shipped in
   */
  static constexpr int FIRST_VERSION = 3;

  /*!
   * \brief Create every library table
   */
  static void CreateTables(CDatabase& db);

  /*!
   * \brief Create the library's indexes, views and triggers
   *
   * Everything here is dropped and recreated on every upgrade, so it must
   * never hold data.
   */
  static void CreateAnalytics(CDatabase& db);

  /*!
   * \brief Bring the library forward from an older schema version
   *
   * \param version The version the database was written by
   */
  static void UpdateTables(CDatabase& db, int version);

private:
  static void CreateIndexes(CDatabase& db);
  static void CreateViews(CDatabase& db);
  static void CreateTriggers(CDatabase& db);
  static void CreateLookupTable(CDatabase& db, const char* table, const char* idColumn);
  static void CreateLookupIndexes(CDatabase& db, const char* table, const char* idColumn);
};
} // namespace GAME
} // namespace KODI
