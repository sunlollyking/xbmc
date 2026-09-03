/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "FileItem.h"
#include "FileItemList.h"
#include "GameDatabase.h"
#include "XBDateTime.h"
#include "dbwrappers/dataset.h"
#include "games/library/GameDbUrl.h"
#include "utils/StringUtils.h"
#include "utils/URIUtils.h"
#include "utils/log.h"

using namespace KODI;
using namespace GAME;

int CGameDatabase::AddPlatform(const PlatformInfo& platform)
{
  if (platform.slug.empty() || platform.name.empty())
    return -1;

  try
  {
    const std::string extensions = StringUtils::Join(platform.extensions, " ");
    const std::string sortName = platform.sortName.empty() ? platform.name : platform.sortName;
    const std::string type(CGameLibraryTypes::ToString(platform.type));
    const std::string media(CGameLibraryTypes::ToString(platform.media));

    int idPlatform = GetSingleValueInt(
        PrepareSQL("SELECT idPlatform FROM platform WHERE slug = '%s'", platform.slug.c_str()));

    if (idPlatform > 0)
    {
      m_pDS->exec(PrepareSQL(
          "UPDATE platform SET name = '%s', sortName = '%s', manufacturer = '%s', type = '%s', "
          "media = '%s', family = '%s', released = %i, discontinued = %i, overview = '%s', "
          "extensions = '%s' WHERE idPlatform = %i",
          platform.name.c_str(), sortName.c_str(), platform.manufacturer.c_str(), type.c_str(),
          media.c_str(), platform.family.c_str(), platform.released, platform.discontinued,
          platform.overview.c_str(), extensions.c_str(), idPlatform));
    }
    else
    {
      m_pDS->exec(PrepareSQL(
          "INSERT INTO platform (slug, name, sortName, manufacturer, type, media, family, "
          "released, discontinued, overview, extensions, defaultGameClient, "
          "defaultVideoFilter, dateAdded) VALUES "
          "('%s', '%s', '%s', '%s', '%s', '%s', '%s', %i, %i, '%s', '%s', '%s', '%s', '%s')",
          platform.slug.c_str(), platform.name.c_str(), sortName.c_str(),
          platform.manufacturer.c_str(), type.c_str(), media.c_str(), platform.family.c_str(),
          platform.released, platform.discontinued, platform.overview.c_str(),
          extensions.c_str(), platform.defaultGameClient.c_str(),
          platform.defaultVideoFilter.c_str(),
          CDateTime::GetCurrentDateTime().GetAsDBDateTime().c_str()));
      idPlatform = static_cast<int>(m_pDS->lastinsertid());
    }

    SetPlatformIds(idPlatform, platform.providerIds);
    return idPlatform;
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "GAME: Failed to add platform {}", platform.slug);
  }
  return -1;
}

void CGameDatabase::SetPlatformIds(int idPlatform,
                                   const std::map<std::string, std::string>& providerIds)
{
  for (const auto& [provider, id] : providerIds)
  {
    if (provider.empty() || id.empty())
      continue;

    m_pDS->exec(PrepareSQL("DELETE FROM uniqueid WHERE media_id = %i AND media_type = '%s' AND "
                           "type = '%s'",
                           idPlatform, MediaTypeGamePlatform, provider.c_str()));
    m_pDS->exec(PrepareSQL("INSERT INTO uniqueid (media_id, media_type, type, value) VALUES (%i, "
                           "'%s', '%s', '%s')",
                           idPlatform, MediaTypeGamePlatform, provider.c_str(), id.c_str()));
  }
}

void CGameDatabase::GetPlatformFromRecord(PlatformInfo& platform)
{
  platform.id = m_pDS->fv("idPlatform").get_asInt();
  platform.slug = m_pDS->fv("slug").get_asString();
  platform.name = m_pDS->fv("name").get_asString();
  platform.sortName = m_pDS->fv("sortName").get_asString();
  platform.manufacturer = m_pDS->fv("manufacturer").get_asString();
  platform.type = CGameLibraryTypes::PlatformTypeFromString(m_pDS->fv("type").get_asString());
  platform.media = CGameLibraryTypes::MediaFormatFromString(m_pDS->fv("media").get_asString());
  platform.family = m_pDS->fv("family").get_asString();
  platform.released = m_pDS->fv("released").get_asInt();
  platform.discontinued = m_pDS->fv("discontinued").get_asInt();
  platform.overview = m_pDS->fv("overview").get_asString();
  platform.extensions = StringUtils::Split(m_pDS->fv("extensions").get_asString(), ' ');
  std::erase_if(platform.extensions, [](const std::string& s) { return s.empty(); });
  platform.defaultGameClient = m_pDS->fv("defaultGameClient").get_asString();
  platform.defaultVideoFilter = m_pDS->fv("defaultVideoFilter").get_asString();
  platform.dateAdded = m_pDS->fv("dateAdded").get_asString();
  platform.lastScraped = m_pDS->fv("lastScraped").get_asString();
  platform.gameCount = m_pDS->fv("gameCount").get_asInt();
}

