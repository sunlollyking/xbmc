/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "VideoFilterTable.h"

#include "PathDefaults.h"
#include "dbwrappers/Database.h"
#include "utils/log.h"

using namespace KODI;
using namespace GAME;

CVideoFilterTable::CVideoFilterTable(CDatabase& database) : m_database(database)
{
}

CVideoFilterTable::~CVideoFilterTable() = default;

void CVideoFilterTable::Create()
{
  CLog::Log(LOGINFO, "GAME: Creating videofilter table");

  m_database.ExecuteQuery("CREATE TABLE videofilter ("
                          "idPath integer primary key,"
                          "path text,"
                          "videoFilter text)");
}

void CVideoFilterTable::CreateAnalytics()
{
  CLog::Log(LOGINFO, "GAME: Creating videofilter index");

  // Unique: a path has one filter, so storing a second replaces the first
  m_database.ExecuteQuery("CREATE UNIQUE INDEX idxVideoFilterPath ON videofilter(path)");
}

void CVideoFilterTable::UpdateTables(int version)
{
  // The table arrived in schema version 2, so a database written before that
  // has to be given it now rather than at creation
  if (version < 2)
  {
    Create();
    CreateAnalytics();
  }
}

bool CVideoFilterTable::SetVideoFilter(const std::string& path, const std::string& videoFilter)
{
  if (path.empty())
    return false;

  // No filter means the path is no longer remembered, rather than remembered
  // as nothing
  if (videoFilter.empty())
    return ClearVideoFilter(path);

  const std::string sql =
      m_database.PrepareSQL("REPLACE INTO videofilter (path, videoFilter) VALUES ('%s', '%s')",
                            path.c_str(), videoFilter.c_str());

  if (!m_database.ExecuteQuery(sql))
  {
    CLog::Log(LOGERROR, "GAME: Failed to remember video filter {} for {}", videoFilter, path);
    return false;
  }

  return true;
}

std::string CVideoFilterTable::GetVideoFilter(const std::string& path)
{
  if (path.empty())
    return "";

  const std::string sql =
      m_database.PrepareSQL("SELECT videoFilter FROM videofilter WHERE path='%s'", path.c_str());

  return m_database.GetSingleValue(sql);
}

std::string CVideoFilterTable::GetVideoFilterForGame(const std::string& path)
{
  return FindPathDefault(path, [this](const std::string& p) { return GetVideoFilter(p); });
}

bool CVideoFilterTable::ClearVideoFilter(const std::string& path)
{
  if (path.empty())
    return false;

  const std::string sql =
      m_database.PrepareSQL("DELETE FROM videofilter WHERE path='%s'", path.c_str());

  if (!m_database.ExecuteQuery(sql))
  {
    CLog::Log(LOGERROR, "GAME: Failed to forget the video filter for {}", path);
    return false;
  }

  return true;
}
