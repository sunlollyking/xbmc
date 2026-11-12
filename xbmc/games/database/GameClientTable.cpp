/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "GameClientTable.h"

#include "PathDefaults.h"
#include "dbwrappers/Database.h"
#include "utils/log.h"

using namespace KODI;
using namespace GAME;

CGameClientTable::CGameClientTable(CDatabase& database) : m_database(database)
{
}

CGameClientTable::~CGameClientTable() = default;

void CGameClientTable::Create()
{
  CLog::Log(LOGINFO, "GAME: Creating gameclient table");

  m_database.ExecuteQuery("CREATE TABLE gameclient ("
                          "idPath integer primary key,"
                          "path text,"
                          "gameClient text)");
}

void CGameClientTable::CreateAnalytics()
{
  CLog::Log(LOGINFO, "GAME: Creating gameclient index");

  // Unique: a path has one emulator, so storing a second replaces the first
  m_database.ExecuteQuery("CREATE UNIQUE INDEX idxGameClientPath ON gameclient(path)");
}

void CGameClientTable::UpdateTables(int version)
{
  // No schema changes yet
  (void)version;
}

bool CGameClientTable::SetGameClient(const std::string& path, const std::string& gameClient)
{
  if (path.empty())
    return false;

  // No emulator means the path is no longer remembered, rather than remembered
  // as nothing
  if (gameClient.empty())
    return ClearGameClient(path);

  const std::string sql =
      m_database.PrepareSQL("REPLACE INTO gameclient (path, gameClient) VALUES ('%s', '%s')",
                            path.c_str(), gameClient.c_str());

  if (!m_database.ExecuteQuery(sql))
  {
    CLog::Log(LOGERROR, "GAME: Failed to remember emulator {} for {}", gameClient, path);
    return false;
  }

  return true;
}

std::string CGameClientTable::GetGameClient(const std::string& path)
{
  if (path.empty())
    return "";

  const std::string sql =
      m_database.PrepareSQL("SELECT gameClient FROM gameclient WHERE path='%s'", path.c_str());

  return m_database.GetSingleValue(sql);
}

std::string CGameClientTable::GetGameClientForGame(const std::string& path)
{
  return FindPathDefault(path, [this](const std::string& p) { return GetGameClient(p); });
}

bool CGameClientTable::ClearGameClient(const std::string& path)
{
  if (path.empty())
    return false;

  const std::string sql =
      m_database.PrepareSQL("DELETE FROM gameclient WHERE path='%s'", path.c_str());

  if (!m_database.ExecuteQuery(sql))
  {
    CLog::Log(LOGERROR, "GAME: Failed to forget the emulator for {}", path);
    return false;
  }

  return true;
}
