/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "GameDatabase.h"

#include "DatabaseTypes.h"
#include "GameLibraryDDL.h"
#include "dbwrappers/dataset.h"
#include "utils/log.h"

#include <chrono>

using namespace KODI;
using namespace GAME;

CGameDatabase::CGameDatabase() : CDatabase(DATABASE::TYPE_GAMES)
{
}

CGameDatabase::~CGameDatabase() = default;

bool CGameDatabase::Open()
{
  return CDatabase::Open();
}

void CGameDatabase::CreateTables()
{
  m_gameClients.Create();
  m_videoFilters.Create();
  CGameLibraryDDL::CreateTables(*this);
}

void CGameDatabase::CreateAnalytics()
{
  m_gameClients.CreateAnalytics();
  m_videoFilters.CreateAnalytics();
  CGameLibraryDDL::CreateAnalytics(*this);
}

void CGameDatabase::UpdateTables(int version)
{
  m_gameClients.UpdateTables(version);
  m_videoFilters.UpdateTables(version);
  CGameLibraryDDL::UpdateTables(*this, version);
}

int CGameDatabase::RunQuery(const std::string& sql)
{
  const auto start = std::chrono::steady_clock::now();

  int rows = -1;
  if (m_pDS->query(sql))
  {
    rows = m_pDS->num_rows();
    if (rows == 0)
      m_pDS->close();
  }

  const auto duration =
      std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);
  CLog::LogFC(LOGDEBUG, LOGDATABASE, "took {} ms for {} items query: {}", duration.count(), rows,
              sql);

  return rows;
}

int CGameDatabase::AddLookup(const std::string& table,
                             const std::string& idColumn,
                             const std::string& name,
                             const std::string& nameColumn)
{
  if (name.empty())
    return -1;

  try
  {
    const int id = GetSingleValueInt(PrepareSQL("SELECT %s FROM %s WHERE %s = '%s'",
                                                idColumn.c_str(), table.c_str(),
                                                nameColumn.c_str(), name.c_str()));
    if (id > 0)
      return id;

    m_pDS->exec(PrepareSQL("INSERT INTO %s (%s) VALUES ('%s')", table.c_str(), nameColumn.c_str(),
                           name.c_str()));
    return static_cast<int>(m_pDS->lastinsertid());
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "GAME: Failed to add '{}' to {}", name, table);
  }
  return -1;
}

void CGameDatabase::SetLinks(const std::string& linkTable,
                             const std::string& idColumn,
                             const std::string& lookupTable,
                             const std::string& lookupIdColumn,
                             int idGame,
                             const std::vector<std::string>& names)
{
  m_pDS->exec(PrepareSQL("DELETE FROM %s WHERE idGame = %i", linkTable.c_str(), idGame));

  for (const std::string& name : names)
  {
    const int id = AddLookup(lookupTable, lookupIdColumn, name);
    if (id <= 0)
      continue;

    m_pDS->exec(PrepareSQL("INSERT INTO %s (%s, idGame) VALUES (%i, %i)", linkTable.c_str(),
                           idColumn.c_str(), id, idGame));
  }
}

std::vector<std::string> CGameDatabase::GetLinkNames(const std::string& linkTable,
                                                     const std::string& idColumn,
                                                     const std::string& lookupTable,
                                                     const std::string& lookupIdColumn,
                                                     int idGame,
                                                     const std::string& nameColumn)
{
  std::vector<std::string> names;

  try
  {
    const std::string sql = PrepareSQL(
        "SELECT %s.%s FROM %s JOIN %s ON %s.%s = %s.%s WHERE %s.idGame = %i ORDER BY %s.%s",
        lookupTable.c_str(), nameColumn.c_str(), lookupTable.c_str(), linkTable.c_str(),
        lookupTable.c_str(), lookupIdColumn.c_str(), linkTable.c_str(), idColumn.c_str(),
        linkTable.c_str(), idGame, lookupTable.c_str(), lookupIdColumn.c_str());

    if (m_pDS->query(sql))
    {
      while (!m_pDS->eof())
      {
        names.emplace_back(m_pDS->fv(0).get_asString());
        m_pDS->next();
      }
      m_pDS->close();
    }
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "GAME: Failed to read {} for game {}", linkTable, idGame);
  }

  return names;
}
