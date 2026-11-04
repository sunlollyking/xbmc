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
#include "games/GameManual.h"
#include "games/library/GameAgeRatings.h"
#include "games/library/GameDbUrl.h"
#include "games/library/GameHighScores.h"
#include "games/library/GameNameParser.h"
#include "games/tags/GameInfoTag.h"
#include "guilib/GUIListItem.h"
#include "playlists/SmartPlayList.h"
#include "resources/LocalizeStrings.h"
#include "resources/ResourcesComponent.h"
#include "settings/Settings.h"
#include "settings/SettingsComponent.h"
#include "utils/DatabaseUtils.h"
#include "utils/JSONVariantParser.h"
#include "utils/JSONVariantWriter.h"
#include "utils/SortUtils.h"
#include "utils/StringUtils.h"
#include "utils/TimeFormat.h"
#include "utils/URIUtils.h"
#include "utils/Variant.h"
#include "utils/log.h"

#include <algorithm>
#include <set>

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

/*!
 * \brief The name to show for a game, as the person asked to see it
 *
 * All three names are already on the row, so changing the setting shows a
 * different one at once rather than rebuilding the library. Sorting is left on
 * the catalogue title, which is what sortTitle was built from: an A-Z ordered
 * by a name nobody can see is confusing, but rewriting 30,000 sort titles
 * whenever this changes is worse.
 */
std::string DisplayTitle(const CGameInfoTag& game, const dbiplus::sql_record* record)
{
  const auto settings = CServiceBroker::GetSettingsComponent()->GetSettings();
  const auto style = static_cast<TitleStyle>(
      settings ? settings->GetInt(SETTING_GAMELIBRARY_TITLESTYLE) : 0);
  switch (style)
  {
    case TitleStyle::ORIGINAL:
      if (!game.GetOriginalTitle().empty())
        return game.GetOriginalTitle();
      break;
    case TitleStyle::ON_DISK:
      if (const std::string onDisk = record->at(GAMEDB_RELEASE_TITLE).get_asString();
          !onDisk.empty())
        return onDisk;
      break;
    case TitleStyle::CATALOGUE:
      break;
  }
  return game.GetTitle();
}

//! The regions a release was sold in, by the names a listing shows
std::string RegionNames(const GameRelease& release, const char* separator)
{
  std::vector<std::string> names;
  for (const GameRegion& region : release.regions)
    names.emplace_back(CGameNameParser::RegionName(region));
  return StringUtils::Join(names, separator);
}

std::string ReleaseLabel(const std::string& gameTitle, const GameRelease& release)
{
  const auto& strings = CServiceBroker::GetResourcesComponent().GetLocalizeStrings();
  std::vector<std::string> parts;

  // A catalogue name carries its tags, which the rest of the label says in words
  std::string own;
  if (!release.title.empty())
    own = CGameNameParser::Parse(release.title).displayTitle;
  if (CGameLibraryTypes::TitleKey(own) == CGameLibraryTypes::TitleKey(gameTitle))
    own.clear();

  // A hack, a translation or a bootleg that has a name of its own goes by it
  if ((!release.edition.empty() || release.licence == Licence::PIRATE) && !own.empty())
    parts.emplace_back(own);

  if (release.edition == "Mod")
    parts.emplace_back(strings.Get(35750)); // "Hack"
  else if (release.edition == "Fan Translation")
    parts.emplace_back(strings.Get(35751)); // "Fan translation"
  else if (!release.edition.empty())
    parts.emplace_back(release.edition);

  if (const int status = CGameLibraryTypes::ReleaseStatusLabel(release.status); status > 0)
    parts.emplace_back(strings.Get(status));
  if (const int licence = CGameLibraryTypes::LicenceLabel(release.licence); licence > 0)
    parts.emplace_back(strings.Get(licence));
  if (!release.revision.empty())
    parts.emplace_back(release.revision);
  if (!release.regions.empty())
    parts.emplace_back(RegionNames(release, ", "));
  if (!release.languages.empty())
    parts.emplace_back(StringUtils::ToUpper(StringUtils::Join(release.languages, ", ")));

  // An arcade set has nothing else to tell it apart, so it goes by its set name
  if (parts.empty())
    return own.empty() ? gameTitle : own;
  return StringUtils::Join(parts, " \u00b7 ");
}

