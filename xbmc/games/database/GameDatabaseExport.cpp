/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "FileItem.h"
#include "GameDatabase.h"
#include "ServiceBroker.h"
#include "TextureCache.h"
#include "URL.h"
#include "XBDateTime.h"
#include "dbwrappers/dataset.h"
#include "dialogs/GUIDialogProgress.h"
#include "filesystem/Directory.h"
#include "games/tags/GameInfoTag.h"
#include "games/tags/GameInfoTagLoader.h"
#include "guilib/GUIComponent.h"
#include "guilib/GUIWindowManager.h"
#include "utils/StringUtils.h"
#include "utils/URIUtils.h"
#include "utils/XBMCTinyXML2.h"
#include "utils/XMLUtils.h"
#include "utils/log.h"

#include <map>
#include <string>
#include <vector>

using namespace KODI;
using namespace GAME;

namespace
{
constexpr const char* XML_ROOT = "gamedb";
constexpr const char* XML_GAME = "game";

//! \brief Progress is reported per game, but a dialog does not need every one
constexpr int PROGRESS_EVERY = 10;

/*!
 * \brief Where a game's pictures are copied to in a single-file export
 *
 * Named after the file rather than the title, because two games can share a
 * title and no two can share a path.
 */
std::string ArtName(const std::string& playPath, const std::string& artType)
{
  std::string name = URIUtils::GetFileName(playPath);
  URIUtils::RemoveExtension(name);
  return name + "-" + artType;
}
} // namespace

void CGameDatabase::ExportToXML(const std::string& path,
                                bool singleFile /* = true */,
                                bool images /* = false */,
                                bool overwrite /* = false */)
{
  if (m_pDB == nullptr || m_pDS == nullptr)
    return;

  CGUIDialogProgress* progress =
      CServiceBroker::GetGUI()->GetWindowManager().GetWindow<CGUIDialogProgress>(
          WINDOW_DIALOG_PROGRESS);

  int exported = 0;
  int failed = 0;
  try
  {
    std::string exportRoot;
    std::string xmlFile;
    std::string artDir;
    if (singleFile)
    {
      // One file for the whole library is a backup, so it takes the pictures
      // with it and starts from an empty folder
      images = true;
      overwrite = false;
      exportRoot = URIUtils::AddFileToFolder(
          path, "kodi_gamedb_" + CDateTime::GetCurrentDateTime().GetAsDBDate());
      xmlFile = URIUtils::AddFileToFolder(exportRoot, "gamedb.xml");
      artDir = URIUtils::AddFileToFolder(exportRoot, "art");
      XFILE::CDirectory::Remove(exportRoot);
      XFILE::CDirectory::Create(exportRoot);
      if (images)
        XFILE::CDirectory::Create(artDir);
    }

    // The whole library at once: a game is small, and reading it row by row
    // while writing files would hold the dataset open across the writes
    m_pDS->query("SELECT idGame FROM game ORDER BY idGame");
    std::vector<int> ids;
    ids.reserve(m_pDS->num_rows());
    while (!m_pDS->eof())
    {
      ids.push_back(m_pDS->fv(0).get_asInt());
      m_pDS->next();
    }
    m_pDS->close();

    if (progress != nullptr)
    {
      progress->SetHeading(CVariant{35631}); // "Export game library"
      progress->SetLine(0, CVariant{15016}); // "Games"
      progress->SetLine(1, CVariant{""});
      progress->SetLine(2, CVariant{""});
      progress->Open();
      progress->ShowProgressBar(true);
    }

    CXBMCTinyXML2 xmlDoc;
    tinyxml2::XMLNode* root = nullptr;
    if (singleFile)
    {
      xmlDoc.InsertEndChild(xmlDoc.NewDeclaration());
      root = xmlDoc.InsertEndChild(xmlDoc.NewElement(XML_ROOT));
    }

    for (size_t i = 0; i < ids.size(); ++i)
    {
      CGameInfoTag tag;
      if (!GetGameInfo(ids[i], tag))
        continue;

      const std::string playPath = GetPlayPathForGame(ids[i]);

      if (singleFile)
      {
        tag.Save(root, XML_GAME);
        // The path is what an import matches on, and it is a property of the
        // library rather than of the game, so it is not part of Save()
        if (!playPath.empty())
        {
          if (tinyxml2::XMLElement* game = root->LastChildElement(XML_GAME); game != nullptr)
            XMLUtils::SetPath(game, "path", playPath);
        }
      }
      else if (!playPath.empty())
      {
        CFileItem item(playPath, false);
        if (!overwrite && CGameInfoTagLoader::HasNFO(item))
          continue;
        if (!CGameInfoTagLoader::Save(item, tag))
        {
          ++failed;
          continue;
        }
      }
      else
      {
        continue;
      }

      if (images && !playPath.empty())
        ExportArt(ids[i], playPath, singleFile ? artDir : "", overwrite);

      ++exported;

      if (progress != nullptr && (i % PROGRESS_EVERY) == 0)
      {
        progress->SetLine(1, CVariant{tag.GetTitle()});
        progress->SetPercentage(static_cast<int>(i * 100 / std::max<size_t>(ids.size(), 1)));
        progress->Progress();
        if (progress->IsCanceled())
          break;
      }
    }

    if (singleFile && !xmlDoc.SaveFile(xmlFile))
    {
      CLog::Log(LOGERROR, "GAME: Failed to write {}: {}", CURL::GetRedacted(xmlFile),
                xmlDoc.ErrorStr());
      ++failed;
    }
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "GAME: Failed to export the library");
    ++failed;
  }

  if (progress != nullptr)
    progress->Close();

  CLog::Log(LOGINFO, "GAME: Exported {} game(s), {} failure(s)", exported, failed);
}

