/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "dbwrappers/Database.h"

#include <string>

namespace KODI
{
namespace GAME
{
/*!
 * \ingroup games
 *
 * \brief Remembers which emulator to open a game with.
 *
 * One row says "this path uses this emulator". The path is either a game or a
 * folder holding games, which is what makes per-game overrides fall out for
 * free: a game is looked up first, and only if it has no emulator of its own
 * is the folder above it asked, and the folder above that.
 */
class CGameClientDatabase : public CDatabase
{
public:
  CGameClientDatabase();
  ~CGameClientDatabase() override;

  bool Open() override;

  /*!
   * \brief Remember the emulator to open a game or folder with
   *
   * \param path The game or folder
   * \param gameClient The add-on ID of the emulator
   *
   * \return True if the emulator was stored
   */
  bool SetGameClient(const std::string& path, const std::string& gameClient);

  /*!
   * \brief The emulator remembered for exactly this path
   *
   * \param path The game or folder
   *
   * \return The add-on ID, or empty if this path has none of its own
   */
  std::string GetGameClient(const std::string& path);

  /*!
   * \brief The emulator remembered for a game, or for the nearest folder above it
   *
   * \param path The game
   *
   * \return The add-on ID, or empty if neither the game nor any folder above
   *         it has one
   */
  std::string GetGameClientForGame(const std::string& path);

  /*!
   * \brief Forget the emulator remembered for a game or folder
   *
   * \param path The game or folder
   *
   * \return True if the path is no longer remembered
   */
  bool ClearGameClient(const std::string& path);

protected:
  // implementation of CDatabase
  void CreateTables() override;
  void CreateAnalytics() override;
  int GetSchemaVersion() const override { return 1; }
  const char* GetBaseDBName() const override { return "Games"; }
};
} // namespace GAME
} // namespace KODI
