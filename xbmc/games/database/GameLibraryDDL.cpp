/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "GameLibraryDDL.h"

#include "dbwrappers/Database.h"
#include "media/MediaType.h"
#include "utils/log.h"

#include <string>

using namespace KODI;
using namespace GAME;

void CGameLibraryDDL::CreateTables(CDatabase& db)
{
  CLog::Log(LOGINFO, "GAME: Creating platform table");
  db.ExecuteQuery("CREATE TABLE platform ("
                  "idPlatform integer primary key, "
                  "slug text, "
                  "name text, "
                  "sortName text, "
                  "manufacturer text, "
                  "type text, "
                  "media text, "
                  "family text, "
                  "released integer, "
                  "discontinued integer, "
                  "overview text, "
                  "extensions text, "
                  "defaultGameClient text, "
                  "defaultVideoFilter text, "
                  "dateAdded text, "
                  "lastScraped text)");

  CLog::Log(LOGINFO, "GAME: Creating path table");
  db.ExecuteQuery("CREATE TABLE path ("
                  "idPath integer primary key, "
                  "strPath text, "
                  "idParentPath integer, "
                  "idPlatform integer, "
                  "strContent text, "
                  "strScraper text, "
                  "strSettings text, "
                  "strHash text, "
                  "scanRecursive integer, "
                  "useFolderNames bool, "
                  "noUpdate bool, "
                  "exclude bool, "
                  "dateAdded text)");

  CLog::Log(LOGINFO, "GAME: Creating files table");
  db.ExecuteQuery("CREATE TABLE files ("
                  "idFile integer primary key, "
                  "idPath integer, "
                  "idRelease integer, "
                  "strFilename text, "
                  "size integer, "
                  "crc32 text, "
                  "md5 text, "
                  "sha1 text, "
                  "serial text, "
                  "raHash text, "
                  "discNumber integer, "
                  "playCount integer, "
                  "lastPlayed text, "
                  "playTime integer, "
                  "dateAdded text)");

  CLog::Log(LOGINFO, "GAME: Creating game table");
  db.ExecuteQuery("CREATE TABLE game ("
                  "idGame integer primary key, "
                  "idPlatform integer, "
                  "title text, "
                  "sortTitle text, "
                  "titleKey text, "
                  "originalTitle text, "
                  "overview text, "
                  "releaseDate text, "
                  "year integer, "
                  "playersMin integer, "
                  "playersMax integer, "
                  "coop bool, "
                  "category text, "
                  "idParentGame integer, "
                  "idDefaultRelease integer, "
                  "idDefaultRating integer, "
                  "idDefaultUniqueId integer, "
                  "userRating integer, "
                  "favourite bool, "
                  "completed bool, "
                  "hidden bool, "
                  "achievementsTotal integer, "
                  "achievementsEarned integer, "
                  "achievementsHardcore integer, "
                  "lastUnlock text, "
                  "matchedBy text, "
                  "trailer text, "
                  "manual text, "
                  "lastScraped text, "
                  "dateAdded text, "
                  "genres text, "
                  "developers text, "
                  "publishers text, "
                  "collections text)");

  CLog::Log(LOGINFO, "GAME: Creating release table");
  db.ExecuteQuery("CREATE TABLE gamerelease ("
                  "idRelease integer primary key, "
                  "idGame integer, "
                  "title text, "
                  "regions text, "
                  "languages text, "
                  "revision text, "
                  "status text, "
                  "licence text, "
                  "isAlternate bool, "
                  "dumpStatus text, "
                  "releaseDate text, "
                  "serial text, "
                  "notes text)");

  CLog::Log(LOGINFO, "GAME: Creating lookup tables");
  CreateLookupTable(db, "genre", "idGenre");
  CreateLookupTable(db, "tag", "idTag");
  db.ExecuteQuery("CREATE TABLE company (idCompany integer primary key, name text, country text)");
  db.ExecuteQuery("CREATE TABLE developer_link (idCompany integer, idGame integer)");
  db.ExecuteQuery("CREATE TABLE publisher_link (idCompany integer, idGame integer)");
  db.ExecuteQuery("CREATE TABLE collection ("
                  "idCollection integer primary key, "
                  "name text, "
                  "sortName text, "
                  "overview text, "
                  "type text)");
  db.ExecuteQuery("CREATE TABLE collection_link (idCollection integer, idGame integer)");
  db.ExecuteQuery("CREATE TABLE region (idRegion integer primary key, code text, name text)");
  db.ExecuteQuery("CREATE TABLE release_region (idRegion integer, idRelease integer)");

  CLog::Log(LOGINFO, "GAME: Creating art, rating and uniqueid tables");
  db.ExecuteQuery("CREATE TABLE art ("
                  "art_id integer primary key, "
                  "media_id integer, "
                  "media_type text, "
                  "type text, "
                  "url text)");
  db.ExecuteQuery("CREATE TABLE rating ("
                  "rating_id integer primary key, "
                  "media_id integer, "
                  "media_type text, "
                  "rating_type text, "
                  "rating float, "
                  "max float, "
                  "votes integer)");
  db.ExecuteQuery("CREATE TABLE agerating ("
                  "agerating_id integer primary key, "
                  "media_id integer, "
                  "media_type text, "
                  "board text, "
                  "value text, "
                  "descriptors text)");
  db.ExecuteQuery("CREATE TABLE uniqueid ("
                  "uniqueid_id integer primary key, "
                  "media_id integer, "
                  "media_type text, "
                  "type text, "
                  "value text)");
}

