/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "GameDatabase.h"
#include "GameDatabaseColumns.h"
#include "XBDateTime.h"
#include "dbwrappers/dataset.h"
#include "filesystem/Directory.h"
#include "filesystem/File.h"
#include "games/tags/GameInfoTag.h"
#include "utils/StringUtils.h"
#include "utils/URIUtils.h"
#include "utils/log.h"

#include <set>

using namespace KODI;
using namespace GAME;

namespace
{
constexpr const char* LIST_SEPARATOR = " / ";

std::string Now()
{
  return CDateTime::GetCurrentDateTime().GetAsDBDateTime();
}

std::vector<std::string> SplitList(const std::string& list)
{
  std::vector<std::string> values = StringUtils::Split(list, LIST_SEPARATOR);
  std::erase_if(values, [](const std::string& v) { return v.empty(); });
  return values;
}
} // namespace

int CGameDatabase::SetDetailsForGame(CGameInfoTag& details, const KODI::ART::Artwork& art)
{
  int idPlatform = details.GetPlatformId();
  if (idPlatform <= 0 && !details.GetPlatformSlug().empty())
  {
    PlatformInfo platform;
    if (GetPlatformBySlug(details.GetPlatformSlug(), platform))
      idPlatform = platform.id;
  }
  if (idPlatform <= 0 || details.GetTitle().empty())
  {
    CLog::Log(LOGERROR, "GAME: Cannot store '{}' without a platform and a title",
              details.GetTitle());
    return -1;
  }

  int idGame = details.GetDatabaseId();

  try
  {
    BeginTransaction();

    const std::string sortTitle =
        details.GetSortTitle().empty() ? details.GetTitle() : details.GetSortTitle();
    const std::string titleKey = CGameLibraryTypes::TitleKey(details.GetTitle());
    const std::string category(CGameLibraryTypes::ToString(details.GetCategory()));
    const std::string matchedBy(CGameLibraryTypes::ToString(details.GetMatchMethod()));
    const std::string genres = StringUtils::Join(details.GetGenres(), LIST_SEPARATOR);
    const std::string developers = StringUtils::Join(details.GetDevelopers(), LIST_SEPARATOR);
    const std::string publishers = StringUtils::Join(details.GetPublishers(), LIST_SEPARATOR);
    const std::string collections = StringUtils::Join(details.GetCollections(), LIST_SEPARATOR);

    if (idGame > 0)
    {
      m_pDS->exec(PrepareSQL(
          "UPDATE game SET idPlatform = %i, title = '%s', sortTitle = '%s', titleKey = '%s', "
          "originalTitle = '%s', overview = '%s', releaseDate = '%s', year = %i, playersMin = "
          "%i, playersMax = %i, coop = %i, category = '%s', idParentGame = %i, userRating = %i, "
          "favourite = %i, completed = %i, achievementsTotal = %i, achievementsEarned = %i, "
          "achievementsHardcore = %i, lastUnlock = '%s', matchedBy = '%s', trailer = '%s', "
          "manual = '%s', lastScraped = '%s', genres = '%s', developers = '%s', publishers = "
          "'%s', collections = '%s' WHERE idGame = %i",
          idPlatform, details.GetTitle().c_str(), sortTitle.c_str(), titleKey.c_str(),
          details.GetOriginalTitle().c_str(), details.GetOverview().c_str(),
          details.GetReleaseDate().c_str(), static_cast<int>(details.GetYear()),
          details.GetPlayersMin(), details.GetPlayersMax(), details.IsCoop() ? 1 : 0,
          category.c_str(), details.GetParentGameId(), details.GetUserRating(),
          details.IsFavourite() ? 1 : 0, details.IsCompleted() ? 1 : 0,
          details.GetAchievementsTotal(), details.GetAchievementsEarned(),
          details.GetAchievementsHardcore(), details.GetLastUnlock().c_str(), matchedBy.c_str(),
          details.GetTrailer().c_str(), details.GetManual().c_str(), Now().c_str(),
          genres.c_str(), developers.c_str(), publishers.c_str(), collections.c_str(), idGame));
    }
    else
    {
      const std::string dateAdded = details.GetDateAdded().empty() ? Now() : details.GetDateAdded();
      m_pDS->exec(PrepareSQL(
          "INSERT INTO game (idPlatform, title, sortTitle, titleKey, originalTitle, overview, "
          "releaseDate, year, playersMin, playersMax, coop, category, idParentGame, "
          "idDefaultRelease, idDefaultRating, idDefaultUniqueId, userRating, favourite, "
          "completed, hidden, achievementsTotal, achievementsEarned, achievementsHardcore, "
          "lastUnlock, matchedBy, trailer, manual, lastScraped, dateAdded, genres, developers, "
          "publishers, collections) VALUES (%i, '%s', '%s', '%s', '%s', '%s', '%s', %i, %i, %i, "
          "%i, '%s', %i, -1, -1, -1, %i, %i, %i, 0, %i, %i, %i, '%s', '%s', '%s', '%s', '%s', "
          "'%s', '%s', '%s', '%s', '%s')",
          idPlatform, details.GetTitle().c_str(), sortTitle.c_str(), titleKey.c_str(),
          details.GetOriginalTitle().c_str(), details.GetOverview().c_str(),
          details.GetReleaseDate().c_str(), static_cast<int>(details.GetYear()),
          details.GetPlayersMin(), details.GetPlayersMax(), details.IsCoop() ? 1 : 0,
          category.c_str(), details.GetParentGameId(), details.GetUserRating(),
          details.IsFavourite() ? 1 : 0, details.IsCompleted() ? 1 : 0,
          details.GetAchievementsTotal(), details.GetAchievementsEarned(),
          details.GetAchievementsHardcore(), details.GetLastUnlock().c_str(), matchedBy.c_str(),
          details.GetTrailer().c_str(), details.GetManual().c_str(), Now().c_str(),
          dateAdded.c_str(), genres.c_str(), developers.c_str(), publishers.c_str(),
          collections.c_str()));
      idGame = static_cast<int>(m_pDS->lastinsertid());
    }

    SetLinks("genre_link", "idGenre", "genre", "idGenre", idGame, details.GetGenres());
    SetLinks("tag_link", "idTag", "tag", "idTag", idGame, details.GetTags());
    SetLinks("developer_link", "idCompany", "company", "idCompany", idGame,
             details.GetDevelopers());
    SetLinks("publisher_link", "idCompany", "company", "idCompany", idGame,
             details.GetPublishers());
    SetLinks("collection_link", "idCollection", "collection", "idCollection", idGame,
             details.GetCollections());

    SetRatingsForGame(idGame, details);
    SetUniqueIdsForGame(idGame, details);
    SetAgeRatingsForGame(idGame, details);

    std::vector<GameRelease> releases = details.GetReleases();
    int idDefaultRelease = -1;
    for (GameRelease& release : releases)
    {
      const int idRelease = SetReleaseForGame(idGame, release);
      if (idRelease <= 0)
        continue;

      if (idDefaultRelease < 0 || release.id == details.GetDefaultReleaseId() ||
          (details.GetDefaultReleaseId() < 0 && release.isDefault))
        idDefaultRelease = idRelease;
    }
    if (idDefaultRelease > 0)
    {
      m_pDS->exec(PrepareSQL("UPDATE game SET idDefaultRelease = %i WHERE idGame = %i",
                             idDefaultRelease, idGame));
    }
    for (GameRelease& release : releases)
      release.isDefault = (release.id == idDefaultRelease);
    details.SetReleases(releases);
    details.SetDefaultReleaseId(idDefaultRelease);

    SetArtForItem(idGame, MediaTypeGame, art);

    CommitTransaction();
    details.SetDatabaseId(idGame);
    details.SetPlatformId(idPlatform);
    return idGame;
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "GAME: Failed to store '{}'", details.GetTitle());
    RollbackTransaction();
  }
  return -1;
}