bool CGameDatabase::SetPlatformDefaults(int idPlatform,
                                       const std::string& gameClient,
                                       const std::string& videoFilter)
{
  return ExecuteQuery(PrepareSQL("UPDATE platform SET defaultGameClient = '%s', "
                                 "defaultVideoFilter = '%s' WHERE idPlatform = %i",
                                 gameClient.c_str(), videoFilter.c_str(), idPlatform));
}

int CGameDatabase::GetPlatformIdForGame(const std::string& path)
{
  const int idGame = GetGameIdByFile(path);
  if (idGame <= 0)
    return -1;
  return GetSingleValueInt(PrepareSQL("SELECT idPlatform FROM game WHERE idGame = %i", idGame));
}

bool CGameDatabase::GetPlatform(int idPlatform, PlatformInfo& platform)
{
  try
  {
    if (!m_pDS->query(
            PrepareSQL("SELECT * FROM platform_view WHERE idPlatform = %i", idPlatform)) ||
        m_pDS->eof())
    {
      m_pDS->close();
      return false;
    }

    GetPlatformFromRecord(platform);
    m_pDS->close();

    if (m_pDS->query(PrepareSQL("SELECT type, value FROM uniqueid WHERE media_id = %i AND "
                                "media_type = '%s'",
                                idPlatform, MediaTypeGamePlatform)))
    {
      while (!m_pDS->eof())
      {
        platform.providerIds[m_pDS->fv(0).get_asString()] = m_pDS->fv(1).get_asString();
        m_pDS->next();
      }
      m_pDS->close();
    }
    return true;
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "GAME: Failed to read platform {}", idPlatform);
  }
  return false;
}

bool CGameDatabase::GetPlatformBySlug(const std::string& slug, PlatformInfo& platform)
{
  const int idPlatform =
      GetSingleValueInt(PrepareSQL("SELECT idPlatform FROM platform WHERE slug = '%s'", slug.c_str()));
  if (idPlatform <= 0)
    return false;
  return GetPlatform(idPlatform, platform);
}

bool CGameDatabase::GetPlatforms(std::vector<PlatformInfo>& platforms, bool onlyWithGames)
{
  try
  {
    std::string sql = "SELECT * FROM platform_view";
    if (onlyWithGames)
      sql += " WHERE gameCount > 0";
    sql += " ORDER BY sortName";

    if (m_pDS->query(sql))
    {
      while (!m_pDS->eof())
      {
        PlatformInfo platform;
        GetPlatformFromRecord(platform);
        platforms.emplace_back(std::move(platform));
        m_pDS->next();
      }
      m_pDS->close();
    }
    return true;
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "GAME: Failed to list platforms");
  }
  return false;
}

bool CGameDatabase::GetPlatformsNav(const std::string& baseDir,
                                    CFileItemList& items,
                                    bool onlyWithGames)
{
  std::vector<PlatformInfo> platforms;
  if (!GetPlatforms(platforms, onlyWithGames))
    return false;

  std::string base = baseDir;
  URIUtils::AddSlashAtEnd(base);

  for (const PlatformInfo& platform : platforms)
  {
    const auto item = std::make_shared<CFileItem>(platform.name);
    item->SetPath(base + std::to_string(platform.id) + "/");
    item->SetFolder(true);
    item->SetLabelPreformatted(true);
    item->SetProperty("platformid", platform.id);
    item->SetProperty("platformslug", platform.slug);
    item->SetProperty("manufacturer", platform.manufacturer);
    item->SetProperty("gamecount", platform.gameCount);
    item->SetProperty("released", platform.released);
    item->SetProperty("platformtype", std::string(CGameLibraryTypes::ToString(platform.type)));
    item->SetLabel2(std::to_string(platform.gameCount));

    KODI::ART::Artwork art;
    if (GetArtForItem(platform.id, MediaTypeGamePlatform, art))
      item->SetArt(art);

    items.Add(item);
  }

  items.SetContent("platforms");
  return true;
}

int CGameDatabase::GetPlatformIdForPath(const std::string& path)
{
  GamePathContent content;
  bool foundDirectly = false;
  if (!GetPathContent(path, content, foundDirectly))
    return -1;
  return content.idPlatform;
}