void CGameLibraryDDL::CreateLookupTable(CDatabase& db, const char* table, const char* idColumn)
{
  db.ExecuteQuery(db.PrepareSQL("CREATE TABLE %s (%s integer primary key, name text)", table,
                                idColumn));
  db.ExecuteQuery(
      db.PrepareSQL("CREATE TABLE %s_link (%s integer, idGame integer)", table, idColumn));
}

void CGameLibraryDDL::CreateAnalytics(CDatabase& db)
{
  CreateIndexes(db);
  CreateViews(db);
  CreateTriggers(db);
}

void CGameLibraryDDL::CreateLookupIndexes(CDatabase& db, const char* table, const char* idColumn)
{
  db.ExecuteQuery(db.PrepareSQL("CREATE UNIQUE INDEX ix_%s_name ON %s (name(255))", table, table));
  db.ExecuteQuery(db.PrepareSQL("CREATE UNIQUE INDEX ix_%s_link_1 ON %s_link (%s, idGame)", table,
                                table, idColumn));
  db.ExecuteQuery(db.PrepareSQL("CREATE UNIQUE INDEX ix_%s_link_2 ON %s_link (idGame, %s)", table,
                                table, idColumn));
}

void CGameLibraryDDL::CreateIndexes(CDatabase& db)
{
  CLog::Log(LOGINFO, "GAME: Creating library indexes");

  db.ExecuteQuery("CREATE UNIQUE INDEX ix_platform_slug ON platform (slug(64))");
  db.ExecuteQuery("CREATE INDEX ix_platform_sort ON platform (sortName(255))");

  db.ExecuteQuery("CREATE UNIQUE INDEX ix_path_1 ON path (strPath(255))");
  db.ExecuteQuery("CREATE INDEX ix_path_2 ON path (idParentPath)");
  db.ExecuteQuery("CREATE INDEX ix_path_3 ON path (idPlatform)");

  db.ExecuteQuery("CREATE UNIQUE INDEX ix_files_1 ON files (idPath, strFilename(255))");
  db.ExecuteQuery("CREATE INDEX ix_files_2 ON files (idRelease)");
  db.ExecuteQuery("CREATE INDEX ix_files_crc32 ON files (crc32(8))");
  db.ExecuteQuery("CREATE INDEX ix_files_md5 ON files (md5(32))");
  db.ExecuteQuery("CREATE INDEX ix_files_serial ON files (serial(32))");
  db.ExecuteQuery("CREATE INDEX ix_files_rahash ON files (raHash(32))");

  db.ExecuteQuery("CREATE INDEX ix_game_1 ON game (idPlatform, sortTitle(255))");
  db.ExecuteQuery("CREATE INDEX ix_game_2 ON game (idParentGame)");
  db.ExecuteQuery("CREATE INDEX ix_game_3 ON game (idDefaultRelease)");
  db.ExecuteQuery("CREATE INDEX ix_game_4 ON game (idPlatform, year)");
  db.ExecuteQuery("CREATE INDEX ix_game_5 ON game (dateAdded)");
  db.ExecuteQuery("CREATE INDEX ix_game_6 ON game (idPlatform, titleKey(255))");

  db.ExecuteQuery("CREATE INDEX ix_release_1 ON gamerelease (idGame)");

  CreateLookupIndexes(db, "genre", "idGenre");
  CreateLookupIndexes(db, "tag", "idTag");
  db.ExecuteQuery("CREATE UNIQUE INDEX ix_company_name ON company (name(255))");
  db.ExecuteQuery("CREATE UNIQUE INDEX ix_developer_link_1 ON developer_link (idCompany, idGame)");
  db.ExecuteQuery("CREATE UNIQUE INDEX ix_developer_link_2 ON developer_link (idGame, idCompany)");
  db.ExecuteQuery("CREATE UNIQUE INDEX ix_publisher_link_1 ON publisher_link (idCompany, idGame)");
  db.ExecuteQuery("CREATE UNIQUE INDEX ix_publisher_link_2 ON publisher_link (idGame, idCompany)");
  db.ExecuteQuery("CREATE UNIQUE INDEX ix_collection_name ON collection (name(255))");
  db.ExecuteQuery(
      "CREATE UNIQUE INDEX ix_collection_link_1 ON collection_link (idCollection, idGame)");
  db.ExecuteQuery(
      "CREATE UNIQUE INDEX ix_collection_link_2 ON collection_link (idGame, idCollection)");
  db.ExecuteQuery("CREATE UNIQUE INDEX ix_region_code ON region (code(16))");
  db.ExecuteQuery("CREATE UNIQUE INDEX ix_release_region_1 ON release_region (idRegion, idRelease)");
  db.ExecuteQuery("CREATE UNIQUE INDEX ix_release_region_2 ON release_region (idRelease, idRegion)");

  db.ExecuteQuery("CREATE INDEX ix_art ON art (media_id, media_type(20), type(20))");
  db.ExecuteQuery("CREATE INDEX ix_rating ON rating (media_id, media_type(20))");
  db.ExecuteQuery("CREATE INDEX ix_agerating ON agerating (media_id, media_type(20))");
  db.ExecuteQuery("CREATE INDEX ix_uniqueid_1 ON uniqueid (media_id, media_type(20), type(20))");
  db.ExecuteQuery("CREATE INDEX ix_uniqueid_2 ON uniqueid (media_type(20), type(20), value(64))");
}