int CGameDatabase::SetReleaseForGame(int idGame, GameRelease& release)
{
  const std::string regions = StringUtils::Join(release.regions, ",");
  const std::string languages = StringUtils::Join(release.languages, ",");
  const std::string status(CGameLibraryTypes::ToString(release.status));
  const std::string licence(CGameLibraryTypes::ToString(release.licence));
  const std::string dump(CGameLibraryTypes::ToString(release.dump));

  if (release.id > 0)
  {
    m_pDS->exec(PrepareSQL(
        "UPDATE gamerelease SET idGame = %i, title = '%s', regions = '%s', languages = '%s', "
        "revision = '%s', status = '%s', licence = '%s', isAlternate = %i, dumpStatus = '%s', "
        "releaseDate = '%s', serial = '%s', notes = '%s' WHERE idRelease = %i",
        idGame, release.title.c_str(), regions.c_str(), languages.c_str(),
        release.revision.c_str(), status.c_str(), licence.c_str(), release.alternate ? 1 : 0,
        dump.c_str(), release.releaseDate.c_str(), release.serial.c_str(), release.notes.c_str(),
        release.id));
  }
  else
  {
    m_pDS->exec(PrepareSQL(
        "INSERT INTO gamerelease (idGame, title, regions, languages, revision, status, licence, "
        "isAlternate, dumpStatus, releaseDate, serial, notes) VALUES (%i, '%s', '%s', '%s', '%s', "
        "'%s', '%s', %i, '%s', '%s', '%s', '%s')",
        idGame, release.title.c_str(), regions.c_str(), languages.c_str(),
        release.revision.c_str(), status.c_str(), licence.c_str(), release.alternate ? 1 : 0,
        dump.c_str(), release.releaseDate.c_str(), release.serial.c_str(), release.notes.c_str()));
    release.id = static_cast<int>(m_pDS->lastinsertid());
  }

  m_pDS->exec(PrepareSQL("DELETE FROM release_region WHERE idRelease = %i", release.id));
  for (const std::string& code : release.regions)
  {
    const int idRegion = AddLookup("region", "idRegion", code, "code");
    if (idRegion > 0)
    {
      m_pDS->exec(PrepareSQL("INSERT INTO release_region (idRegion, idRelease) VALUES (%i, %i)",
                             idRegion, release.id));
    }
  }

  for (GameFile& file : release.files)
    file.id = AddFile(file.path, release.id, file);

  return release.id;
}