/*!
 * \brief Give an item a game's pictures, with the stand-ins a game with no
 *        cover or backdrop is drawn with
 */
void SetGameArt(CFileItem& item, const KODI::ART::Artwork& art)
{
  item.SetArt(art);

  // Skins draw their backdrop from fanart, and games almost never have
  // any: of this library's 30,000, a couple of hundred do. Nearly all of
  // them have a picture of themselves running, though, which is the same
  // thing for this purpose. Declared as a fallback rather than written
  // in, so nothing here claims to be fanart that isn't.
  if (art.find("fanart") == art.end())
  {
    for (const char* type : {"screenshot", "titlescreen"})
    {
      if (art.find(type) != art.end())
      {
        item.SetArtFallback("fanart", type);
        break;
      }
    }
  }

  // The same stand-ins the thumb loader gives a game with no cover, so a
  // widget shows it as the library window does
  if (art.find("poster") == art.end() && art.find("boxfront") != art.end())
    item.SetArtFallback("poster", "boxfront");
  if (art.find("thumb") == art.end())
  {
    for (const char* type : {"boxfront", "clearlogo", "titlescreen", "screenshot"})
    {
      if (art.find(type) != art.end())
      {
        item.SetArtFallback("thumb", type);
        break;
      }
    }
  }
}

//! A time to beat in whole hours, or in minutes under an hour
std::string TimeToBeatLabel(int seconds)
{
  const auto& strings = CServiceBroker::GetResourcesComponent().GetLocalizeStrings();
  if (seconds < 60 * 60)
    return StringUtils::Format(strings.Get(35775), std::max(1, (seconds + 30) / 60)); // "{0:d} min"
  return StringUtils::Format(strings.Get(35774), (seconds + 30 * 60) / (60 * 60)); // "{0:d} h"
}

