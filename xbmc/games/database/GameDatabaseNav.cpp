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
#include "GameDatabaseColumns.h"
#include "ServiceBroker.h"
#include "URL.h"
#include "dbwrappers/dataset.h"
#include "games/library/GameDbUrl.h"
#include "games/tags/GameInfoTag.h"
#include "settings/Settings.h"
#include "settings/SettingsComponent.h"
#include "utils/DatabaseUtils.h"
#include "utils/SortUtils.h"
#include "utils/StringUtils.h"
#include "utils/URIUtils.h"
#include "utils/Variant.h"
#include "utils/log.h"

using namespace KODI;
using namespace GAME;

namespace
{
constexpr int RECENT_LIMIT = 25;

bool ShowDerivedGames()
{
  const auto settings = CServiceBroker::GetSettingsComponent()->GetSettings();
  return settings && settings->GetBool(SETTING_GAMELIBRARY_SHOWDERIVEDGAMES);
}

std::string ReleaseLabel(const std::string& gameTitle, const GameRelease& release)
{
  std::vector<std::string> parts;
  if (!release.regions.empty())
    parts.emplace_back(StringUtils::ToUpper(StringUtils::Join(release.regions, ", ")));
  if (!release.languages.empty())
    parts.emplace_back(StringUtils::Join(release.languages, ","));
  if (!release.revision.empty())
    parts.emplace_back(release.revision);
  if (release.status != ReleaseStatus::RETAIL)
    parts.emplace_back(std::string(CGameLibraryTypes::ToString(release.status)));
  if (release.licence != Licence::LICENSED)
    parts.emplace_back(std::string(CGameLibraryTypes::ToString(release.licence)));

  std::string label = gameTitle;
  if (!parts.empty())
    label += " (" + StringUtils::Join(parts, ") (") + ")";
  return label;
}
} // namespace

bool CGameDatabase::GetFilter(CDbUrl& dbUrl, Filter& filter, SortDescription& sorting)
{
  if (!dbUrl.IsValid() || dbUrl.GetType() != "games")
    return false;

  return GetFilter(static_cast<CGameDbUrl&>(dbUrl), filter, sorting);
}

