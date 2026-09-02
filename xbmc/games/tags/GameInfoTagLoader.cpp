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
#include "utils/StringUtils.h"
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

//! \brief The text of a child element, or an empty string
std::string GetText(const tinyxml2::XMLElement* parent, const char* name)
{
  const tinyxml2::XMLElement* element = parent->FirstChildElement(name);
  if (element == nullptr || element->GetText() == nullptr)
    return "";

  std::string text = element->GetText();
  StringUtils::Trim(text);
  return text;
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

  if (const std::string title = GetText(rootElement, "title"); !title.empty())
    tag.SetTitle(title);

  // "plot" is accepted alongside "overview" because that is what every other
  // NFO in Kodi calls it, and a collection is likely to have been written for
  // those first
  std::string overview = GetText(rootElement, "overview");
  if (overview.empty())
    overview = GetText(rootElement, "plot");
  if (!overview.empty())
    tag.SetOverview(overview);

  if (const std::string platform = GetText(rootElement, "platform"); !platform.empty())
    tag.SetPlatform(platform);

  if (const std::string developer = GetText(rootElement, "developer"); !developer.empty())
    tag.SetDeveloper(developer);

  if (const std::string publisher = GetText(rootElement, "publisher"); !publisher.empty())
    tag.SetPublisher(publisher);

  if (const std::string region = GetText(rootElement, "region"); !region.empty())
    tag.SetRegion(region);

  if (const std::string year = GetText(rootElement, "year"); !year.empty())
  {
    if (StringUtils::IsNaturalNumber(year))
      tag.SetYear(static_cast<unsigned int>(std::stoul(year)));
  }

  std::vector<std::string> genres;
  for (const tinyxml2::XMLElement* genre = rootElement->FirstChildElement("genre"); genre != nullptr;
       genre = genre->NextSiblingElement("genre"))
  {
    if (genre->GetText() == nullptr)
      continue;

    std::string text = genre->GetText();
    StringUtils::Trim(text);
    if (!text.empty())
      genres.emplace_back(std::move(text));
  }
  if (!genres.empty())
    tag.SetGenres(genres);

  return true;
}