void SetTimeToBeatProperties(CFileItem& item, const GameTimeToBeat& time)
{
  const auto& strings = CServiceBroker::GetResourcesComponent().GetLocalizeStrings();
  for (const char* key : {"timetobeat", "timetobeatmain", "timetobeatextras", "timetobeatcomplete"})
    item.ClearProperty(key);

  if (time.main > 0)
    item.SetProperty("timetobeatmain", TimeToBeatLabel(time.main));
  if (time.extras > 0)
    item.SetProperty("timetobeatextras", TimeToBeatLabel(time.extras));
  if (time.complete > 0)
    item.SetProperty("timetobeatcomplete", TimeToBeatLabel(time.complete));

  // One line for a skin to show, from the shortest way through to 100%
  const int toBeat = time.main > 0 ? time.main : time.extras;
  std::string summary;
  if (toBeat > 0 && time.complete > 0)
    summary = StringUtils::Format(strings.Get(35776), TimeToBeatLabel(toBeat),
                                  TimeToBeatLabel(time.complete)); // "{0:s} · 100%: {1:s}"
  else if (toBeat > 0)
    summary = TimeToBeatLabel(toBeat);
  else if (time.complete > 0)
    summary =
        StringUtils::Format(strings.Get(35777), TimeToBeatLabel(time.complete)); // "100%: {0:s}"
  if (!summary.empty())
    item.SetProperty("timetobeat", summary);
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
  if (const CVariant* v = option("modeid"))
  {
    filter.AppendJoin("JOIN gamemode_link ON gamemode_link.idGame = game_view.idGame");
    filter.AppendWhere(
        PrepareSQL("gamemode_link.idGameMode = %i", static_cast<int>(v->asInteger())));
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
  if (const CVariant* v = option("similarto"))
  {
    const int idGame = static_cast<int>(v->asInteger());
    filter.AppendWhere(PrepareSQL("game_view.idGame <> %i AND game_view.titleKey IN (SELECT "
                                  "titleKey FROM game_link WHERE idGame = %i AND kind = 'similar')",
                                  idGame, idGame));
  }
  if (const CVariant* v = option("relatedto"))
  {
    // Both ways: what this game names, and the games that name this one
    const int idGame = static_cast<int>(v->asInteger());
    filter.AppendWhere(PrepareSQL(
        "game_view.idGame <> %i AND (game_view.titleKey IN (SELECT titleKey FROM game_link WHERE "
        "idGame = %i AND kind <> 'similar') OR game_view.idGame IN (SELECT idGame FROM game_link "
        "WHERE kind <> 'similar' AND titleKey = (SELECT titleKey FROM game WHERE idGame = %i)))",
        idGame, idGame, idGame));
  }
  if (const CVariant* v = option("seriesid"))
    filter.AppendWhere(PrepareSQL("game_view.idSeries = %i", static_cast<int>(v->asInteger())));
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
    filter.AppendWhere(GameAgeSQL() + PrepareSQL(" = %i", static_cast<int>(v->asInteger())));

  bool derivedShown = ShowDerivedGames();
  if (const CVariant* v = option("category"))
  {
    filter.AppendWhere(PrepareSQL("game_view.category = '%s'", v->asString().c_str()));
    derivedShown = true;
  }

  const std::string& list = url.GetList();
  // These nodes are the newest few whatever they are then sorted by, so the
  // limit belongs in the query and the sort is applied to what comes back
  if (list == "recentlyadded")
  {
    filter.AppendOrder("game_view.dateAdded DESC");
    filter.limit = PrepareSQL("%i", RECENT_LIMIT);
  }
  else if (list == "recentlyplayed")
  {
    filter.AppendWhere("game_view.lastPlayed IS NOT NULL AND game_view.lastPlayed <> ''");
    filter.AppendOrder("game_view.lastPlayed DESC");
    filter.limit = PrepareSQL("%i", RECENT_LIMIT);
  }
  else if (list == "continueplaying")
  {
    filter.AppendWhere("game_view.playCount > 0 AND game_view.completed = 0");
    filter.AppendOrder("game_view.lastPlayed DESC");
    filter.limit = PrepareSQL("%i", RECENT_LIMIT);
  }
  else if (list == "mostplayed")
  {
    filter.AppendWhere("game_view.playTime > 0");
    filter.AppendOrder("game_view.playTime DESC");
    filter.limit = PrepareSQL("%i", RECENT_LIMIT);
  }
  else if (list == "popular")
  {
    filter.AppendWhere("game_view.popularity > 0");
    filter.AppendOrder("game_view.popularity DESC");
    filter.limit = PrepareSQL("%i", RECENT_LIMIT);
  }
  else if (list == "mostwanted")
  {
    filter.AppendWhere("game_view.wantToPlay > 0");
    filter.AppendOrder("game_view.wantToPlay DESC");
    filter.limit = PrepareSQL("%i", RECENT_LIMIT);
  }
  else if (list == "neverplayed")
    filter.AppendWhere("game_view.playCount = 0");
  else if (list == "favourites")
    filter.AppendWhere("game_view.favourite = 1");
  else if (list == "beaten")
    filter.AppendWhere("game_view.beaten = 1");
  else if (list == "completed")
    filter.AppendWhere("game_view.completed = 1");
  else if (list == "multiplayer")
    filter.AppendWhere("game_view.playersMax >= 2");
  else if (list == "coop")
    filter.AppendWhere("game_view.coop = 1");
  else if (list == "englishimports")
  {
    // A game from the far east whose own dump says it's in English, as No-Intro
    // marks one: Japan (En,Ja)
    filter.AppendWhere(
        "EXISTS (SELECT 1 FROM gamerelease r JOIN release_region rr ON rr.idRelease = r.idRelease "
        "JOIN region ON region.idRegion = rr.idRegion WHERE r.idGame = game_view.idGame AND "
        "region.code IN ('JP', 'KR', 'CN', 'TW', 'HK', '030') AND (r.languages = 'en' OR "
        "r.languages LIKE 'en,%' OR r.languages LIKE '%,en' OR r.languages LIKE '%,en,%'))");
  }
  else if (list == "couchcoop")
    filter.AppendWhere("(game_view.coop = 1 OR game_view.dropIn = 1 OR game_view.splitScreen = 1)");
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
  else if (list == "needsattention")
  {
    // Nothing named it, or only its title did; the unnamed are worth seeing first
    filter.AppendWhere("game_view.matchedBy IN ('', 'name')");
    filter.AppendOrder("game_view.matchedBy, game_view.sortTitle");
    derivedShown = true;
  }

  // Showing hacks and homebrew adds those two; a BIOS set, an application or a
  // machine that is not a video game is not listed among games either way
  if (option("category") == nullptr)
    filter.AppendWhere(derivedShown
                           ? "game_view.category IN ('retail', 'demo', 'hack', 'homebrew')"
                           : "game_view.category IN ('retail', 'demo')");

  filter.AppendWhere("game_view.hidden = 0");

  // A smart playlist arrives as JSON on the path and adds its own rules
  for (const char* key : {"xsp", "filter"})
  {
    const CVariant* value = option(key);
    if (value == nullptr)
      continue;

    PLAYLIST::CSmartPlaylist xsp;
    if (!xsp.LoadFromJson(value->asString()))
      return false;
    if (!xsp.IsGameType())
      continue;

    std::set<std::string, std::less<>> playlists;
    filter.AppendWhere(xsp.GetWhereClause(*this, playlists));

    if (xsp.GetLimit() > 0)
      sorting.limitEnd = xsp.GetLimit();
    if (xsp.GetOrder() != SortBy::NONE)
      sorting.sortBy = xsp.GetOrder();
    if (xsp.GetOrderDirection() != SortOrder::NONE)
      sorting.sortOrder = xsp.GetOrderDirection();
    if (CServiceBroker::GetSettingsComponent()->GetSettings()->GetBool(
            CSettings::SETTING_FILELISTS_IGNORETHEWHENSORTING))
      sorting.sortAttributes = SortAttributeIgnoreArticle;
  }

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
    if (!SortUtils::SortFromDataset(sortDescription, KODI::MEDIA::TYPE::GAME, *m_pDS, results))
      return false;

    items.Reserve(results.size());
    const dbiplus::query_data& data = m_pDS->get_result_set().records;

    // Shared across the listing so that each folder is read once, not once per
    // game that lives in it
    CManualIndex manuals;
    const bool ranked = url.GetList() == "popular" || url.GetList() == "mostwanted";
    for (const auto& result : results)
    {
      const auto row = static_cast<unsigned int>(result.at(Field::ROW).asInteger());
      const dbiplus::sql_record* const record = data.at(row);

      CGameInfoTag game;
      GetDetailsForGame(record, game);

      const auto item = std::make_shared<CFileItem>(DisplayTitle(game, record));
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

      // Everything else the library holds about this game, so a skin can put
      // it on screen without asking the database again
      if (game.GetCategory() != GameCategory::RETAIL)
        item->SetProperty("gamecategory",
                          CServiceBroker::GetResourcesComponent().GetLocalizeStrings().Get(
                              CGameLibraryTypes::CategoryLabel(game.GetCategory())));

      if (!game.GetEdition().empty())
        item->SetProperty("edition", game.GetEdition());

      const std::string languages = record->at(GAMEDB_RELEASE_LANGUAGES).get_asString();
      if (!languages.empty())
        item->SetProperty("languages", StringUtils::ToUpper(languages));

      const std::string revision = record->at(GAMEDB_RELEASE_REVISION).get_asString();
      if (!revision.empty())
        item->SetProperty("revision", revision);

      const std::string serial = record->at(GAMEDB_RELEASE_SERIAL).get_asString();
      if (!serial.empty())
        item->SetProperty("serial", serial);

      if (game.IsCoop())
        item->SetProperty("coop", CServiceBroker::GetResourcesComponent().GetLocalizeStrings().Get(
                                      35599)); // "Co-op"

      const std::string matched(CGameLibraryTypes::ToString(game.GetMatchMethod()));
      if (!matched.empty())
        item->SetProperty("matchedby", matched);

      // A manual already on disk counts as much as one a catalogue knows
      // where to fetch, and is the only kind that works offline
      if (!game.GetManual().empty() || manuals.HasManual(game.GetURL()))
      {
        item->SetProperty("hasmanual", true);
        // The catalogues report a manual as somewhere to fetch it from, so the
        // address is carried through for whoever opens it
        if (!game.GetManual().empty())
          item->SetProperty("manual", game.GetManual());
      }
      if (game.IsFavourite())
        item->SetProperty("favourite", true);
      if (game.IsCompleted())
      {
        item->SetProperty("completed", true);
        // The same tick the video library puts on something watched
        item->SetOverlayImage(CGUIListItem::ICON_OVERLAY_WATCHED);
      }
      SetCompletionProperties(*item, game);
      SetSeriesProperties(*item, game);
      SetHighScoreProperties(*item, game);
      SetCoopProperties(*item, game);
      SetLanguageProperties(*item, game);
      if (game.GetPlayTime() > 0)
        item->SetProperty("playtime", StringUtils::SecondsToTimeString(game.GetPlayTime(),
                                                                       TIME_FORMAT_HH_MM));
      if (game.GetAchievementsEarned() > 0)
        item->SetProperty("achievementsearned", game.GetAchievementsEarned());
      // Kept in IGDB's order, which no field of the game sorts by, the way a
      // playlist keeps its own
      if (ranked)
        item->SetProgramCount(items.Size());
      item->SetLabelPreformatted(true);

      items.Add(item);
    }

    m_pDS->close();

    // The art comes with the listing, so a home widget or any other control
    // that never runs a thumb loader still shows the covers
    std::vector<int> ids;
    ids.reserve(static_cast<size_t>(items.Size()));
    for (const auto& item : items)
      ids.emplace_back(static_cast<int>(item->GetProperty("gameid").asInteger()));

    std::map<int, KODI::ART::Artwork> art;
    if (GetArtForItems(ids, KODI::MEDIA::TYPE::GAME, art))
    {
      for (const auto& item : items)
      {
        const auto found = art.find(static_cast<int>(item->GetProperty("gameid").asInteger()));
        if (found == art.end())
          continue;

        SetGameArt(*item, found->second);
      }
    }

    items.SetContent("games");
    return true;
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "GAME: Failed to list games for {}", baseDir);
  }
  return false;
}