bool CGameDatabase::GetFilter(CGameDbUrl& url, Filter& filter, SortDescription& sorting)
{
  const CUrlOptions::UrlOptions& options = url.GetOptions();

  auto option = [&options](const char* key) -> const CVariant*
  {
    const auto it = options.find(key);
    return it == options.end() ? nullptr : &it->second;
  };

  if (const CVariant* v = option("platformid"))
    filter.AppendWhere(PrepareSQL("game_view.idPlatform = %i", static_cast<int>(v->asInteger())));
  if (const CVariant* v = option("gameid"))
    filter.AppendWhere(PrepareSQL("game_view.idGame = %i", static_cast<int>(v->asInteger())));
  if (const CVariant* v = option("genreid"))
  {
    filter.AppendJoin("JOIN genre_link ON genre_link.idGame = game_view.idGame");
    filter.AppendWhere(PrepareSQL("genre_link.idGenre = %i", static_cast<int>(v->asInteger())));
  }
  if (const CVariant* v = option("tagid"))
  {
    filter.AppendJoin("JOIN tag_link ON tag_link.idGame = game_view.idGame");
    filter.AppendWhere(PrepareSQL("tag_link.idTag = %i", static_cast<int>(v->asInteger())));
  }
  if (const CVariant* v = option("developerid"))
  {
    filter.AppendJoin("JOIN developer_link ON developer_link.idGame = game_view.idGame");
    filter.AppendWhere(
        PrepareSQL("developer_link.idCompany = %i", static_cast<int>(v->asInteger())));
  }
  if (const CVariant* v = option("publisherid"))
  {
    filter.AppendJoin("JOIN publisher_link ON publisher_link.idGame = game_view.idGame");
    filter.AppendWhere(
        PrepareSQL("publisher_link.idCompany = %i", static_cast<int>(v->asInteger())));
  }
  if (const CVariant* v = option("collectionid"))
  {
    filter.AppendJoin("JOIN collection_link ON collection_link.idGame = game_view.idGame");
    filter.AppendWhere(
        PrepareSQL("collection_link.idCollection = %i", static_cast<int>(v->asInteger())));
  }
  if (const CVariant* v = option("year"))
    filter.AppendWhere(PrepareSQL("game_view.year = %i", static_cast<int>(v->asInteger())));
  if (const CVariant* v = option("players"))
    filter.AppendWhere(PrepareSQL("game_view.playersMax = %i", static_cast<int>(v->asInteger())));
  if (const CVariant* v = option("region"))
  {
    filter.AppendWhere(PrepareSQL(
        "EXISTS (SELECT 1 FROM gamerelease r JOIN release_region rr ON rr.idRelease = "
        "r.idRelease JOIN region ON region.idRegion = rr.idRegion WHERE r.idGame = "
        "game_view.idGame AND region.code = '%s')",
        v->asString().c_str()));
  }
  if (const CVariant* v = option("agerating"))
  {
    filter.AppendWhere(
        PrepareSQL("EXISTS (SELECT 1 FROM agerating WHERE agerating.media_id = game_view.idGame "
                   "AND agerating.media_type = '%s' AND agerating.value = '%s')",
                   MediaTypeGame, v->asString().c_str()));
  }

  bool derivedShown = ShowDerivedGames();
  if (const CVariant* v = option("category"))
  {
    filter.AppendWhere(PrepareSQL("game_view.category = '%s'", v->asString().c_str()));
    derivedShown = true;
  }

  const std::string& list = url.GetList();
  if (list == "recentlyadded")
  {
    filter.AppendOrder("game_view.dateAdded DESC");
    if (sorting.sortBy == SortBy::NONE && sorting.limitEnd == 0)
      filter.limit = PrepareSQL("%i", RECENT_LIMIT);
  }
  else if (list == "recentlyplayed")
  {
    filter.AppendWhere("game_view.lastPlayed IS NOT NULL AND game_view.lastPlayed <> ''");
    filter.AppendOrder("game_view.lastPlayed DESC");
    if (sorting.sortBy == SortBy::NONE && sorting.limitEnd == 0)
      filter.limit = PrepareSQL("%i", RECENT_LIMIT);
  }
  else if (list == "neverplayed")
    filter.AppendWhere("game_view.playCount = 0");
  else if (list == "favourites")
    filter.AppendWhere("game_view.favourite = 1");
  else if (list == "completed")
    filter.AppendWhere("game_view.completed = 1");
  else if (list == "multiplayer")
    filter.AppendWhere("game_view.playersMax >= 2");
  else if (list == "coop")
    filter.AppendWhere("game_view.coop = 1");
  else if (list == "achievements")
    filter.AppendWhere("game_view.achievementsTotal > 0");
  else if (list == "inprogress")
    filter.AppendWhere("game_view.playCount > 0 AND game_view.completed = 0");
  else if (list == "hacks")
  {
    filter.AppendWhere("game_view.category = 'hack'");
    derivedShown = true;
  }
  else if (list == "homebrew")
  {
    filter.AppendWhere("game_view.category = 'homebrew'");
    derivedShown = true;
  }

  if (!derivedShown)
    filter.AppendWhere("game_view.category IN ('retail', 'demo')");

  filter.AppendWhere("game_view.hidden = 0");

  return true;
}

bool CGameDatabase::GetGamesNav(const std::string& baseDir,
                                CFileItemList& items,
                                const SortDescription& sortDescription)
{
  return GetGamesByWhere(baseDir, Filter(), items, sortDescription);
}