void CGameDatabase::ExportArt(int idGame,
                              const std::string& playPath,
                              const std::string& artDir,
                              bool overwrite)
{
  KODI::ART::Artwork art;
  if (!GetArtForItem(idGame, MediaTypeGame, art))
    return;

  for (const auto& [type, url] : art)
  {
    if (url.empty())
      continue;

    std::string destination;
    if (!artDir.empty())
    {
      std::string name = ArtName(playPath, type);
      name += URIUtils::GetExtension(url).empty() ? ".jpg" : URIUtils::GetExtension(url);
      destination = URIUtils::AddFileToFolder(artDir, name);
    }
    else
    {
      // Beside the game, where Kodi looks for it: a file's own picture is a
      // .tbn, and anything else is ignored
      if (type != "thumb" && type != "boxfront")
        continue;
      destination = URIUtils::ReplaceExtension(playPath, ".tbn");
    }

    // From the texture cache rather than from where the art came from, so an
    // export does not fetch the whole library off the internet again
    CServiceBroker::GetTextureCache()->Export(url, destination, overwrite);
  }
}

void CGameDatabase::ImportFromXML(const std::string& path)
{
  if (m_pDB == nullptr || m_pDS == nullptr)
    return;

  const std::string xmlFile = URIUtils::AddFileToFolder(path, "gamedb.xml");
  CXBMCTinyXML2 xmlDoc;
  if (!xmlDoc.LoadFile(xmlFile))
  {
    CLog::Log(LOGERROR, "GAME: Failed to read {}: {}", CURL::GetRedacted(xmlFile),
              xmlDoc.ErrorStr());
    return;
  }

  const tinyxml2::XMLElement* root = xmlDoc.RootElement();
  if (root == nullptr || std::strcmp(root->Name(), XML_ROOT) != 0)
  {
    CLog::Log(LOGERROR, "GAME: {} is not a game library export", CURL::GetRedacted(xmlFile));
    return;
  }

  CGUIDialogProgress* progress =
      CServiceBroker::GetGUI()->GetWindowManager().GetWindow<CGUIDialogProgress>(
          WINDOW_DIALOG_PROGRESS);
  if (progress != nullptr)
  {
    progress->SetHeading(CVariant{35634}); // "Import game library"
    progress->SetLine(0, CVariant{15016}); // "Games"
    progress->SetLine(1, CVariant{""});
    progress->SetLine(2, CVariant{""});
    progress->Open();
    progress->ShowProgressBar(true);
  }

  int matched = 0;
  int missing = 0;
  int index = 0;
  const int total = [root]
  {
    int n = 0;
    for (const tinyxml2::XMLElement* e = root->FirstChildElement(XML_GAME); e != nullptr;
         e = e->NextSiblingElement(XML_GAME))
      ++n;
    return n;
  }();

  BeginTransaction();
  for (const tinyxml2::XMLElement* element = root->FirstChildElement(XML_GAME); element != nullptr;
       element = element->NextSiblingElement(XML_GAME), ++index)
  {
    CGameInfoTag tag;
    if (!tag.Load(element))
      continue;

    const int idGame = FindGameForImport(element, tag);
    if (idGame <= 0)
    {
      ++missing;
      continue;
    }

    // Only what a rescan cannot recover is restored. Everything a scraper
    // knows it will fetch again, and the export may be older than the
    // catalogues are
    if (tag.GetPlayCount() > 0)
      SetPlayCount(idGame, tag.GetPlayCount());
    if (tag.IsFavourite())
      SetFavourite(idGame, true);
    if (tag.IsCompleted())
      SetCompleted(idGame, true);
    if (tag.GetUserRating() > 0)
      SetUserRating(idGame, tag.GetUserRating());
    ++matched;

    if (progress != nullptr && (index % PROGRESS_EVERY) == 0)
    {
      progress->SetLine(1, CVariant{tag.GetTitle()});
      progress->SetPercentage(index * 100 / std::max(total, 1));
      progress->Progress();
      if (progress->IsCanceled())
        break;
    }
  }
  CommitTransaction();

  if (progress != nullptr)
    progress->Close();

  CLog::Log(LOGINFO, "GAME: Imported {} game(s); {} were not in the library", matched, missing);
}

std::string CGameDatabase::GetPlayPathForGame(int idGame)
{
  // The default release's file, which is the one the library opens
  return GetSingleValue(
      PrepareSQL("SELECT strPath || strFilename FROM files JOIN gamerelease ON "
                 "gamerelease.idRelease = files.idRelease JOIN path ON path.idPath = files.idPath "
                 "WHERE gamerelease.idGame = %i ORDER BY files.discNumber, files.idFile LIMIT 1",
                 idGame));
}

int CGameDatabase::FindGameForImport(const tinyxml2::XMLElement* element, const CGameInfoTag& tag)
{
  // The file it was exported from, if that file is still where it was
  if (const tinyxml2::XMLElement* pathElement = element->FirstChildElement("path");
      pathElement != nullptr && pathElement->GetText() != nullptr)
  {
    if (const int idGame = GetGameIdByFile(pathElement->GetText()); idGame > 0)
      return idGame;
  }

  // Otherwise whichever catalogue named it, which survives the files moving
  for (const auto& [type, value] : tag.GetUniqueIDs())
  {
    const int idGame = GetSingleValueInt(
        PrepareSQL("SELECT media_id FROM uniqueid WHERE media_type = '%s' AND type = '%s' AND "
                   "value = '%s'",
                   MediaTypeGame, type.c_str(), value.c_str()));
    if (idGame > 0)
      return idGame;
  }

  return -1;
}
