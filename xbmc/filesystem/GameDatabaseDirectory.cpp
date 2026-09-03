/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "GameDatabaseDirectory.h"

#include "FileItem.h"
#include "FileItemList.h"
#include "URL.h"
#include "games/database/GameDatabase.h"
#include "games/library/GameDbUrl.h"
#include "games/tags/GameInfoTag.h"
#include "ServiceBroker.h"
#include "resources/LocalizeStrings.h"
#include "resources/ResourcesComponent.h"
#include "utils/URIUtils.h"
#include "utils/Variant.h"
#include "utils/log.h"

#include <array>
#include <utility>

using namespace KODI::GAME;
using namespace XFILE;

namespace
{
// Each entry an overview node lists, and the string it is labelled with
constexpr std::array<std::pair<std::string_view, int>, 22> overviewChildren{{
    {"titles", 35544},
    {"genres", 135},
    {"years", 652},
    {"developers", 35521},
    {"publishers", 35522},
    {"collections", 35523},
    {"tags", 20459},
    {"regions", 35524},
    {"players", 35525},
    {"ageratings", 35526},
    {"categories", 35527},
    {"recentlyadded", 35528},
    {"recentlyplayed", 35529},
    {"neverplayed", 35530},
    {"favourites", 1036},
    {"completed", 35531},
    {"multiplayer", 35532},
    {"coop", 35533},
    {"achievements", 35534},
    {"inprogress", 35535},
    {"hacks", 35536},
    {"homebrew", 35537},
}};

std::string Localize(int id)
{
  return CServiceBroker::GetResourcesComponent().GetLocalizeStrings().Get(id);
}

int LabelForSegment(std::string_view segment)
{
  if (segment == "platforms")
    return 35520;
  for (const auto& [name, label] : overviewChildren)
  {
    if (name == segment)
      return label;
  }
  return -1;
}
} // namespace

CGameDatabaseDirectory::CGameDatabaseDirectory() = default;

CGameDatabaseDirectory::~CGameDatabaseDirectory() = default;

bool CGameDatabaseDirectory::GetDirectory(const CURL& url, CFileItemList& items)
{
  const std::string path = url.Get();

  CGameDbUrl dbUrl;
  if (!dbUrl.FromString(path))
  {
    CLog::Log(LOGWARNING, "GAME: {} is not a library path", path);
    return false;
  }

  CGameDatabase db;
  if (!db.Open())
    return false;

  items.SetPath(path);

  switch (dbUrl.GetNode())
  {
    case GameDbNode::OVERVIEW:
    {
      std::string base = path;
      URIUtils::AddSlashAtEnd(base);

      if (!dbUrl.HasPlatform())
      {
        const auto item = std::make_shared<CFileItem>(Localize(35520));
        item->SetPath(base + "platforms/");
        item->SetFolder(true);
        items.Add(item);
      }

      for (const auto& [segment, label] : overviewChildren)
      {
        const auto item = std::make_shared<CFileItem>(Localize(label));
        item->SetPath(base + std::string(segment) + "/");
        item->SetFolder(true);
        items.Add(item);
      }
      items.SetContent("");
      return true;
    }
    case GameDbNode::PLATFORMS:
      return db.GetPlatformsNav(path, items);
    case GameDbNode::GAMES:
      return db.GetGamesNav(path, items, SortDescription());
    case GameDbNode::RELEASES:
      return db.GetReleasesNav(path, items);
    case GameDbNode::GENRES:
    case GameDbNode::YEARS:
    case GameDbNode::DEVELOPERS:
    case GameDbNode::PUBLISHERS:
    case GameDbNode::COLLECTIONS:
    case GameDbNode::TAGS:
    case GameDbNode::REGIONS:
    case GameDbNode::PLAYERS:
    case GameDbNode::AGERATINGS:
    case GameDbNode::CATEGORIES:
      return db.GetFacetNav(path, items);
    default:
      break;
  }

  return false;
}

bool CGameDatabaseDirectory::Exists(const CURL& url)
{
  CGameDbUrl dbUrl;
  return dbUrl.FromString(url.Get());
}

std::string CGameDatabaseDirectory::GetLabel(const std::string& path)
{
  CGameDbUrl dbUrl;
  if (!dbUrl.FromString(path))
    return "";

  std::string trimmed = path;
  URIUtils::RemoveSlashAtEnd(trimmed);
  const std::string last = URIUtils::GetFileName(trimmed);

  const int label = LabelForSegment(last);
  if (label > 0)
    return Localize(label);

  if (dbUrl.GetNode() == GameDbNode::OVERVIEW && dbUrl.HasPlatform())
  {
    CVariant platformId;
    dbUrl.GetOption("platformid", platformId);
    CGameDatabase db;
    PlatformInfo platform;
    if (db.Open() && db.GetPlatform(static_cast<int>(platformId.asInteger()), platform))
      return platform.name;
  }

  if (dbUrl.GetNode() == GameDbNode::RELEASES)
  {
    CVariant gameId;
    dbUrl.GetOption("gameid", gameId);
    CGameDatabase db;
    CGameInfoTag game;
    if (db.Open() && db.GetGameInfo(static_cast<int>(gameId.asInteger()), game))
      return game.GetTitle();
  }

  return "";
}