bool CGameDatabase::GetGamesByWhere(const std::string& baseDir,
                                    const Filter& filter,
                                    CFileItemList& items,
                                    const SortDescription& sortDescription)
{
  try
  {
    if (nullptr == m_pDB || nullptr == m_pDS)
      return false;

    CGameDbUrl url;
    Filter extFilter = filter;
    SortDescription sorting = sortDescription;
    if (!url.FromString(baseDir) || !GetFilter(url, extFilter, sorting))
      return false;

    int total = -1;

    std::string strSQL = "SELECT %s FROM game_view ";
    std::string strSQLExtra;
    if (!CDatabase::BuildSQL(strSQLExtra, extFilter, strSQLExtra))
      return false;

    if (extFilter.limit.empty() && sorting.sortBy == SortBy::NONE &&
        (sorting.limitStart > 0 || sorting.limitEnd > 0 ||
         (sorting.limitStart == 0 && sorting.limitEnd == 0)))
    {
      total = GetSingleValueInt(PrepareSQL(strSQL, "COUNT(1)") + strSQLExtra, *m_pDS);
      strSQLExtra += DatabaseUtils::BuildLimitClause(sorting.limitEnd, sorting.limitStart);
    }

    strSQL = PrepareSQL(strSQL, "game_view.*") + strSQLExtra;

    const int rowsFound = RunQuery(strSQL);
    if (total < rowsFound)
      total = rowsFound;
    items.SetProperty("total", total);

    if (rowsFound <= 0)
      return rowsFound == 0;

    DatabaseResults results;
    results.reserve(rowsFound);
    if (!SortUtils::SortFromDataset(sortDescription, MediaTypeGame, *m_pDS, results))
      return false;

    items.Reserve(results.size());
    const dbiplus::query_data& data = m_pDS->get_result_set().records;
    for (const auto& result : results)
    {
      const auto row = static_cast<unsigned int>(result.at(Field::ROW).asInteger());
      const dbiplus::sql_record* const record = data.at(row);

      CGameInfoTag game;
      GetDetailsForGame(record, game);

      const auto item = std::make_shared<CFileItem>(game.GetTitle());
      *item->GetGameInfoTag() = game;

      // A game plays its default release; with no file it can only be browsed
      if (!game.GetURL().empty())
      {
        item->SetPath(game.GetURL());
        item->SetFolder(false);
      }
      else
      {
        CGameDbUrl itemUrl{url};
        itemUrl.AppendPath(std::to_string(game.GetDatabaseId()) + "/");
        item->SetPath(itemUrl.ToString());
        item->SetFolder(true);
      }
      item->SetProperty("gameid", game.GetDatabaseId());
      item->SetLabelPreformatted(true);

      items.Add(item);
    }

    m_pDS->close();
    items.SetContent("games");
    return true;
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "GAME: Failed to list games for {}", baseDir);
  }
  return false;
}