void CGameDatabase::SetRatingsForGame(int idGame, CGameInfoTag& details)
{
  m_pDS->exec(PrepareSQL("DELETE FROM rating WHERE media_id = %i AND media_type = '%s'", idGame,
                         MediaTypeGame));

  int idDefault = -1;
  for (const auto& [type, rating] : details.GetRatings())
  {
    m_pDS->exec(PrepareSQL("INSERT INTO rating (media_id, media_type, rating_type, rating, max, "
                           "votes) VALUES (%i, '%s', '%s', %f, %f, %i)",
                           idGame, MediaTypeGame, type.c_str(),
                           static_cast<double>(rating.rating), static_cast<double>(rating.max),
                           rating.votes));
    if (idDefault < 0 || type == details.GetDefaultRatingType())
      idDefault = static_cast<int>(m_pDS->lastinsertid());
  }
  m_pDS->exec(
      PrepareSQL("UPDATE game SET idDefaultRating = %i WHERE idGame = %i", idDefault, idGame));
}

void CGameDatabase::SetUniqueIdsForGame(int idGame, CGameInfoTag& details)
{
  m_pDS->exec(PrepareSQL("DELETE FROM uniqueid WHERE media_id = %i AND media_type = '%s'", idGame,
                         MediaTypeGame));

  int idDefault = -1;
  for (const auto& [type, value] : details.GetUniqueIDs())
  {
    m_pDS->exec(PrepareSQL("INSERT INTO uniqueid (media_id, media_type, type, value) VALUES (%i, "
                           "'%s', '%s', '%s')",
                           idGame, MediaTypeGame, type.c_str(), value.c_str()));
    if (idDefault < 0 || type == details.GetDefaultUniqueIDType())
      idDefault = static_cast<int>(m_pDS->lastinsertid());
  }
  m_pDS->exec(
      PrepareSQL("UPDATE game SET idDefaultUniqueId = %i WHERE idGame = %i", idDefault, idGame));
}

void CGameDatabase::SetAgeRatingsForGame(int idGame, const CGameInfoTag& details)
{
  m_pDS->exec(PrepareSQL("DELETE FROM agerating WHERE media_id = %i AND media_type = '%s'",
                         idGame, MediaTypeGame));

  for (const GameAgeRating& rating : details.GetAgeRatings())
  {
    m_pDS->exec(PrepareSQL("INSERT INTO agerating (media_id, media_type, board, value, "
                           "descriptors) VALUES (%i, '%s', '%s', '%s', '%s')",
                           idGame, MediaTypeGame, rating.board.c_str(), rating.value.c_str(),
                           rating.descriptors.c_str()));
  }
}

