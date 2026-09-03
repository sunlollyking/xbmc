/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "GameLibrary.h"

#include "FileItem.h"
#include "FileItemList.h"
#include "ServiceBroker.h"
#include "games/database/GameDatabase.h"
#include "games/library/GameDbUrl.h"
#include "games/library/GameLibraryQueue.h"
#include "games/tags/GameInfoTag.h"
#include "interfaces/AnnouncementManager.h"
#include "utils/SortUtils.h"
#include "utils/StringUtils.h"
#include "utils/URIUtils.h"
#include "utils/Variant.h"

using namespace JSONRPC;
using namespace KODI::GAME;

namespace
{
std::string PlatformScope(const CVariant& parameterObject)
{
  const int idPlatform = static_cast<int>(parameterObject["platformid"].asInteger(-1));
  if (idPlatform > 0)
    return "gamedb://platforms/" + std::to_string(idPlatform) + "/";
  return "gamedb://";
}

// The facet a method name asks for, as the path segment the library lists it by
std::string FacetSegment(const std::string& method)
{
  const std::string facet = StringUtils::ToLower(method.substr(method.find('.') + 4));
  return facet; // "genres", "developers", ...
}

void FillPlatformItem(CFileItem& item, const PlatformInfo& platform)
{
  CGameInfoTag* tag = item.GetGameInfoTag();
  tag->SetTitle(platform.name);
  tag->SetPlatform(platform.name);
  tag->SetPlatformId(platform.id);
  tag->SetPlatformSlug(platform.slug);
  tag->SetLoaded(true);
  item.SetProperty("manufacturer", platform.manufacturer);
  item.SetProperty("platformtype", std::string(CGameLibraryTypes::ToString(platform.type)));
  item.SetProperty("released", platform.released);
  item.SetProperty("gamecount", platform.gameCount);
}
} // namespace

JSONRPC_STATUS CGameLibrary::GetPlatforms(const std::string& method,
                                          ITransportLayer* transport,
                                          IClient* client,
                                          const CVariant& parameterObject,
                                          CVariant& result)
{
  CGameDatabase db;
  if (!db.Open())
    return InternalError;

  std::vector<PlatformInfo> platforms;
  if (!db.GetPlatforms(platforms, !parameterObject["all"].asBoolean()))
    return InternalError;

  CFileItemList items;
  for (const PlatformInfo& platform : platforms)
  {
    const auto item = std::make_shared<CFileItem>(platform.name);
    item->SetPath("gamedb://platforms/" + std::to_string(platform.id) + "/");
    item->SetFolder(true);
    FillPlatformItem(*item, platform);
    KODI::ART::Artwork art;
    if (db.GetArtForItem(platform.id, MediaTypeGamePlatform, art))
      item->SetArt(art);
    items.Add(item);
  }

  HandleFileItemList("platformid", false, "platforms", items, parameterObject, result);
  return OK;
}

JSONRPC_STATUS CGameLibrary::GetPlatformDetails(const std::string& method,
                                                ITransportLayer* transport,
                                                IClient* client,
                                                const CVariant& parameterObject,
                                                CVariant& result)
{
  CGameDatabase db;
  if (!db.Open())
    return InternalError;

  PlatformInfo platform;
  if (!db.GetPlatform(static_cast<int>(parameterObject["platformid"].asInteger()), platform))
    return InvalidParams;

  const auto item = std::make_shared<CFileItem>(platform.name);
  item->SetPath("gamedb://platforms/" + std::to_string(platform.id) + "/");
  item->SetFolder(true);
  FillPlatformItem(*item, platform);
  KODI::ART::Artwork art;
  if (db.GetArtForItem(platform.id, MediaTypeGamePlatform, art))
    item->SetArt(art);

  HandleFileItem("platformid", true, "platformdetails", item, parameterObject,
                 parameterObject["properties"], result, false);
  return OK;
}

JSONRPC_STATUS CGameLibrary::GetGames(const std::string& method,
                                      ITransportLayer* transport,
                                      IClient* client,
                                      const CVariant& parameterObject,
                                      CVariant& result)
{
  CGameDatabase db;
  if (!db.Open())
    return InternalError;

  const CVariant& filter = parameterObject["filter"];
  int idPlatform = static_cast<int>(parameterObject["platformid"].asInteger(-1));
  if (filter.isMember("platformid"))
    idPlatform = static_cast<int>(filter["platformid"].asInteger());

  std::string base = idPlatform > 0 ? "gamedb://platforms/" + std::to_string(idPlatform) + "/"
                                    : "gamedb://";
  if (filter.isMember("list"))
    base += filter["list"].asString() + "/";
  else
    base += "titles/";

  CGameDbUrl url;
  if (!url.FromString(base))
    return InternalError;

  for (const char* key : {"genreid", "developerid", "publisherid", "collectionid", "tagid", "year",
                          "players"})
  {
    if (filter.isMember(key))
      url.AddOption(key, static_cast<int>(filter[key].asInteger()));
  }
  for (const char* key : {"region", "category"})
  {
    if (filter.isMember(key))
      url.AddOption(key, filter[key].asString());
  }

  SortDescription sorting;
  ParseLimits(parameterObject, sorting.limitStart, sorting.limitEnd);
  if (!ParseSorting(parameterObject, sorting.sortBy, sorting.sortOrder, sorting.sortAttributes))
    return InvalidParams;

  CFileItemList items;
  if (!db.GetGamesNav(url.ToString(), items, sorting))
    return InternalError;

  int size = items.Size();
  if (items.HasProperty("total") && items.GetProperty("total").asInteger() > size)
    size = static_cast<int>(items.GetProperty("total").asInteger());
  HandleFileItemList("gameid", true, "games", items, parameterObject, result, size, false);
  return OK;
}