void CGameLibraryDDL::CreateViews(CDatabase& db)
{
  CLog::Log(LOGINFO, "GAME: Creating library views");

  // A platform with how many games it holds
  db.ExecuteQuery("CREATE VIEW platform_view AS SELECT "
                  "platform.*, "
                  "(SELECT COUNT(1) FROM game WHERE game.idPlatform = platform.idPlatform) AS "
                  "gameCount "
                  "FROM platform");

  // A game resolved to what a list shows: its platform, its default release,
  // the rating and identifier it prefers, and its play state summed over
  // every file of every release
  db.ExecuteQuery(
      "CREATE VIEW game_view AS SELECT "
      "game.*, "
      "platform.slug AS platformSlug, "
      "platform.name AS platformName, "
      "platform.sortName AS platformSortName, "
      "rel.title AS releaseTitle, "
      "rel.regions AS releaseRegions, "
      "rel.languages AS releaseLanguages, "
      "rel.revision AS releaseRevision, "
      "rel.status AS releaseStatus, "
      "rel.licence AS releaseLicence, "
      "rel.serial AS releaseSerial, "
      "rating.rating_type AS ratingType, "
      "rating.rating AS rating, "
      "rating.max AS ratingMax, "
      "rating.votes AS votes, "
      "uniqueid.type AS uniqueIdType, "
      "uniqueid.value AS uniqueIdValue, "
      "(SELECT COUNT(1) FROM gamerelease r WHERE r.idGame = game.idGame) AS releaseCount, "
      "(SELECT COALESCE(SUM(f.playCount), 0) FROM files f JOIN gamerelease r ON f.idRelease = "
      "r.idRelease WHERE r.idGame = game.idGame) AS playCount, "
      "(SELECT MAX(f.lastPlayed) FROM files f JOIN gamerelease r ON f.idRelease = r.idRelease "
      "WHERE r.idGame = game.idGame) AS lastPlayed, "
      "df.idFile AS idFile, "
      "df.strFilename AS strFilename, "
      "dp.strPath AS strPath "
      "FROM game "
      "JOIN platform ON platform.idPlatform = game.idPlatform "
      "LEFT JOIN gamerelease rel ON rel.idRelease = game.idDefaultRelease "
      "LEFT JOIN files df ON df.idFile = (SELECT f.idFile FROM files f WHERE f.idRelease = "
      "game.idDefaultRelease ORDER BY f.discNumber, f.idFile LIMIT 1) "
      "LEFT JOIN path dp ON dp.idPath = df.idPath "
      "LEFT JOIN rating ON rating.rating_id = game.idDefaultRating "
      "LEFT JOIN uniqueid ON uniqueid.uniqueid_id = game.idDefaultUniqueId");

  // A release with the game and platform it belongs to and its first file
  db.ExecuteQuery(
      "CREATE VIEW release_view AS SELECT "
      "gamerelease.*, "
      "game.idPlatform AS idPlatform, "
      "game.title AS gameTitle, "
      "game.sortTitle AS gameSortTitle, "
      "CASE WHEN game.idDefaultRelease = gamerelease.idRelease THEN 1 ELSE 0 END AS isDefault, "
      "(SELECT COUNT(1) FROM files f WHERE f.idRelease = gamerelease.idRelease) AS fileCount "
      "FROM gamerelease "
      "JOIN game ON game.idGame = gamerelease.idGame");
}

