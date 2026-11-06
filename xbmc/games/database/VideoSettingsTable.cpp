/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "VideoSettingsTable.h"

#include "PathDefaults.h"
#include "dbwrappers/Database.h"
#include "utils/StringUtils.h"
#include "utils/log.h"

using namespace KODI;
using namespace GAME;

CVideoSettingsTable::CVideoSettingsTable(CDatabase& database) : m_database(database)
{
}

CVideoSettingsTable::~CVideoSettingsTable() = default;

void CVideoSettingsTable::Create()
{
  CLog::Log(LOGINFO, "GAME: Creating videosettings table");

  // A NULL column is a setting the path leaves to the folders above it
  m_database.ExecuteQuery("CREATE TABLE videosettings ("
                          "idPath integer primary key,"
                          "path text,"
                          "videoFilter text,"
                          "stretchMode text,"
                          "rotation integer,"
                          "bezel integer)");
}

void CVideoSettingsTable::CreateAnalytics()
{
  CLog::Log(LOGINFO, "GAME: Creating videosettings index");

  // Unique: a path has one row, so storing it again replaces it
  m_database.ExecuteQuery("CREATE UNIQUE INDEX idxVideoSettingsPath ON videosettings(path)");
}

void CVideoSettingsTable::UpdateTables(int version)
{
  // Its index is made by the CreateAnalytics that follows an update
  if (version < 9)
    Create();

  // Before version 9 only the filter was kept, from version 2
  if (version >= 2 && version < 9)
  {
    m_database.ExecuteQuery("INSERT INTO videosettings (path, videoFilter) "
                            "SELECT path, videoFilter FROM videofilter WHERE videoFilter != ''");
    m_database.ExecuteQuery("DROP TABLE videofilter");
  }
}

bool CVideoSettingsTable::SetVideoSettings(const std::string& path,
                                           const GameVideoSettings& settings)
{
  if (path.empty())
    return false;

  if (settings.IsEmpty())
    return ClearVideoSettings(path);

  const auto text = [this](const std::optional<std::string>& value)
  { return value ? m_database.PrepareSQL("'%s'", value->c_str()) : std::string("NULL"); };
  const std::string rotation =
      settings.rotationDegCCW ? std::to_string(*settings.rotationDegCCW) : "NULL";
  const std::string bezel =
      settings.bezelEnabled ? std::to_string(*settings.bezelEnabled ? 1 : 0) : "NULL";

  const std::string sql =
      m_database.PrepareSQL("REPLACE INTO videosettings (path, videoFilter, stretchMode, "
                            "rotation, bezel) VALUES ('%s', ",
                            path.c_str()) +
      text(settings.videoFilter) + ", " + text(settings.stretchMode) + ", " + rotation + ", " +
      bezel + ")";

  if (!m_database.ExecuteQuery(sql))
  {
    CLog::Log(LOGERROR, "GAME: Failed to remember the video settings for {}", path);
    return false;
  }

  return true;
}

GameVideoSettings CVideoSettingsTable::GetVideoSettings(const std::string& path)
{
  GameVideoSettings settings;
  if (path.empty())
    return settings;

  // Each column is read on its own, marked so that a NULL can be told apart
  // from a value that is empty or zero
  const auto column = [this, &path](const char* name) -> std::optional<std::string>
  {
    const std::string value = m_database.GetSingleValue(m_database.PrepareSQL(
        "SELECT '=' || %s FROM videosettings WHERE path='%s'", name, path.c_str()));
    if (value.empty())
      return std::nullopt;
    return value.substr(1);
  };

  settings.videoFilter = column("videoFilter");
  settings.stretchMode = column("stretchMode");
  if (const auto rotation = column("rotation"))
    settings.rotationDegCCW = static_cast<unsigned int>(StringUtils::ToUint32(*rotation, 0));
  if (const auto bezel = column("bezel"))
    settings.bezelEnabled = (*bezel != "0");

  return settings;
}

GameVideoSettings CVideoSettingsTable::GetFolderVideoSettings(const std::string& path)
{
  GameVideoSettings settings;
  for (const std::string& folder : GetParentPaths(path))
    settings.Inherit(GetVideoSettings(folder));
  return settings;
}

bool CVideoSettingsTable::SetVideoFilter(const std::string& path, const std::string& videoFilter)
{
  GameVideoSettings settings = GetVideoSettings(path);
  if (videoFilter.empty())
    settings.videoFilter.reset();
  else
    settings.videoFilter = videoFilter;
  return SetVideoSettings(path, settings);
}

std::string CVideoSettingsTable::GetVideoFilter(const std::string& path)
{
  return GetVideoSettings(path).videoFilter.value_or("");
}

bool CVideoSettingsTable::ClearVideoSettings(const std::string& path)
{
  const std::string sql =
      m_database.PrepareSQL("DELETE FROM videosettings WHERE path='%s'", path.c_str());

  if (!m_database.ExecuteQuery(sql))
  {
    CLog::Log(LOGERROR, "GAME: Failed to forget the video settings for {}", path);
    return false;
  }

  return true;
}