void CGameDatabase::SetAgeProperties(CFileItem& item, int age)
{
  item.ClearProperty("age");
  item.ClearProperty("ageratingboard");
  item.ClearProperty("agerating");
  if (age <= 0)
    return;

  item.SetProperty("age", age);
  const std::string board = PreferredAgeRatingBoards().front();
  const std::string classification = CGameAgeRatings::ClassificationFor(board, age);
  if (!classification.empty())
  {
    item.SetProperty("ageratingboard", board);
    item.SetProperty("agerating", classification);
  }
}

void CGameDatabase::SetCompletionProperties(CFileItem& item, const CGameInfoTag& game)
{
  item.ClearProperty("beaten");
  item.ClearProperty("completion");
  if (game.IsBeaten())
  {
    item.SetProperty("beaten", true);
    item.SetProperty("completion",
                     CServiceBroker::GetResourcesComponent().GetLocalizeStrings().Get(
                         game.IsCompleted() ? 35531 : 35712)); // "Completed" / "Beaten"
  }
  SetTimeToBeatProperties(item, game.GetTimeToBeat());
}

void CGameDatabase::SetSeriesProperties(CFileItem& item, const CGameInfoTag& game)
{
  item.ClearProperty("series");
  item.ClearProperty("seriesid");
  if (game.GetSeriesId() > 0 && !game.GetSeries().empty())
  {
    item.SetProperty("series", game.GetSeries());
    item.SetProperty("seriesid", game.GetSeriesId());
  }
}