void CGameDatabase::LoadRatings(int idGame, CGameInfoTag& details)
{
  std::map<std::string, GameRating> ratings;
  std::string defaultType = details.GetDefaultRatingType();

  if (m_pDS->query(PrepareSQL("SELECT rating_type, rating, max, votes FROM rating WHERE media_id "
                              "= %i AND media_type = '%s'",
                              idGame, MediaTypeGame)))
  {
    while (!m_pDS->eof())
    {
      GameRating rating;
      rating.rating = m_pDS->fv(1).get_asFloat();
      rating.max = m_pDS->fv(2).get_asFloat();
      rating.votes = m_pDS->fv(3).get_asInt();
      ratings[m_pDS->fv(0).get_asString()] = rating;
      m_pDS->next();
    }
    m_pDS->close();
  }

  if (!ratings.empty())
    details.SetRatings(ratings, defaultType);
}

void CGameDatabase::LoadUniqueIds(int idGame, CGameInfoTag& details)
{
  std::map<std::string, std::string> ids;
  const std::string defaultType = details.GetDefaultUniqueIDType();

  if (m_pDS->query(PrepareSQL(
          "SELECT type, value FROM uniqueid WHERE media_id = %i AND media_type = '%s'", idGame,
          MediaTypeGame)))
  {
    while (!m_pDS->eof())
    {
      ids[m_pDS->fv(0).get_asString()] = m_pDS->fv(1).get_asString();
      m_pDS->next();
    }
    m_pDS->close();
  }

  if (!ids.empty())
    details.SetUniqueIDs(ids, defaultType);
}

void CGameDatabase::LoadAgeRatings(int idGame, CGameInfoTag& details)
{
  std::vector<GameAgeRating> ratings;

  if (m_pDS->query(PrepareSQL("SELECT board, value, descriptors FROM agerating WHERE media_id = "
                              "%i AND media_type = '%s'",
                              idGame, MediaTypeGame)))
  {
    while (!m_pDS->eof())
    {
      GameAgeRating rating;
      rating.board = m_pDS->fv(0).get_asString();
      rating.value = m_pDS->fv(1).get_asString();
      rating.descriptors = m_pDS->fv(2).get_asString();
      ratings.emplace_back(std::move(rating));
      m_pDS->next();
    }
    m_pDS->close();
  }

  details.SetAgeRatings(ratings);
}

void CGameDatabase::GetReleasesForGame(int idGame, std::vector<GameRelease>& releases)
{
  if (m_pDS->query(PrepareSQL("SELECT * FROM release_view WHERE idGame = %i ORDER BY isDefault "
                              "DESC, idRelease",
                              idGame)))
  {
    while (!m_pDS->eof())
    {
      GameRelease release;
      release.id = m_pDS->fv("idRelease").get_asInt();
      release.title = m_pDS->fv("title").get_asString();
      release.regions = StringUtils::Split(m_pDS->fv("regions").get_asString(), ',');
      release.languages = StringUtils::Split(m_pDS->fv("languages").get_asString(), ',');
      std::erase_if(release.regions, [](const std::string& s) { return s.empty(); });
      std::erase_if(release.languages, [](const std::string& s) { return s.empty(); });
      release.revision = m_pDS->fv("revision").get_asString();
      release.status =
          CGameLibraryTypes::ReleaseStatusFromString(m_pDS->fv("status").get_asString());
      release.licence = CGameLibraryTypes::LicenceFromString(m_pDS->fv("licence").get_asString());
      release.alternate = m_pDS->fv("isAlternate").get_asBool();
      release.dump = CGameLibraryTypes::DumpStatusFromString(m_pDS->fv("dumpStatus").get_asString());
      release.releaseDate = m_pDS->fv("releaseDate").get_asString();
      release.serial = m_pDS->fv("serial").get_asString();
      release.notes = m_pDS->fv("notes").get_asString();
      release.isDefault = m_pDS->fv("isDefault").get_asBool();
      releases.emplace_back(std::move(release));
      m_pDS->next();
    }
    m_pDS->close();
  }

  for (GameRelease& release : releases)
    GetFilesForRelease(release.id, release.files);
}

