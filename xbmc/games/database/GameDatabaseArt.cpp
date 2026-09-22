/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "GameDatabase.h"
#include "dbwrappers/dataset.h"
#include "utils/StringUtils.h"
#include "utils/log.h"

using namespace KODI;
using namespace GAME;

bool CGameDatabase::SetArtForItem(int mediaId,
                                  const MediaType& mediaType,
                                  const std::string& artType,
                                  const std::string& url)
{
  if (mediaId <= 0 || mediaType.empty() || artType.empty())
    return false;

  try
  {
    const std::string sql =
        PrepareSQL("SELECT art_id, url FROM art WHERE media_id = %i AND media_type = '%s' AND type "
                   "= '%s'",
                   mediaId, mediaType.c_str(), artType.c_str());
    if (m_pDS->query(sql) && !m_pDS->eof())
    {
      const int artId = m_pDS->fv(0).get_asInt();
      const std::string oldUrl = m_pDS->fv(1).get_asString();
      m_pDS->close();

      if (oldUrl != url)
        m_pDS->exec(PrepareSQL("UPDATE art SET url = '%s' WHERE art_id = %i", url.c_str(), artId));
    }
    else
    {
      m_pDS->close();
      m_pDS->exec(PrepareSQL("INSERT INTO art (media_id, media_type, type, url) VALUES (%i, '%s', "
                             "'%s', '%s')",
                             mediaId, mediaType.c_str(), artType.c_str(), url.c_str()));
    }
    return true;
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "GAME: Failed to set {} art for {} {}", artType, mediaType, mediaId);
  }
  return false;
}

bool CGameDatabase::SetArtForItem(int mediaId,
                                  const MediaType& mediaType,
                                  const KODI::ART::Artwork& art)
{
  bool ok = true;
  for (const auto& [type, url] : art)
  {
    if (url.empty())
      ok = RemoveArtForItem(mediaId, mediaType, type) && ok;
    else
      ok = SetArtForItem(mediaId, mediaType, type, url) && ok;
  }
  return ok;
}

bool CGameDatabase::GetArtForItem(int mediaId, const MediaType& mediaType, KODI::ART::Artwork& art)
{
  try
  {
    const std::string sql = PrepareSQL(
        "SELECT type, url FROM art WHERE media_id = %i AND media_type = '%s'", mediaId,
        mediaType.c_str());
    if (m_pDS->query(sql))
    {
      while (!m_pDS->eof())
      {
        art[m_pDS->fv(0).get_asString()] = m_pDS->fv(1).get_asString();
        m_pDS->next();
      }
      m_pDS->close();
    }
    AddDefaultArt(art);
    return !art.empty();
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "GAME: Failed to read art for {} {}", mediaType, mediaId);
  }
  return false;
}

void CGameDatabase::AddDefaultArt(KODI::ART::Artwork& art)
{
  // A game is sold in a box, so its front is the picture; skins ask for a
  // thumb or a poster, and both mean the same picture here
  const auto front = art.find("boxfront");
  if (front == art.end() || front->second.empty())
    return;
  for (const char* alias : {"thumb", "poster"})
  {
    if (art.find(alias) == art.end())
      art[alias] = front->second;
  }
}

bool CGameDatabase::GetArtForItems(const std::vector<int>& mediaIds,
                                   const MediaType& mediaType,
                                   std::map<int, KODI::ART::Artwork>& art)
{
  if (mediaIds.empty())
    return true;

  try
  {
    // In chunks, because a library can hold tens of thousands of games
    constexpr size_t chunkSize = 500;
    for (size_t start = 0; start < mediaIds.size(); start += chunkSize)
    {
      const size_t end = std::min(start + chunkSize, mediaIds.size());
      std::vector<std::string> ids;
      ids.reserve(end - start);
      for (size_t i = start; i < end; ++i)
        ids.emplace_back(std::to_string(mediaIds[i]));

      const std::string sql =
          PrepareSQL("SELECT media_id, type, url FROM art WHERE media_type = '%s' AND media_id IN (",
                     mediaType.c_str()) +
          StringUtils::Join(ids, ",") + ")";

      if (!m_pDS->query(sql))
        continue;
      while (!m_pDS->eof())
      {
        art[m_pDS->fv(0).get_asInt()][m_pDS->fv(1).get_asString()] = m_pDS->fv(2).get_asString();
        m_pDS->next();
      }
      m_pDS->close();
    }

    for (auto& [id, one] : art)
      AddDefaultArt(one);

    return true;
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "GAME: Failed to read art for {} items", mediaIds.size());
  }
  return false;
}

std::string CGameDatabase::GetArtForItem(int mediaId,
                                         const MediaType& mediaType,
                                         const std::string& artType)
{
  try
  {
    return GetSingleValue(PrepareSQL(
        "SELECT url FROM art WHERE media_id = %i AND media_type = '%s' AND type = '%s'", mediaId,
        mediaType.c_str(), artType.c_str()));
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "GAME: Failed to read {} art for {} {}", artType, mediaType, mediaId);
  }
  return "";
}

bool CGameDatabase::RemoveArtForItem(int mediaId,
                                     const MediaType& mediaType,
                                     const std::string& artType)
{
  return ExecuteQuery(
      PrepareSQL("DELETE FROM art WHERE media_id = %i AND media_type = '%s' AND type = '%s'",
                 mediaId, mediaType.c_str(), artType.c_str()));
}

bool CGameDatabase::GetArtTypes(const MediaType& mediaType, std::vector<std::string>& artTypes)
{
  try
  {
    if (m_pDS->query(PrepareSQL("SELECT DISTINCT type FROM art WHERE media_type = '%s'",
                                mediaType.c_str())))
    {
      while (!m_pDS->eof())
      {
        artTypes.emplace_back(m_pDS->fv(0).get_asString());
        m_pDS->next();
      }
      m_pDS->close();
    }
    return true;
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "GAME: Failed to list art types for {}", mediaType);
  }
  return false;
}