void CGameDatabase::SetModeProperties(CFileItem& item, const CGameInfoTag& game)
{
  item.ClearProperty("modes");
  if (!game.GetModes().empty())
    item.SetProperty("modes", StringUtils::Join(game.GetModes(), ", "));
}

void CGameDatabase::SetLanguageProperties(CFileItem& item, const CGameInfoTag& game)
{
  const auto& strings = CServiceBroker::GetResourcesComponent().GetLocalizeStrings();
  std::vector<std::string> names;
  for (const GameLanguage& language : game.GetLanguages())
  {
    const bool text = language.subtitles || language.interface;
    if (language.audio && !text)
      names.emplace_back(
          StringUtils::Format(strings.Get(35794), language.language)); // "{0:s} (spoken)"
    else if (text && !language.audio)
      names.emplace_back(
          StringUtils::Format(strings.Get(35795), language.language)); // "{0:s} (text)"
    else
      names.emplace_back(language.language);
  }

  item.ClearProperty("languagesupport");
  if (!names.empty())
    item.SetProperty("languagesupport", StringUtils::Join(names, ", "));
}

std::string CGameDatabase::SerializeLanguages(const std::vector<GameLanguage>& languages)
{
  if (languages.empty())
    return "";

  CVariant value(CVariant::VariantTypeArray);
  for (const GameLanguage& language : languages)
  {
    CVariant entry;
    entry["language"] = language.language;
    entry["audio"] = language.audio;
    entry["subtitles"] = language.subtitles;
    entry["interface"] = language.interface;
    value.push_back(entry);
  }
  std::string json;
  return CJSONVariantWriter::Write(value, json, true) ? json : "";
}