void CGameDatabase::GetDetailsForGame(const dbiplus::sql_record* record, CGameInfoTag& details)
{
  details.Reset();

  details.SetDatabaseId(record->at(GAMEDB_ID).get_asInt());
  details.SetPlatformId(record->at(GAMEDB_PLATFORM_ID).get_asInt());
  details.SetTitle(record->at(GAMEDB_TITLE).get_asString());
  details.SetSortTitle(record->at(GAMEDB_SORT_TITLE).get_asString());
  details.SetOriginalTitle(record->at(GAMEDB_ORIGINAL_TITLE).get_asString());
  details.SetOverview(record->at(GAMEDB_OVERVIEW).get_asString());
  details.SetReleaseDate(record->at(GAMEDB_RELEASE_DATE).get_asString());
  details.SetYear(static_cast<unsigned int>(std::max(record->at(GAMEDB_YEAR).get_asInt(), 0)));
  details.SetPlayers(record->at(GAMEDB_PLAYERS_MIN).get_asInt(),
                     record->at(GAMEDB_PLAYERS_MAX).get_asInt());
  details.SetCoop(record->at(GAMEDB_COOP).get_asBool());
  details.SetCategory(
      CGameLibraryTypes::GameCategoryFromString(record->at(GAMEDB_CATEGORY).get_asString()));
  details.SetParentGameId(record->at(GAMEDB_PARENT_GAME_ID).get_asInt());
  details.SetDefaultReleaseId(record->at(GAMEDB_DEFAULT_RELEASE_ID).get_asInt());
  details.SetUserRating(record->at(GAMEDB_USER_RATING).get_asInt());
  details.SetFavourite(record->at(GAMEDB_FAVOURITE).get_asBool());
  details.SetCompleted(record->at(GAMEDB_COMPLETED).get_asBool());
  details.SetAchievements(record->at(GAMEDB_ACHIEVEMENTS_TOTAL).get_asInt(),
                          record->at(GAMEDB_ACHIEVEMENTS_EARNED).get_asInt(),
                          record->at(GAMEDB_ACHIEVEMENTS_HARDCORE).get_asInt());
  details.SetLastUnlock(record->at(GAMEDB_LAST_UNLOCK).get_asString());
  details.SetMatchMethod(
      CGameLibraryTypes::MatchMethodFromString(record->at(GAMEDB_MATCHED_BY).get_asString()));
  details.SetTrailer(record->at(GAMEDB_TRAILER).get_asString());
  details.SetManual(record->at(GAMEDB_MANUAL).get_asString());
  details.SetDateAdded(record->at(GAMEDB_DATE_ADDED).get_asString());
  details.SetGenres(SplitList(record->at(GAMEDB_GENRES).get_asString()));
  details.SetDevelopers(SplitList(record->at(GAMEDB_DEVELOPERS).get_asString()));
  details.SetPublishers(SplitList(record->at(GAMEDB_PUBLISHERS).get_asString()));
  details.SetCollections(SplitList(record->at(GAMEDB_COLLECTIONS).get_asString()));

  details.SetPlatformSlug(record->at(GAMEDB_PLATFORM_SLUG).get_asString());
  details.SetPlatform(record->at(GAMEDB_PLATFORM_NAME).get_asString());
  details.SetRegion(record->at(GAMEDB_RELEASE_REGIONS).get_asString());

  const std::string ratingType = record->at(GAMEDB_RATING_TYPE).get_asString();
  if (!ratingType.empty())
  {
    GameRating rating;
    rating.rating = record->at(GAMEDB_RATING).get_asFloat();
    rating.max = record->at(GAMEDB_RATING_MAX).get_asFloat();
    rating.votes = record->at(GAMEDB_VOTES).get_asInt();
    details.SetRating(ratingType, rating);
  }

  const std::string uniqueIdType = record->at(GAMEDB_UNIQUEID_TYPE).get_asString();
  if (!uniqueIdType.empty())
    details.SetUniqueID(uniqueIdType, record->at(GAMEDB_UNIQUEID_VALUE).get_asString(), true);

  details.SetPlayCount(record->at(GAMEDB_PLAY_COUNT).get_asInt());
  details.SetLastPlayed(record->at(GAMEDB_LAST_PLAYED).get_asString());

  const std::string path = record->at(GAMEDB_PATH).get_asString();
  const std::string fileName = record->at(GAMEDB_FILENAME).get_asString();
  if (!fileName.empty())
    details.SetURL(path + fileName);

  details.SetLoaded(true);
}

