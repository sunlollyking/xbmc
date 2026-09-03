/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "GameDatabase.h"
#include "XBDateTime.h"
#include "dbwrappers/dataset.h"
#include "utils/URIUtils.h"
#include "utils/log.h"

using namespace KODI;
using namespace GAME;

namespace
{
std::string FolderPath(const std::string& path)
{
  std::string folder = path;
  URIUtils::AddSlashAtEnd(folder);
  return folder;
}

std::string Now()
{
  return CDateTime::GetCurrentDateTime().GetAsDBDateTime();
}
} // namespace

int CGameDatabase::AddPath(const std::string& path, const std::string& parentPath)
{
  if (path.empty())
    return -1;

  try
  {
    const std::string folder = FolderPath(path);

    int idPath = GetPathId(folder);
    if (idPath > 0)
      return idPath;

    std::string parent = parentPath;
    if (parent.empty())
      parent = URIUtils::GetParentPath(folder);
    const int idParent = parent.empty() ? -1 : GetPathId(FolderPath(parent));

    const std::string sql = PrepareSQL(
        "INSERT INTO path (strPath, idParentPath, dateAdded, scanRecursive, useFolderNames, "
        "noUpdate, exclude) VALUES ('%s', %i, '%s', 1, 0, 0, 0)",
        folder.c_str(), idParent, Now().c_str());
    m_pDS->exec(sql);
    return static_cast<int>(m_pDS->lastinsertid());
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "GAME: Failed to add path {}", path);
  }
  return -1;
}

int CGameDatabase::GetPathId(const std::string& path)
{
  if (path.empty())
    return -1;

  try
  {
    return GetSingleValueInt(
        PrepareSQL("SELECT idPath FROM path WHERE strPath = '%s'", FolderPath(path).c_str()));
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "GAME: Failed to look up path {}", path);
  }
  return -1;
}

bool CGameDatabase::GetPathHash(const std::string& path, std::string& hash)
{
  try
  {
    hash = GetSingleValue(
        PrepareSQL("SELECT strHash FROM path WHERE strPath = '%s'", FolderPath(path).c_str()));
    return true;
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "GAME: Failed to read the hash of {}", path);
  }
  return false;
}

bool CGameDatabase::SetPathHash(const std::string& path, const std::string& hash)
{
  const int idPath = AddPath(path);
  if (idPath <= 0)
    return false;

  return ExecuteQuery(
      PrepareSQL("UPDATE path SET strHash = '%s' WHERE idPath = %i", hash.c_str(), idPath));
}

bool CGameDatabase::SetPathContent(const std::string& path, const GamePathContent& content)
{
  const int idPath = AddPath(path);
  if (idPath <= 0)
    return false;

  return ExecuteQuery(PrepareSQL("UPDATE path SET idPlatform = %i, strContent = '%s', strScraper "
                                 "= '%s', strSettings = '%s', scanRecursive = %i, useFolderNames "
                                 "= %i, noUpdate = %i, exclude = %i WHERE idPath = %i",
                                 content.idPlatform, content.HasContent() ? "games" : "",
                                 content.scraper.c_str(), content.settings.c_str(),
                                 content.scanRecursive ? 1 : 0, content.useFolderNames ? 1 : 0,
                                 content.noUpdate ? 1 : 0, content.exclude ? 1 : 0, idPath));
}

bool CGameDatabase::GetPathContent(const std::string& path,
                                   GamePathContent& content,
                                   bool& foundDirectly)
{
  content = {};
  foundDirectly = false;

  std::string folder = FolderPath(path);
  bool first = true;

  try
  {
    while (!folder.empty())
    {
      const std::string sql = PrepareSQL(
          "SELECT idPlatform, strScraper, strSettings, scanRecursive, useFolderNames, noUpdate, "
          "exclude FROM path WHERE strPath = '%s'",
          folder.c_str());

      if (m_pDS->query(sql) && !m_pDS->eof())
      {
        const bool exclude = m_pDS->fv("exclude").get_asBool();
        const int idPlatform = m_pDS->fv("idPlatform").get_asInt();

        if (exclude)
        {
          m_pDS->close();
          content.exclude = true;
          return false;
        }

        if (idPlatform > 0)
        {
          content.idPlatform = idPlatform;
          content.scraper = m_pDS->fv("strScraper").get_asString();
          content.settings = m_pDS->fv("strSettings").get_asString();
          content.scanRecursive = m_pDS->fv("scanRecursive").get_asInt() != 0;
          content.useFolderNames = m_pDS->fv("useFolderNames").get_asBool();
          content.noUpdate = m_pDS->fv("noUpdate").get_asBool();
          m_pDS->close();
          foundDirectly = first;
          return true;
        }
      }
      m_pDS->close();

      const std::string parent = URIUtils::GetParentPath(folder);
      if (parent.empty() || parent == folder)
        break;
      folder = FolderPath(parent);
      first = false;
    }
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "GAME: Failed to read the content of {}", path);
  }

  return false;
}

bool CGameDatabase::GetContentPaths(std::vector<std::string>& paths)
{
  try
  {
    if (m_pDS->query("SELECT strPath FROM path WHERE idPlatform > 0 AND exclude = 0 AND noUpdate "
                     "= 0 ORDER BY strPath"))
    {
      while (!m_pDS->eof())
      {
        paths.emplace_back(m_pDS->fv(0).get_asString());
        m_pDS->next();
      }
      m_pDS->close();
    }
    return true;
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "GAME: Failed to list the folders with content");
  }
  return false;
}