std::vector<GameLanguage> CGameDatabase::DeserializeLanguages(const std::string& json)
{
  std::vector<GameLanguage> languages;
  CVariant value;
  if (json.empty() || !CJSONVariantParser::Parse(json, value) || !value.isArray())
    return languages;

  for (auto it = value.begin_array(); it != value.end_array() && languages.size() < 100; ++it)
  {
    GameLanguage language{(*it)["language"].asString().substr(0, 64)};
    language.audio = (*it)["audio"].asBoolean();
    language.subtitles = (*it)["subtitles"].asBoolean();
    language.interface = (*it)["interface"].asBoolean();
    if (!language.language.empty())
      languages.emplace_back(std::move(language));
  }
  return languages;
}

void CGameDatabase::SetAgeContentProperties(CFileItem& item, const CGameInfoTag& game)
{
  item.ClearProperty("ageratingcontent");
  const std::vector<GameAgeRating>& ratings = game.GetAgeRatings();
  const auto rated = std::ranges::find_if(ratings, [](const GameAgeRating& rating)
                                          { return !rating.descriptors.empty(); });
  if (rated == ratings.end())
    return;

  item.SetProperty("ageratingcontent", rated == ratings.begin()
                                           ? rated->descriptors
                                           : rated->board + ": " + rated->descriptors);
}

void CGameDatabase::SetCoopProperties(CFileItem& item, const CGameInfoTag& game)
{
  const auto& strings = CServiceBroker::GetResourcesComponent().GetLocalizeStrings();
  std::vector<std::string> parts;
  if (game.GetCoopPlayers() > 1)
    parts.emplace_back(
        StringUtils::Format(strings.Get(35786), game.GetCoopPlayers())); // "{0:d} players"
  if (game.IsDropIn())
    parts.emplace_back(strings.Get(35787)); // "drop-in"
  if (game.IsSplitScreen())
    parts.emplace_back(strings.Get(35788)); // "split screen"

  item.ClearProperty("coopdetails");
  if (!parts.empty())
    item.SetProperty("coopdetails", StringUtils::Join(parts, " \u00b7 "));
}

void CGameDatabase::SetHighScoreProperties(CFileItem& item, const CGameInfoTag& game)
{
  item.ClearProperty("highscore");
  item.ClearProperty("highscores");
  if (const std::string best = CGameHighScores::Best(game.GetHighScores()); !best.empty())
  {
    item.SetProperty("highscore", best);
    item.SetProperty("highscores", CGameHighScores::Text(game.GetHighScores()));
  }
}