bool CGameDatabase::GetGameInfo(int idGame, CGameInfoTag& details)
{
  try
  {
    if (!m_pDS->query(PrepareSQL("SELECT * FROM game_view WHERE idGame = %i", idGame)) ||
        m_pDS->eof())
    {
      m_pDS->close();
      return false;
    }

    GetDetailsForGame(m_pDS->get_sql_record(), details);
    m_pDS->close();

    LoadRatings(idGame, details);
    LoadUniqueIds(idGame, details);
    LoadAgeRatings(idGame, details);
    details.SetTags(GetLinkNames("tag_link", "idTag", "tag", "idTag", idGame));

    std::vector<GameRelease> releases;
    GetReleasesForGame(idGame, releases);
    details.SetReleases(releases);

    return true;
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "GAME: Failed to read game {}", idGame);
  }
  return false;
}

bool CGameDatabase::DeleteGame(int idGame)
{
  if (idGame <= 0)
    return false;
  return ExecuteQuery(PrepareSQL("DELETE FROM game WHERE idGame = %i", idGame));
}

int CGameDatabase::FindGameByUniqueId(int idPlatform,
                                      const std::string& type,
                                      const std::string& value)
{
  if (type.empty() || value.empty())
    return -1;

  try
  {
    return GetSingleValueInt(PrepareSQL(
        "SELECT game.idGame FROM uniqueid JOIN game ON game.idGame = uniqueid.media_id WHERE "
        "uniqueid.media_type = '%s' AND uniqueid.type = '%s' AND uniqueid.value = '%s' AND "
        "game.idPlatform = %i",
        MediaTypeGame, type.c_str(), value.c_str(), idPlatform));
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "GAME: Failed to look up {} id {}", type, value);
  }
  return -1;
}

int CGameDatabase::FindGameByTitleKey(int idPlatform, const std::string& titleKey)
{
  if (titleKey.empty())
    return -1;

  try
  {
    return GetSingleValueInt(
        PrepareSQL("SELECT idGame FROM game WHERE idPlatform = %i AND titleKey = '%s'", idPlatform,
                   titleKey.c_str()));
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "GAME: Failed to look up title key {}", titleKey);
  }
  return -1;
}

bool CGameDatabase::SetDefaultRelease(int idGame, int idRelease)
{
  const int owner = GetSingleValueInt(
      PrepareSQL("SELECT idGame FROM gamerelease WHERE idRelease = %i", idRelease));
  if (owner != idGame)
    return false;

  return ExecuteQuery(PrepareSQL("UPDATE game SET idDefaultRelease = %i WHERE idGame = %i",
                                 idRelease, idGame));
}

bool CGameDatabase::SetFavourite(int idGame, bool favourite)
{
  return ExecuteQuery(PrepareSQL("UPDATE game SET favourite = %i WHERE idGame = %i",
                                 favourite ? 1 : 0, idGame));
}

bool CGameDatabase::SetCompleted(int idGame, bool completed)
{
  return ExecuteQuery(PrepareSQL("UPDATE game SET completed = %i WHERE idGame = %i",
                                 completed ? 1 : 0, idGame));
}

bool CGameDatabase::SetUserRating(int idGame, int rating)
{
  return ExecuteQuery(PrepareSQL("UPDATE game SET userRating = %i WHERE idGame = %i",
                                 std::clamp(rating, 0, 10), idGame));
}

bool CGameDatabase::HasContent()
{
  try
  {
    return GetSingleValueInt("SELECT COUNT(1) FROM game") > 0;
  }
  catch (...)
  {
  }
  return false;
}

