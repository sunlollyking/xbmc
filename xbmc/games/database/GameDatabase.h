/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "GameClientTable.h"
#include "VideoFilterTable.h"
#include "dbwrappers/Database.h"
#include "games/library/GameLibraryTypes.h"
#include "games/library/GameScraper.h"
#include "media/MediaType.h"
#include "utils/Artwork.h"

#include <map>
#include <set>
#include <string>
#include <vector>

#include <tinyxml2.h>

class CFileItem;
class CFileItemList;
class CDbUrl;
struct SortDescription;

namespace dbiplus
{
class field_value;
using sql_record = std::vector<field_value>;
} // namespace dbiplus

namespace KODI
{
namespace GAME
{
class CGameInfoTag;
class CGameDbUrl;

/*!
 * \brief Base name of the database file, before the schema version suffix
 *
 * Kodi opens "Games" + the schema version, so this is the name every version
 * of the file shares. Changing it orphans the user's existing database.
 */
constexpr const char* GAME_DATABASE_NAME = "Games";

/*!
 * \ingroup games
 *
 * \brief What the scanner needs to know about a folder it was pointed at
 *
 * Stored on the path table and inherited by every folder below it, so a
 * whole platform is set up once at its root.
 */
struct GamePathContent
{
  int idPlatform{-1};
  std::string scraper;
  std::string settings;
  bool scanRecursive{true};
  bool useFolderNames{false};
  bool noUpdate{false};
  bool exclude{false};

  bool HasContent() const { return idPlatform > 0; }
};

/*!
 * \ingroup games
 *
 * \brief The database of everything Kodi remembers about games
 *
 * The database owns the file and its schema version. The per-path tables
 * (which emulator, which video filter) each own their schema and queries and
 * are reached through an accessor. The library tables share one schema in
 * CGameLibraryDDL and are queried through the methods below, grouped by what
 * they answer about: paths and files, platforms, games, and art.
 */
class CGameDatabase : public CDatabase
{
public:
  CGameDatabase();
  ~CGameDatabase() override;

  // Implementation of CDatabase
  bool Open() override;
  bool GetFilter(CDbUrl& dbUrl, Filter& filter, SortDescription& sorting) override;

  /*!
   * \brief The table remembering which emulator to open a game with
   */
  CGameClientTable& GameClients() { return m_gameClients; }

  /*!
   * \brief The table remembering how a game should be drawn
   */
  CVideoFilterTable& VideoFilters() { return m_videoFilters; }

  /*!
   * \name Paths and files
   */
  ///@{
  /*!
   * \brief Remember a folder, returning its ID whether it was new or not
   */
  int AddPath(const std::string& path, const std::string& parentPath = "");
  int GetPathId(const std::string& path);
  bool GetPathHash(const std::string& path, std::string& hash);
  bool SetPathHash(const std::string& path, const std::string& hash);

  /*!
   * \brief Record what a folder holds and how to scan it
   */
  bool SetPathContent(const std::string& path, const GamePathContent& content);

  /*!
   * \brief What applies to a folder: its own content, or the nearest folder
   *        above it that has some
   *
   * \param foundDirectly True when the folder itself carries the content
   *
   * \return False when nothing applies or the folder is excluded
   */
  bool GetPathContent(const std::string& path, GamePathContent& content, bool& foundDirectly);

  /*!
   * \brief Every folder that has been given a platform: the roots a library
   *        update scans
   */
  bool GetContentPaths(std::vector<std::string>& paths);

  /*!
   * \brief Every remembered folder under a root, root included
   */
  bool GetSubPaths(const std::string& basePath, std::vector<std::pair<int, std::string>>& paths);

  /*!
   * \brief Remember a file as part of a release, returning its ID
   *
   * A file already known is updated rather than duplicated.
   */
  int AddFile(const std::string& fileNameAndPath, int idRelease, const GameFile& file);
  int GetFileId(const std::string& fileNameAndPath);
  int GetGameIdByFile(const std::string& fileNameAndPath);
  bool GetFilesForRelease(int idRelease, std::vector<GameFile>& files);

  /*!
   * \brief Count a play of a file, now
   */
  bool MarkPlayed(const std::string& fileNameAndPath);
  ///@}