namespace
{
//! Where a series opens: within the platform the list was scoped to, if any
std::string SeriesPath(const CGameDbUrl& url, int idSeries)
{
  std::string path = "gamedb://";
  if (CVariant platformId; url.GetOption("platformid", platformId))
    path += "platforms/" + std::to_string(platformId.asInteger()) + "/";
  return path + "series/" + std::to_string(idSeries) + "/";
}

std::shared_ptr<CFileItem> SeriesItem(const std::string& name,
                                      const std::string& path,
                                      int idSeries,
                                      int count)
{
  auto item = std::make_shared<CFileItem>(name);
  item->SetPath(path);
  item->SetFolder(true);
  item->SetLabelPreformatted(true);
  item->SetLabel2(std::to_string(count));
  item->SetProperty("gamecount", count);
  item->SetProperty("seriesid", idSeries);
  return item;
}
} // namespace

bool CGameDatabase::GetSeriesNav(const std::string& baseDir, CFileItemList& items)
{
  try
  {
    CGameDbUrl url;
    if (!url.FromString(baseDir))
      return false;

    Filter scope;
    SortDescription sorting;
    if (!GetFilter(url, scope, sorting))
      return false;

    // Every game in scope that is in a series, earliest first, so each series
    // is counted and pictured by its first game
    const std::string sql =
        "SELECT game_view.idSeries, game_view.seriesName, game_view.idGame FROM game_view " +
        scope.join + " WHERE game_view.idSeries > 0" +
        (scope.where.empty() ? "" : " AND " + scope.where) +
        " ORDER BY game_view.year = 0, game_view.year, game_view.releaseDate, game_view.idGame";

    struct Series
    {
      std::string name;
      int firstGame{-1};
      int count{0};
    };
    std::vector<int> order;
    std::map<int, Series> series;
    if (m_pDS->query(sql))
    {
      while (!m_pDS->eof())
      {
        const int id = m_pDS->fv(0).get_asInt();
        const auto [it, added] = series.try_emplace(id);
        if (added)
        {
          it->second.name = m_pDS->fv(1).get_asString();
          it->second.firstGame = m_pDS->fv(2).get_asInt();
          order.emplace_back(id);
        }
        ++it->second.count;
        m_pDS->next();
      }
      m_pDS->close();
    }

    std::vector<int> firstGames;
    firstGames.reserve(order.size());
    for (const int id : order)
      firstGames.emplace_back(series[id].firstGame);
    std::map<int, KODI::ART::Artwork> art;
    GetArtForItems(firstGames, KODI::MEDIA::TYPE::GAME, art);

    for (const int id : order)
    {
      const Series& entry = series[id];
      const auto item = SeriesItem(entry.name, SeriesPath(url, id), id, entry.count);
      if (const auto found = art.find(entry.firstGame); found != art.end())
        SetGameArt(*item, found->second);
      items.Add(item);
    }

    items.SetContent("series");
    return true;
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "GAME: Failed to list {}", baseDir);
  }
  return false;
}

void CGameDatabase::FoldSeries(const CGameDbUrl& url, CFileItemList& items)
{
  std::map<int, std::vector<std::shared_ptr<CFileItem>>> bySeries;
  for (const auto& item : items)
  {
    if (item->HasGameInfoTag() && item->GetGameInfoTag()->GetSeriesId() > 0)
      bySeries[item->GetGameInfoTag()->GetSeriesId()].emplace_back(item);
  }

  for (const auto& [id, games] : bySeries)
  {
    if (games.size() < 2)
      continue;

    // Pictured by the first game of the series, as its box would stand on a shelf
    const auto first =
        std::ranges::min_element(games,
                                 [](const auto& a, const auto& b)
                                 {
                                   const unsigned int yearA = a->GetGameInfoTag()->GetYear();
                                   const unsigned int yearB = b->GetGameInfoTag()->GetYear();
                                   if ((yearA == 0) != (yearB == 0))
                                     return yearB == 0;
                                   return yearA < yearB;
                                 });

    const auto series = SeriesItem((*first)->GetGameInfoTag()->GetSeries(), SeriesPath(url, id), id,
                                   static_cast<int>(games.size()));
    for (const char* type : {"poster", "thumb", "fanart", "clearlogo", "banner", "boxfront"})
    {
      if (const std::string& picture = (*first)->GetArt(type); !picture.empty())
        series->SetArt(type, picture);
    }

    for (const auto& game : games)
      items.Remove(game.get());
    items.Add(series);
  }
}