bool CGameDatabase::GetSubPaths(const std::string& basePath,
                                std::vector<std::pair<int, std::string>>& paths)
{
  try
  {
    const std::string folder = FolderPath(basePath);
    if (m_pDS->query(PrepareSQL("SELECT idPath, strPath FROM path WHERE SUBSTR(strPath, 1, %i) = "
                                "'%s' ORDER BY strPath",
                                static_cast<int>(folder.size()), folder.c_str())))
    {
      while (!m_pDS->eof())
      {
        paths.emplace_back(m_pDS->fv(0).get_asInt(), m_pDS->fv(1).get_asString());
        m_pDS->next();
      }
      m_pDS->close();
    }
    return true;
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "GAME: Failed to list the folders under {}", basePath);
  }
  return false;
}

int CGameDatabase::AddFile(const std::string& fileNameAndPath, int idRelease, const GameFile& file)
{
  if (fileNameAndPath.empty())
    return -1;

  try
  {
    std::string folder;
    std::string fileName;
    URIUtils::Split(fileNameAndPath, folder, fileName);

    const int idPath = AddPath(folder);
    if (idPath <= 0)
      return -1;

    const int idFile = GetFileId(fileNameAndPath);
    if (idFile > 0)
    {
      m_pDS->exec(PrepareSQL("UPDATE files SET idRelease = %i, size = %llu, crc32 = '%s', md5 = "
                             "'%s', sha1 = '%s', serial = '%s', raHash = '%s', discNumber = %i "
                             "WHERE idFile = %i",
                             idRelease, static_cast<unsigned long long>(file.size),
                             file.crc32.c_str(), file.md5.c_str(), file.sha1.c_str(),
                             file.serial.c_str(), file.raHash.c_str(), file.disc, idFile));
      return idFile;
    }

    m_pDS->exec(PrepareSQL(
        "INSERT INTO files (idPath, idRelease, strFilename, size, crc32, md5, sha1, serial, "
        "raHash, discNumber, playCount, lastPlayed, playTime, dateAdded) VALUES (%i, %i, '%s', "
        "%llu, '%s', '%s', '%s', '%s', '%s', %i, %i, '%s', 0, '%s')",
        idPath, idRelease, fileName.c_str(), static_cast<unsigned long long>(file.size),
        file.crc32.c_str(), file.md5.c_str(), file.sha1.c_str(), file.serial.c_str(),
        file.raHash.c_str(), file.disc, file.playCount, file.lastPlayed.c_str(),
        (file.dateAdded.empty() ? Now() : file.dateAdded).c_str()));
    return static_cast<int>(m_pDS->lastinsertid());
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "GAME: Failed to add file {}", fileNameAndPath);
  }
  return -1;
}

int CGameDatabase::GetFileId(const std::string& fileNameAndPath)
{
  if (fileNameAndPath.empty())
    return -1;

  try
  {
    std::string folder;
    std::string fileName;
    URIUtils::Split(fileNameAndPath, folder, fileName);

    return GetSingleValueInt(PrepareSQL("SELECT idFile FROM files JOIN path ON path.idPath = "
                                        "files.idPath WHERE path.strPath = '%s' AND "
                                        "files.strFilename = '%s'",
                                        FolderPath(folder).c_str(), fileName.c_str()));
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "GAME: Failed to look up file {}", fileNameAndPath);
  }
  return -1;
}

int CGameDatabase::GetGameIdByFile(const std::string& fileNameAndPath)
{
  const int idFile = GetFileId(fileNameAndPath);
  if (idFile <= 0)
    return -1;

  try
  {
    return GetSingleValueInt(
        PrepareSQL("SELECT gamerelease.idGame FROM files JOIN gamerelease ON gamerelease.idRelease "
                   "= files.idRelease WHERE files.idFile = %i",
                   idFile));
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "GAME: Failed to find the game of {}", fileNameAndPath);
  }
  return -1;
}

bool CGameDatabase::GetFilesForRelease(int idRelease, std::vector<GameFile>& files)
{
  try
  {
    const std::string sql =
        PrepareSQL("SELECT files.*, path.strPath FROM files JOIN path ON path.idPath = "
                   "files.idPath WHERE files.idRelease = %i ORDER BY files.discNumber, "
                   "files.strFilename",
                   idRelease);
    if (m_pDS->query(sql))
    {
      while (!m_pDS->eof())
      {
        GameFile file;
        file.id = m_pDS->fv("idFile").get_asInt();
        file.path = m_pDS->fv("strPath").get_asString() + m_pDS->fv("strFilename").get_asString();
        file.size = static_cast<uint64_t>(m_pDS->fv("size").get_asDouble());
        file.crc32 = m_pDS->fv("crc32").get_asString();
        file.md5 = m_pDS->fv("md5").get_asString();
        file.sha1 = m_pDS->fv("sha1").get_asString();
        file.serial = m_pDS->fv("serial").get_asString();
        file.raHash = m_pDS->fv("raHash").get_asString();
        file.disc = m_pDS->fv("discNumber").get_asInt();
        file.playCount = m_pDS->fv("playCount").get_asInt();
        file.lastPlayed = m_pDS->fv("lastPlayed").get_asString();
        file.dateAdded = m_pDS->fv("dateAdded").get_asString();
        files.emplace_back(std::move(file));
        m_pDS->next();
      }
      m_pDS->close();
    }
    return true;
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "GAME: Failed to list the files of release {}", idRelease);
  }
  return false;
}

bool CGameDatabase::MarkPlayed(const std::string& fileNameAndPath)
{
  const int idFile = GetFileId(fileNameAndPath);
  if (idFile <= 0)
    return false;

  return ExecuteQuery(
      PrepareSQL("UPDATE files SET playCount = playCount + 1, lastPlayed = '%s' WHERE idFile = %i",
                 Now().c_str(), idFile));
}
