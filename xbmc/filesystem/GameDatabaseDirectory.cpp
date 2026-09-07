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
#include "media/MediaType.h"
#include "utils/SortUtils.h"
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
struct OverviewChild
{
  std::string_view segment;
  int label;
  int description;
};

constexpr std::array<OverviewChild, 23> overviewChildren{{
    {"titles", 35544, 35640},
    {"genres", 135, 35641},
    {"years", 652, 35642},
    {"developers", 35521, 35643},
    {"publishers", 35522, 35644},
    {"collections", 35523, 35645},
    {"tags", 20459, 35646},
    {"regions", 35524, 35647},
    {"players", 35525, 35648},
    {"ageratings", 35526, 35649},
    {"categories", 35527, 35650},
    {"recentlyadded", 35528, 35651},
    {"recentlyplayed", 35529, 35652},
    {"neverplayed", 35530, 35653},
    {"favourites", 1036, 35654},
    {"completed", 35531, 35655},
    {"multiplayer", 35532, 35656},
    {"coop", 35533, 35657},
    {"achievements", 35534, 35658},
    {"inprogress", 35535, 35659},
    {"hacks", 35536, 35660},
    {"homebrew", 35537, 35661},
    {"needsattention", 35562, 35662},
}};

std::string Localize(int id)
{
  return CServiceBroker::GetResourcesComponent().GetLocalizeStrings().Get(id);
}

int LabelForSegment(std::string_view segment)
{
  if (segment == "platforms")
    return 35520;
  for (const auto& [name, label, description] : overviewChildren)
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

      for (const auto& [segment, label, description] : overviewChildren)
      {
        const auto item = std::make_shared<CFileItem>(Localize(label));
        item->SetPath(base + std::string(segment) + "/");
        item->SetFolder(true);
        // A line on what is down there. Several of these are not obvious from
        // the name alone -- what counts as a hack, or as needing attention.
        item->SetProperty("description", Localize(description));
        // A glyph for the node, named after it. These ship with Kodi rather
        // than with a skin, so every skin draws the same thing here instead of
        // falling back to a folder that says nothing about where it leads.
        const std::string glyph =
            "special://xbmc/media/gamelibrary/" + std::string(segment) + ".png";
        item->SetArt("icon", glyph);
        item->SetArt("thumb", glyph);
        // The rest of these are filters and read well in alphabetical order.
        // This one is not a filter, it is the way to the games themselves, and
        // sorting it under A leaves it second behind age ratings.
        if (segment == "titles")
          item->SetSpecialSort(SortSpecial::TOP);
        items.Add(item);
      }

      if (!dbUrl.HasPlatform())
      {
        // The saved lists live in the profile, not in the library
        const auto playlists = std::make_shared<CFileItem>(Localize(136));
        playlists->SetPath("special://gameplaylists/");
        playlists->SetFolder(true);
        items.Add(playlists);
      }
      // Inside a platform these all belong to that machine, and none of them
      // has art of its own. Lend them the machine's, so a skin's art panel
      // shows what is being browsed rather than a folder with no picture.
      if (dbUrl.HasPlatform())
      {
        CVariant platformId;
        KODI::ART::Artwork art;
        if (dbUrl.GetOption("platformid", platformId) &&
            db.GetArtForItem(static_cast<int>(platformId.asInteger()), MediaTypeGamePlatform, art))
        {
          for (const auto& item : items)
          {
            // Merged rather than assigned, and not over what the node draws
            // itself with. The machine's art is the backdrop here; the glyph
            // says where the row leads, and a skin reading ListItem.Icon
            // resolves that through the thumb.
            for (const auto& [type, url] : art)
            {
              // Only the backdrop, and only that. Lending every type hands a
              // skin the machine's photo or its controller to draw as the row's
              // own picture, and then every way into the platform looks the
              // same as every other. Types arrive numbered too -- fanart1,
              // fanart2 -- so the digits come off before the comparison.
              const std::string kind = type.substr(0, type.find_last_not_of("0123456789") + 1);
              if (kind == "fanart")
                item->SetArt(type, url);
            }
          }
        }
      }

      // These are ways into the library, not things in it. Naming the content
      // lets a skin lay them out as navigation; left blank, the window fills it
      // in as "games" and they are drawn as though each folder were a game.
      items.SetContent("overview");
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
