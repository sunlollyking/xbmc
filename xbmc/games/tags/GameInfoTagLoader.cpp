/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "GameInfoTagLoader.h"

#include "FileItem.h"
#include "GameInfoTag.h"
#include "URL.h"
#include "utils/ArtUtils.h"
#include "utils/FileUtils.h"
#include "utils/URIUtils.h"
#include "utils/XBMCTinyXML2.h"
#include "utils/log.h"

#include <cstring>
#include <string>
#include <vector>

#include <tinyxml2.h>

using namespace KODI;
using namespace GAME;

namespace
{
constexpr auto XML_ROOT = "game";
constexpr auto NFO_EXTENSION = ".nfo";
constexpr auto FOLDER_NFO = "folder.nfo";

//! \brief The NFO belonging to an item, whether or not it exists
std::string GetNFOPath(const CFileItem& item)
{
  if (item.IsFolder())
    return ART::GetFolderThumb(item, FOLDER_NFO);

  return URIUtils::ReplaceExtension(item.GetPath(), NFO_EXTENSION);
}

} // namespace

bool CGameInfoTagLoader::HasNFO(const CFileItem& item)
{
  const std::string nfoPath = GetNFOPath(item);
  return !nfoPath.empty() && CFileUtils::Exists(nfoPath);
}

bool CGameInfoTagLoader::Load(const CFileItem& item, CGameInfoTag& tag)
{
  const std::string nfoPath = GetNFOPath(item);
  if (nfoPath.empty() || !CFileUtils::Exists(nfoPath))
    return false;

  CXBMCTinyXML2 xmlDoc;
  if (!xmlDoc.LoadFile(nfoPath))
  {
    CLog::Log(LOGWARNING, "Failed to load game NFO {}: {} at line {}", CURL::GetRedacted(nfoPath),
              xmlDoc.ErrorStr(), xmlDoc.ErrorLineNum());
    return false;
  }

  const tinyxml2::XMLElement* rootElement = xmlDoc.RootElement();
  if (rootElement == nullptr || std::strcmp(rootElement->Name(), XML_ROOT) != 0)
  {
    CLog::Log(LOGWARNING, "Failed to parse game NFO {}, missing root element '{}'",
              CURL::GetRedacted(nfoPath), XML_ROOT);
    return false;
  }

  CLog::Log(LOGDEBUG, "Loading game NFO {}", CURL::GetRedacted(nfoPath));

  return tag.Load(rootElement);
}

bool CGameInfoTagLoader::Save(const CFileItem& item, const CGameInfoTag& tag)
{
  const std::string nfoPath = GetNFOPath(item);
  if (nfoPath.empty())
    return false;

  CXBMCTinyXML2 xmlDoc;
  xmlDoc.InsertEndChild(xmlDoc.NewDeclaration());
  tag.Save(&xmlDoc, XML_ROOT);

  if (!xmlDoc.SaveFile(nfoPath))
  {
    CLog::Log(LOGERROR, "Failed to write game NFO {}: {}", CURL::GetRedacted(nfoPath),
              xmlDoc.ErrorStr());
    return false;
  }
  return true;
  return true;
}