JSONRPC_STATUS CGameLibrary::GetGameDetails(const std::string& method,
                                            ITransportLayer* transport,
                                            IClient* client,
                                            const CVariant& parameterObject,
                                            CVariant& result)
{
  CGameDatabase db;
  if (!db.Open())
    return InternalError;

  const int idGame = static_cast<int>(parameterObject["gameid"].asInteger());
  CGameInfoTag tag;
  if (!db.GetGameInfo(idGame, tag))
    return InvalidParams;

  const auto item = std::make_shared<CFileItem>(tag.GetTitle());
  *item->GetGameInfoTag() = tag;
  if (!tag.GetURL().empty())
    item->SetPath(tag.GetURL());
  item->SetProperty("gameid", idGame);
  KODI::ART::Artwork art;
  if (db.GetArtForItem(idGame, MediaTypeGame, art))
    item->SetArt(art);

  HandleFileItem("gameid", true, "gamedetails", item, parameterObject,
                 parameterObject["properties"], result, false);
  return OK;
}

JSONRPC_STATUS CGameLibrary::GetFacet(const std::string& method,
                                      ITransportLayer* transport,
                                      IClient* client,
                                      const CVariant& parameterObject,
                                      CVariant& result)
{
  CGameDatabase db;
  if (!db.Open())
    return InternalError;

  const std::string facet = FacetSegment(method);
  const std::string path = PlatformScope(parameterObject) + facet + "/";

  CFileItemList items;
  if (!db.GetFacetNav(path, items))
    return InternalError;

  // A facet value is named by its label and identified by its path segment
  for (const auto& item : items)
  {
    CGameInfoTag* tag = item->GetGameInfoTag();
    tag->SetTitle(item->GetLabel());
    tag->SetLoaded(true);
    std::string segment = item->GetPath();
    URIUtils::RemoveSlashAtEnd(segment);
    segment = URIUtils::GetFileName(segment);
    if (StringUtils::IsNaturalNumber(segment))
      tag->SetDatabaseId(std::stoi(segment));
    item->SetProperty("value", segment);
  }

  HandleFileItemList("genreid", false, facet.c_str(), items, parameterObject, result);
  return OK;
}

JSONRPC_STATUS CGameLibrary::SetGameDetails(const std::string& method,
                                            ITransportLayer* transport,
                                            IClient* client,
                                            const CVariant& parameterObject,
                                            CVariant& result)
{
  CGameDatabase db;
  if (!db.Open())
    return InternalError;

  const int idGame = static_cast<int>(parameterObject["gameid"].asInteger());
  CGameInfoTag tag;
  if (!db.GetGameInfo(idGame, tag))
    return InvalidParams;

  if (ParameterNotNull(parameterObject, "userrating"))
    db.SetUserRating(idGame, static_cast<int>(parameterObject["userrating"].asInteger()));
  if (ParameterNotNull(parameterObject, "favourite"))
    db.SetFavourite(idGame, parameterObject["favourite"].asBoolean());
  if (ParameterNotNull(parameterObject, "completed"))
    db.SetCompleted(idGame, parameterObject["completed"].asBoolean());
  if (ParameterNotNull(parameterObject, "defaultreleaseid"))
  {
    if (!db.SetDefaultRelease(idGame, static_cast<int>(parameterObject["defaultreleaseid"].asInteger())))
      return InvalidParams;
  }

  CVariant data;
  data["id"] = idGame;
  data["type"] = "game";
  CServiceBroker::GetAnnouncementManager()->Announce(ANNOUNCEMENT::GameLibrary, "OnUpdate", data);
  return ACK;
}

JSONRPC_STATUS CGameLibrary::RefreshGame(const std::string& method,
                                         ITransportLayer* transport,
                                         IClient* client,
                                         const CVariant& parameterObject,
                                         CVariant& result)
{
  const int idGame = static_cast<int>(parameterObject["gameid"].asInteger());
  CGameDatabase db;
  CGameInfoTag tag;
  if (!db.Open() || !db.GetGameInfo(idGame, tag))
    return InvalidParams;

  CGameLibraryQueue::GetInstance().RefreshGame(idGame);
  return ACK;
}

JSONRPC_STATUS CGameLibrary::RemoveGame(const std::string& method,
                                        ITransportLayer* transport,
                                        IClient* client,
                                        const CVariant& parameterObject,
                                        CVariant& result)
{
  const int idGame = static_cast<int>(parameterObject["gameid"].asInteger());
  CGameDatabase db;
  CGameInfoTag tag;
  if (!db.Open() || !db.GetGameInfo(idGame, tag))
    return InvalidParams;
  if (!db.DeleteGame(idGame))
    return InternalError;

  CVariant data;
  data["id"] = idGame;
  data["type"] = "game";
  CServiceBroker::GetAnnouncementManager()->Announce(ANNOUNCEMENT::GameLibrary, "OnRemove", data);
  return ACK;
}

JSONRPC_STATUS CGameLibrary::Scan(const std::string& method,
                                  ITransportLayer* transport,
                                  IClient* client,
                                  const CVariant& parameterObject,
                                  CVariant& result)
{
  CGameLibraryQueue::GetInstance().ScanLibrary(parameterObject["directory"].asString(),
                                               parameterObject["showdialogs"].asBoolean());
  return ACK;
}

JSONRPC_STATUS CGameLibrary::Clean(const std::string& method,
                                   ITransportLayer* transport,
                                   IClient* client,
                                   const CVariant& parameterObject,
                                   CVariant& result)
{
  if (!CGameLibraryQueue::GetInstance().CleanLibrary({}, false))
    return FailedToExecute;
  return ACK;
}