int CGameDatabase::CountGames(const std::string& baseDir)
{
  try
  {
    CGameDbUrl url;
    Filter filter;
    SortDescription sorting;
    if (!url.FromString(baseDir) || !GetFilter(url, filter, sorting))
      return 0;

    std::string sql;
    if (!CDatabase::BuildSQL("SELECT COUNT(DISTINCT game_view.idGame) FROM game_view ", filter,
                             sql))
      return 0;
    return GetSingleValueInt(sql, *m_pDS);
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "GAME: Failed to count {}", baseDir);
  }
  return 0;
}

bool CGameDatabase::GetFacetNav(const std::string& baseDir, CFileItemList& items)
{
  try
  {
    CGameDbUrl url;
    if (!url.FromString(baseDir))
      return false;

    if (url.GetNode() == GameDbNode::SERIES)
      return GetSeriesNav(baseDir, items);

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
      case GameDbNode::MODES:
        sql = "SELECT gamemode.idGameMode AS id, gamemode.name AS label, COUNT(DISTINCT "
              "game_view.idGame) AS n FROM gamemode JOIN gamemode_link ON "
              "gamemode_link.idGameMode = gamemode.idGameMode JOIN game_view ON game_view.idGame = "
              "gamemode_link.idGame " +
              join + where + " GROUP BY gamemode.idGameMode, gamemode.name ORDER BY gamemode.name";
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
        sql = "SELECT region.code AS id, COALESCE(NULLIF(region.name, ''), region.code) AS label, "
              "COUNT(DISTINCT game_view.idGame) AS n FROM region JOIN release_region ON "
              "release_region.idRegion = region.idRegion JOIN gamerelease ON "
              "gamerelease.idRelease = release_region.idRelease JOIN game_view ON "
              "game_view.idGame = gamerelease.idGame " +
              join + where + " GROUP BY region.code, region.name ORDER BY label";
        break;
      case GameDbNode::AGERATINGS:
        sql = "SELECT age AS id, age AS label, COUNT(*) AS n FROM (SELECT DISTINCT "
              "game_view.idGame, " +
              GameAgeSQL() + " AS age FROM game_view " + join + where +
              ") AS ages GROUP BY age ORDER BY age = 0, age";
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

    // Skins pick the icon for a genre folder by its type, and a game facet has
    // no info tag to say it; a home widget can't read it off the listing's
    // content either
    const bool isGenre = url.GetNode() == GameDbNode::GENRES;

    const bool isAge = url.GetNode() == GameDbNode::AGERATINGS;

    if (m_pDS->query(sql))
    {
      while (!m_pDS->eof())
      {
        const std::string id = m_pDS->fv("id").get_asString();
        std::string label = m_pDS->fv("label").get_asString();
        if (isAge)
          label = id == "0" ? CServiceBroker::GetResourcesComponent().GetLocalizeStrings().Get(
                                  35765) // "Unrated"
                            : id + "+";
        const int count = m_pDS->fv("n").get_asInt();

        const auto item = std::make_shared<CFileItem>(label);
        item->SetPath(base + (numericValue ? id : CURL::Encode(id)) + "/");
        item->SetFolder(true);
        item->SetLabelPreformatted(true);
        item->SetLabel2(std::to_string(count));
        item->SetProperty("gamecount", count);
        if (isGenre)
          item->SetProperty("DBType", "genre");
        if (isAge)
          SetAgeProperties(*item, m_pDS->fv("id").get_asInt());
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
    tag.SetRegion(RegionNames(release, ","));
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