bool CGameDatabase::GetFacetNav(const std::string& baseDir, CFileItemList& items)
{
  try
  {
    CGameDbUrl url;
    if (!url.FromString(baseDir))
      return false;

    // The scope every facet is counted within
    Filter scope;
    SortDescription sorting;
    if (!GetFilter(url, scope, sorting))
      return false;
    const std::string where = scope.where.empty() ? "" : " WHERE " + scope.where;
    const std::string join = scope.join;

    std::string sql;
    bool numericValue = true;
    switch (url.GetNode())
    {
      case GameDbNode::GENRES:
        sql = "SELECT genre.idGenre AS id, genre.name AS label, COUNT(DISTINCT game_view.idGame) "
              "AS n FROM genre JOIN genre_link ON genre_link.idGenre = genre.idGenre JOIN "
              "game_view ON game_view.idGame = genre_link.idGame " +
              join + where + " GROUP BY genre.idGenre, genre.name ORDER BY genre.name";
        break;
      case GameDbNode::TAGS:
        sql = "SELECT tag.idTag AS id, tag.name AS label, COUNT(DISTINCT game_view.idGame) AS n "
              "FROM tag JOIN tag_link ON tag_link.idTag = tag.idTag JOIN game_view ON "
              "game_view.idGame = tag_link.idGame " +
              join + where + " GROUP BY tag.idTag, tag.name ORDER BY tag.name";
        break;
      case GameDbNode::DEVELOPERS:
        sql = "SELECT company.idCompany AS id, company.name AS label, COUNT(DISTINCT "
              "game_view.idGame) AS n FROM company JOIN developer_link ON developer_link.idCompany "
              "= company.idCompany JOIN game_view ON game_view.idGame = developer_link.idGame " +
              join + where + " GROUP BY company.idCompany, company.name ORDER BY company.name";
        break;
      case GameDbNode::PUBLISHERS:
        sql = "SELECT company.idCompany AS id, company.name AS label, COUNT(DISTINCT "
              "game_view.idGame) AS n FROM company JOIN publisher_link ON publisher_link.idCompany "
              "= company.idCompany JOIN game_view ON game_view.idGame = publisher_link.idGame " +
              join + where + " GROUP BY company.idCompany, company.name ORDER BY company.name";
        break;
      case GameDbNode::COLLECTIONS:
        sql = "SELECT collection.idCollection AS id, collection.name AS label, COUNT(DISTINCT "
              "game_view.idGame) AS n FROM collection JOIN collection_link ON "
              "collection_link.idCollection = collection.idCollection JOIN game_view ON "
              "game_view.idGame = collection_link.idGame " +
              join + where +
              " GROUP BY collection.idCollection, collection.name ORDER BY collection.sortName, "
              "collection.name";
        break;
      case GameDbNode::YEARS:
        sql = "SELECT game_view.year AS id, game_view.year AS label, COUNT(DISTINCT "
              "game_view.idGame) AS n FROM game_view " +
              join + where + (where.empty() ? " WHERE " : " AND ") +
              "game_view.year > 0 GROUP BY game_view.year ORDER BY game_view.year";
        break;
      case GameDbNode::PLAYERS:
        sql = "SELECT game_view.playersMax AS id, game_view.playersMax AS label, COUNT(DISTINCT "
              "game_view.idGame) AS n FROM game_view " +
              join + where + (where.empty() ? " WHERE " : " AND ") +
              "game_view.playersMax > 0 GROUP BY game_view.playersMax ORDER BY "
              "game_view.playersMax";
        break;
      case GameDbNode::REGIONS:
        numericValue = false;
        sql = "SELECT region.code AS id, region.code AS label, COUNT(DISTINCT game_view.idGame) "
              "AS n FROM region JOIN release_region ON release_region.idRegion = region.idRegion "
              "JOIN gamerelease ON gamerelease.idRelease = release_region.idRelease JOIN "
              "game_view ON game_view.idGame = gamerelease.idGame " +
              join + where + " GROUP BY region.code ORDER BY region.code";
        break;
      case GameDbNode::AGERATINGS:
        numericValue = false;
        sql = "SELECT agerating.value AS id, agerating.value AS label, COUNT(DISTINCT "
              "game_view.idGame) AS n FROM agerating JOIN game_view ON game_view.idGame = "
              "agerating.media_id AND agerating.media_type = '" MediaTypeGame "' " +
              join + where + " GROUP BY agerating.value ORDER BY agerating.value";
        break;
      case GameDbNode::CATEGORIES:
        numericValue = false;
        sql = "SELECT game_view.category AS id, game_view.category AS label, COUNT(DISTINCT "
              "game_view.idGame) AS n FROM game_view " +
              join + where + " GROUP BY game_view.category ORDER BY game_view.category";
        break;
      default:
        return false;
    }

    std::string base = baseDir;
    URIUtils::AddSlashAtEnd(base);

    if (m_pDS->query(sql))
    {
      while (!m_pDS->eof())
      {
        const std::string id = m_pDS->fv("id").get_asString();
        const std::string label = m_pDS->fv("label").get_asString();
        const int count = m_pDS->fv("n").get_asInt();

        const auto item = std::make_shared<CFileItem>(label);
        item->SetPath(base + (numericValue ? id : CURL::Encode(id)) + "/");
        item->SetFolder(true);
        item->SetLabelPreformatted(true);
        item->SetLabel2(std::to_string(count));
        item->SetProperty("gamecount", count);
        items.Add(item);
        m_pDS->next();
      }
      m_pDS->close();
    }

    items.SetContent(url.GetItemType());
    return true;
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "GAME: Failed to list {}", baseDir);
  }
  return false;
}

bool CGameDatabase::GetReleasesNav(const std::string& baseDir, CFileItemList& items)
{
  CGameDbUrl url;
  if (!url.FromString(baseDir) || !url.HasOption("gameid"))
    return false;

  CVariant gameId;
  url.GetOption("gameid", gameId);

  CGameInfoTag game;
  if (!GetGameInfo(static_cast<int>(gameId.asInteger()), game))
    return false;

  for (const GameRelease& release : game.GetReleases())
  {
    if (release.files.empty())
      continue;

    const auto item = std::make_shared<CFileItem>(ReleaseLabel(game.GetTitle(), release));
    CGameInfoTag tag = game;
    tag.SetTitle(release.title.empty() ? game.GetTitle() : release.title);
    tag.SetRegion(StringUtils::Join(release.regions, ","));
    tag.SetURL(release.files.front().path);
    *item->GetGameInfoTag() = tag;
    item->SetPath(release.files.front().path);
    item->SetFolder(false);
    item->SetLabelPreformatted(true);
    item->SetProperty("gameid", game.GetDatabaseId());
    item->SetProperty("releaseid", release.id);
    item->SetProperty("isdefaultrelease", release.isDefault);
    if (release.isDefault)
      item->Select(true);
    items.Add(item);
  }

  items.SetProperty("customtitle", game.GetTitle());
  items.SetContent("releases");
  return true;
}