  /*!
   * \name Platforms
   */
  ///@{
  /*!
   * \brief Add a platform, or update the one with the same slug
   *
   * \return The platform's ID, or -1
   */
  int AddPlatform(const PlatformInfo& platform);

  //! \brief Store what a scraper said about a platform
  bool SetPlatformDetails(const PlatformInfo& platform);

  /*!
   * \brief What a platform's games play with unless a game says otherwise
   *
   * A collection is arranged by machine, and the emulator and the picture
   * belong to the machine rather than to the folder a file happens to sit in.
   */
  bool SetPlatformDefaults(int idPlatform,
                           const std::string& gameClient,
                           const std::string& videoFilter);

  //! \brief When a scraper last described a game, or empty if none has
  std::string GetLastScraped(int idGame);

  //! \brief The platform of the game a file belongs to, or -1
  int GetPlatformIdForGame(const std::string& path);

  /*!
   * \brief Every disc of the game a file belongs to, in order
   *
   * A game of several discs is one game in the library, so the disc manager is
   * handed the whole set rather than the one file that was launched. Empty
   * where the game has a single disc or is not in the library.
   */
  std::vector<std::string> GetDiscsForFile(const std::string& path);
  bool GetPlatform(int idPlatform, PlatformInfo& platform);
  bool GetPlatformBySlug(const std::string& slug, PlatformInfo& platform);
  bool GetPlatforms(std::vector<PlatformInfo>& platforms, bool onlyWithGames = false);
  bool GetPlatformsNav(const std::string& baseDir, CFileItemList& items, bool onlyWithGames = true);

  /*!
   * \brief The platform a folder or file belongs to, from the nearest folder
   *        above it that was given one
   */
  int GetPlatformIdForPath(const std::string& path);
  ///@}

  /*!
   * \name Games
   */
  ///@{
  /*!
   * \brief Store a game and everything hanging off it
   *
   * Writes the game, its links, ratings, identifiers, releases and their
   * files, and its art. A game with a database ID is updated in place.
   *
   * \return The game's ID, also written back to the tag, or -1
   */
  int SetDetailsForGame(CGameInfoTag& details, const KODI::ART::Artwork& art);

  /*!
   * \brief Everything about a game
   */
  bool GetGameInfo(int idGame, CGameInfoTag& details);

  /*!
   * \brief The games a gamedb:// URL lists
   */
  bool GetGamesNav(const std::string& baseDir,
                   CFileItemList& items,
                   const SortDescription& sortDescription);
  bool GetGamesByWhere(const std::string& baseDir,
                       const Filter& filter,
                       CFileItemList& items,
                       const SortDescription& sortDescription);

  /*!
   * \brief The values of a facet (genres, years, ...) a gamedb:// URL lists,
   *        each with how many games have it
   */
  bool GetFacetNav(const std::string& baseDir, CFileItemList& items);

  /*!
   * \brief The releases of a game, as items that play
   */
  bool GetReleasesNav(const std::string& baseDir, CFileItemList& items);

  bool DeleteGame(int idGame);
  int FindGameByUniqueId(int idPlatform, const std::string& type, const std::string& value);
  int FindGameByTitleKey(int idPlatform, const std::string& titleKey);
  bool SetDefaultRelease(int idGame, int idRelease);
  bool SetFavourite(int idGame, bool favourite);
  bool SetCompleted(int idGame, bool completed);

  //! \brief How often a game was played, over every file it has
  bool SetPlayCount(int idGame, int count);

  /*!
   * \brief Record how far the signed-in person has got with each game
   *
   * Keyed by the id the source keeps the count under. A game not named in
   * the answer has not been played, so its count is cleared rather than left
   * to go stale.
   *
   * \return How many games the answer matched
   */
  int SetAchievementProgress(const std::string& source,
                             const std::map<std::string, GameProgress>& progress);

  /*!
   * \brief Write the library out as XML
   *
   * \param path Where to write it
   * \param singleFile One gamedb.xml for the whole library, rather than an
   *                   NFO beside each game
   * \param images Copy the artwork alongside
   * \param overwrite Replace files that are already there
   */
  void ExportToXML(const std::string& path,
                   bool singleFile = true,
                   bool images = false,
                   bool overwrite = false);