int CGameDatabase::CleanDatabase(const std::set<int>& paths)
{
  try
  {
    std::string sql = "SELECT files.idFile, path.strPath, files.strFilename FROM files JOIN path "
                      "ON path.idPath = files.idPath";
    if (!paths.empty())
    {
      std::vector<std::string> ids;
      for (int id : paths)
        ids.emplace_back(std::to_string(id));
      sql += " WHERE path.idPath IN (" + StringUtils::Join(ids, ",") + ")";
    }
    sql += " ORDER BY path.strPath";

    // A folder whose whole source is unreachable is not cleaned, so an
    // unplugged drive does not empty the library
    std::map<std::string, bool> reachableRoots;
    auto RootReachable = [this, &reachableRoots](const std::string& folder)
    {
      GamePathContent content;
      bool foundDirectly = false;
      std::string root = folder;
      if (GetPathContent(folder, content, foundDirectly))
      {
        // Walk to the folder that carries the content
        std::string probe = folder;
        while (!probe.empty())
        {
          GamePathContent c;
          bool direct = false;
          if (GetPathContent(probe, c, direct) && direct)
          {
            root = probe;
            break;
          }
          const std::string parent = URIUtils::GetParentPath(probe);
          if (parent.empty() || parent == probe)
            break;
          probe = parent;
        }
      }
      auto it = reachableRoots.find(root);
      if (it == reachableRoots.end())
        it = reachableRoots.emplace(root, XFILE::CDirectory::Exists(root, false)).first;
      return it->second;
    };

    std::vector<int> missing;
    std::string lastFolder;
    bool lastFolderReachable = true;
    if (m_pDS->query(sql))
    {
      while (!m_pDS->eof())
      {
        const int idFile = m_pDS->fv(0).get_asInt();
        const std::string folder = m_pDS->fv(1).get_asString();
        const std::string fileName = m_pDS->fv(2).get_asString();

        if (folder != lastFolder)
        {
          lastFolder = folder;
          lastFolderReachable = RootReachable(folder);
        }

        if (lastFolderReachable && !XFILE::CFile::Exists(folder + fileName, false))
          missing.push_back(idFile);

        m_pDS->next();
      }
      m_pDS->close();
    }

    BeginTransaction();

    for (int idFile : missing)
      m_pDS->exec(PrepareSQL("DELETE FROM files WHERE idFile = %i", idFile));

    m_pDS->exec("DELETE FROM gamerelease WHERE NOT EXISTS (SELECT 1 FROM files WHERE "
                "files.idRelease = gamerelease.idRelease)");

    const int gamesBefore = GetSingleValueInt("SELECT COUNT(1) FROM game", *m_pDS);
    m_pDS->exec("DELETE FROM game WHERE NOT EXISTS (SELECT 1 FROM gamerelease WHERE "
                "gamerelease.idGame = game.idGame)");
    const int gamesAfter = GetSingleValueInt("SELECT COUNT(1) FROM game", *m_pDS);

    m_pDS->exec("UPDATE game SET idDefaultRelease = (SELECT MIN(idRelease) FROM gamerelease WHERE "
                "gamerelease.idGame = game.idGame) WHERE NOT EXISTS (SELECT 1 FROM gamerelease "
                "WHERE gamerelease.idRelease = game.idDefaultRelease)");

    m_pDS->exec("DELETE FROM genre WHERE NOT EXISTS (SELECT 1 FROM genre_link WHERE "
                "genre_link.idGenre = genre.idGenre)");
    m_pDS->exec("DELETE FROM tag WHERE NOT EXISTS (SELECT 1 FROM tag_link WHERE tag_link.idTag = "
                "tag.idTag)");
    m_pDS->exec("DELETE FROM company WHERE NOT EXISTS (SELECT 1 FROM developer_link WHERE "
                "developer_link.idCompany = company.idCompany) AND NOT EXISTS (SELECT 1 FROM "
                "publisher_link WHERE publisher_link.idCompany = company.idCompany)");
    m_pDS->exec("DELETE FROM collection WHERE NOT EXISTS (SELECT 1 FROM collection_link WHERE "
                "collection_link.idCollection = collection.idCollection)");
    m_pDS->exec("DELETE FROM path WHERE idPlatform IS NULL AND NOT EXISTS (SELECT 1 FROM files "
                "WHERE files.idPath = path.idPath) AND NOT EXISTS (SELECT 1 FROM path p WHERE "
                "p.idParentPath = path.idPath)");

    CommitTransaction();

    CLog::Log(LOGINFO, "GAME: Cleaned {} missing files and {} games", missing.size(),
              gamesBefore - gamesAfter);
    return gamesBefore - gamesAfter;
  }
  catch (...)
  {
    CLog::Log(LOGERROR, "GAME: Failed to clean the library");
    RollbackTransaction();
  }
  return -1;
}