void CGameLibraryDDL::CreateTriggers(CDatabase& db)
{
  CLog::Log(LOGINFO, "GAME: Creating library triggers");

  db.ExecuteQuery("CREATE TRIGGER delete_game AFTER DELETE ON game FOR EACH ROW BEGIN "
                  "DELETE FROM gamerelease WHERE idGame = old.idGame; "
                  "DELETE FROM genre_link WHERE idGame = old.idGame; "
                  "DELETE FROM tag_link WHERE idGame = old.idGame; "
                  "DELETE FROM developer_link WHERE idGame = old.idGame; "
                  "DELETE FROM publisher_link WHERE idGame = old.idGame; "
                  "DELETE FROM collection_link WHERE idGame = old.idGame; "
                  "DELETE FROM art WHERE media_id = old.idGame AND media_type = '" MediaTypeGame
                  "'; "
                  "DELETE FROM rating WHERE media_id = old.idGame AND media_type = '" MediaTypeGame
                  "'; "
                  "DELETE FROM agerating WHERE media_id = old.idGame AND media_type "
                  "= '" MediaTypeGame "'; "
                  "DELETE FROM uniqueid WHERE media_id = old.idGame AND media_type "
                  "= '" MediaTypeGame "'; "
                  "UPDATE game SET idParentGame = NULL WHERE idParentGame = old.idGame; "
                  "END");

  db.ExecuteQuery("CREATE TRIGGER delete_release AFTER DELETE ON gamerelease FOR EACH ROW BEGIN "
                  "DELETE FROM files WHERE idRelease = old.idRelease; "
                  "DELETE FROM release_region WHERE idRelease = old.idRelease; "
                  "DELETE FROM art WHERE media_id = old.idRelease AND media_type "
                  "= '" MediaTypeGameRelease "'; "
                  "DELETE FROM uniqueid WHERE media_id = old.idRelease AND media_type "
                  "= '" MediaTypeGameRelease "'; "
                  "END");

  db.ExecuteQuery("CREATE TRIGGER delete_platform AFTER DELETE ON platform FOR EACH ROW BEGIN "
                  "DELETE FROM art WHERE media_id = old.idPlatform AND media_type "
                  "= '" MediaTypeGamePlatform "'; "
                  "DELETE FROM uniqueid WHERE media_id = old.idPlatform AND media_type "
                  "= '" MediaTypeGamePlatform "'; "
                  "UPDATE path SET idPlatform = NULL WHERE idPlatform = old.idPlatform; "
                  "END");

  db.ExecuteQuery("CREATE TRIGGER delete_collection AFTER DELETE ON collection FOR EACH ROW BEGIN "
                  "DELETE FROM collection_link WHERE idCollection = old.idCollection; "
                  "DELETE FROM art WHERE media_id = old.idCollection AND media_type "
                  "= '" MediaTypeGameCollection "'; "
                  "DELETE FROM uniqueid WHERE media_id = old.idCollection AND media_type "
                  "= '" MediaTypeGameCollection "'; "
                  "END");
}

void CGameLibraryDDL::UpdateTables(CDatabase& db, int version)
{
  if (version < FIRST_VERSION)
  {
    CreateTables(db);
    return;
  }

  // 4: a platform remembers the picture its games play with, as it already
  // remembered the emulator
  if (version < 4)
    db.ExecuteQuery("ALTER TABLE platform ADD COLUMN defaultVideoFilter text");
}