  /*!
   * \brief Read back what a person did with their games
   *
   * Matches an exported game to one in the library by the file it came from,
   * then by whichever catalogue named it. Only play count, favourite,
   * completed and the person's own rating are restored: everything a scraper
   * knows, a rescan fetches again and more recently.
   */
  void ImportFromXML(const std::string& path);

  //! \brief The classification boards a person wants to see, in their order
  static std::vector<std::string> PreferredAgeRatingBoards();
  bool SetUserRating(int idGame, int rating);

  /*!
   * \brief Whether any game has been added
   */
  bool HasContent();

  /*!
   * \brief Forget every file that no longer exists, and whatever is left
   *        with no files
   *
   * \param paths Limit the check to these path IDs, or empty for everything
   *
   * \return The number of games removed, or -1 on failure
   */
  int CleanDatabase(const std::set<int>& paths = {});
  ///@}

  /*!
   * \name Art
   */
  ///@{
  bool SetArtForItem(int mediaId,
                     const MediaType& mediaType,
                     const std::string& artType,
                     const std::string& url);
  bool SetArtForItem(int mediaId, const MediaType& mediaType, const KODI::ART::Artwork& art);
  bool GetArtForItem(int mediaId, const MediaType& mediaType, KODI::ART::Artwork& art);

  /*!
   * \brief Read the art of many items at once, by id
   *
   * A listing of a full set would otherwise be one query per game.
   */
  bool GetArtForItems(const std::vector<int>& mediaIds,
                      const MediaType& mediaType,
                      std::map<int, KODI::ART::Artwork>& art);

  //! \brief Let a skin that asks for a thumb or a poster find the box front
  static void AddDefaultArt(KODI::ART::Artwork& art);
  std::string GetArtForItem(int mediaId, const MediaType& mediaType, const std::string& artType);
  bool RemoveArtForItem(int mediaId, const MediaType& mediaType, const std::string& artType);
  bool GetArtTypes(const MediaType& mediaType, std::vector<std::string>& artTypes);
  ///@}

protected:
  // Implementation of CDatabase
  void CreateTables() override;
  void CreateAnalytics() override;
  void UpdateTables(int version) override;
  int GetSchemaVersion() const override { return 4; }
  const char* GetBaseDBName() const override { return GAME_DATABASE_NAME; }

private:
  std::string GetPlayPathForGame(int idGame);
  void ExportArt(int idGame,
                 const std::string& playPath,
                 const std::string& artDir,
                 bool overwrite);
  int FindGameForImport(const tinyxml2::XMLElement* element, const CGameInfoTag& tag);

  int RunQuery(const std::string& sql);

  // Lookup tables (genre, tag, company, collection, region)
  int AddLookup(const std::string& table,
                const std::string& idColumn,
                const std::string& name,
                const std::string& nameColumn = "name");
  void SetLinks(const std::string& linkTable,
                const std::string& idColumn,
                const std::string& lookupTable,
                const std::string& lookupIdColumn,
                int idGame,
                const std::vector<std::string>& names);
  std::vector<std::string> GetLinkNames(const std::string& linkTable,
                                        const std::string& idColumn,
                                        const std::string& lookupTable,
                                        const std::string& lookupIdColumn,
                                        int idGame,
                                        const std::string& nameColumn = "name");

  // Games
  void GetDetailsForGame(const dbiplus::sql_record* record, CGameInfoTag& details);
  void GetReleasesForGame(int idGame, std::vector<GameRelease>& releases);
  int SetReleaseForGame(int idGame, GameRelease& release);
  void SetRatingsForGame(int idGame, CGameInfoTag& details);
  void SetUniqueIdsForGame(int idGame, CGameInfoTag& details);
  void SetAgeRatingsForGame(int idGame, const CGameInfoTag& details);
  void LoadRatings(int idGame, CGameInfoTag& details);
  void LoadUniqueIds(int idGame, CGameInfoTag& details);
  void LoadAgeRatings(int idGame, CGameInfoTag& details);
  bool GetFilter(CGameDbUrl& url, Filter& filter, SortDescription& sorting);

  // Platforms
  void GetPlatformFromRecord(PlatformInfo& platform);
  void SetPlatformIds(int idPlatform, const std::map<std::string, std::string>& providerIds);

  // Tables
  CGameClientTable m_gameClients{*this};
  CVideoFilterTable m_videoFilters{*this};
};
} // namespace GAME
} // namespace KODI
