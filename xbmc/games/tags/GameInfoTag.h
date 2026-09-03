/*
 *  Copyright (C) 2012-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "games/library/GameLibraryTypes.h"
#include "utils/IArchivable.h"
#include "utils/ISerializable.h"
#include "utils/ISortable.h"

#include <map>
#include <string>
#include <vector>

namespace KODI
{
namespace GAME
{
/*!
 * \ingroup games
 *
 * \brief Everything a list item knows about a game
 *
 * Filled by the game client when a game is running, by the library when a
 * game is listed from it, and by a sidecar file when one sits beside the game.
 * A tag from the library carries a database ID; one from the other sources
 * does not.
 */
class CGameInfoTag : public IArchivable, public ISerializable, public ISortable
{
public:
  CGameInfoTag() { Reset(); }
  CGameInfoTag(const CGameInfoTag& tag) { *this = tag; }
  CGameInfoTag& operator=(const CGameInfoTag& tag);
  virtual ~CGameInfoTag() = default;
  void Reset();

  bool operator==(const CGameInfoTag& tag) const;
  bool operator!=(const CGameInfoTag& tag) const { return !(*this == tag); }

  bool IsLoaded() const { return m_bLoaded; }
  void SetLoaded(bool bOnOff = true) { m_bLoaded = bOnOff; }

  // File path
  const std::string& GetURL() const { return m_strURL; }
  void SetURL(const std::string& strURL) { m_strURL = strURL; }

  // Title
  const std::string& GetTitle() const { return m_strTitle; }
  void SetTitle(const std::string& strTitle) { m_strTitle = strTitle; }

  // Platform
  const std::string& GetPlatform() const { return m_strPlatform; }
  void SetPlatform(const std::string& strPlatform) { m_strPlatform = strPlatform; }

  // Genres
  const std::vector<std::string>& GetGenres() const { return m_genres; }
  void SetGenres(const std::vector<std::string>& genres) { m_genres = genres; }

  // Developer
  const std::string& GetDeveloper() const { return m_strDeveloper; }
  void SetDeveloper(const std::string& strDeveloper) { m_strDeveloper = strDeveloper; }

  // Overview
  const std::string& GetOverview() const { return m_strOverview; }
  void SetOverview(const std::string& strOverview) { m_strOverview = strOverview; }

  // Year
  unsigned int GetYear() const { return m_year; }
  void SetYear(unsigned int year) { m_year = year; }

  // Game Code (ID)
  const std::string& GetID() const { return m_strID; }
  void SetID(const std::string& strID) { m_strID = strID; }

  // Region
  const std::string& GetRegion() const { return m_strRegion; }
  void SetRegion(const std::string& strRegion) { m_strRegion = strRegion; }

  // Publisher / Licensee
  const std::string& GetPublisher() const { return m_strPublisher; }
  void SetPublisher(const std::string& strPublisher) { m_strPublisher = strPublisher; }

  // Format (PAL/NTSC)
  const std::string& GetFormat() const { return m_strFormat; }
  void SetFormat(const std::string& strFormat) { m_strFormat = strFormat; }

  // Cartridge Type, e.g. "ROM+MBC5+RAM+BATT" or "CD"
  const std::string& GetCartridgeType() const { return m_strCartridgeType; }
  void SetCartridgeType(const std::string& strCartridgeType)
  {
    m_strCartridgeType = strCartridgeType;
  }

  // Game client add-on ID
  const std::string& GetGameClient() const { return m_strGameClient; }
  void SetGameClient(const std::string& strGameClient) { m_strGameClient = strGameClient; }

  /*!
   * \name Library identity
   *
   * Present only when the tag was filled from the library.
   */
  ///@{
  int GetDatabaseId() const { return m_databaseId; }
  void SetDatabaseId(int id) { m_databaseId = id; }
  bool HasDatabaseId() const { return m_databaseId > 0; }

  int GetPlatformId() const { return m_platformId; }
  void SetPlatformId(int id) { m_platformId = id; }

  const std::string& GetPlatformSlug() const { return m_strPlatformSlug; }
  void SetPlatformSlug(const std::string& slug) { m_strPlatformSlug = slug; }

  int GetDefaultReleaseId() const { return m_defaultReleaseId; }
  void SetDefaultReleaseId(int id) { m_defaultReleaseId = id; }

  MatchMethod GetMatchMethod() const { return m_matchMethod; }
  void SetMatchMethod(MatchMethod method) { m_matchMethod = method; }
  ///@}

  /*!
   * \name Titles and text
   */
  ///@{
  const std::string& GetSortTitle() const { return m_strSortTitle; }
  void SetSortTitle(const std::string& title) { m_strSortTitle = title; }

  const std::string& GetOriginalTitle() const { return m_strOriginalTitle; }
  void SetOriginalTitle(const std::string& title) { m_strOriginalTitle = title; }

  const std::string& GetReleaseDate() const { return m_strReleaseDate; }
  void SetReleaseDate(const std::string& date) { m_strReleaseDate = date; }

  const std::string& GetTrailer() const { return m_strTrailer; }
  void SetTrailer(const std::string& url) { m_strTrailer = url; }

  const std::string& GetManual() const { return m_strManual; }
  void SetManual(const std::string& url) { m_strManual = url; }
  ///@}

  /*!
   * \name People and groupings
   *
   * The single developer and publisher above are kept for the game client
   * and older sidecars; these lists are what the library stores.
   */
  ///@{
  const std::vector<std::string>& GetDevelopers() const { return m_developers; }
  void SetDevelopers(const std::vector<std::string>& developers);

  const std::vector<std::string>& GetPublishers() const { return m_publishers; }
  void SetPublishers(const std::vector<std::string>& publishers);

  const std::vector<std::string>& GetCollections() const { return m_collections; }
  void SetCollections(const std::vector<std::string>& collections) { m_collections = collections; }

  const std::vector<std::string>& GetTags() const { return m_tags; }
  void SetTags(const std::vector<std::string>& tags) { m_tags = tags; }
  ///@}

  /*!
   * \name Play facts
   */
  ///@{
  int GetPlayersMin() const { return m_playersMin; }
  int GetPlayersMax() const { return m_playersMax; }
  void SetPlayers(int min, int max);

  bool IsCoop() const { return m_coop; }
  void SetCoop(bool coop) { m_coop = coop; }

  GameCategory GetCategory() const { return m_category; }
  void SetCategory(GameCategory category) { m_category = category; }

  int GetParentGameId() const { return m_parentGameId; }
  void SetParentGameId(int id) { m_parentGameId = id; }
  ///@}

  /*!
   * \name Ratings and identifiers
   */
  ///@{
  const std::map<std::string, GameRating>& GetRatings() const { return m_ratings; }
  void SetRating(const std::string& type, const GameRating& rating);
  void SetRatings(const std::map<std::string, GameRating>& ratings, const std::string& defaultType);

  /*!
   * \brief The rating to show, scaled to 0-10
   */
  GameRating GetRating() const;
  const std::string& GetDefaultRatingType() const { return m_strDefaultRating; }

  const std::vector<GameAgeRating>& GetAgeRatings() const { return m_ageRatings; }
  void SetAgeRatings(const std::vector<GameAgeRating>& ratings) { m_ageRatings = ratings; }

  /*!
   * \brief The age rating to show, e.g. "ESRB E" or "PEGI 12"
   */
  std::string GetAgeRating() const;

  const std::map<std::string, std::string>& GetUniqueIDs() const { return m_uniqueIds; }
  void SetUniqueID(const std::string& type, const std::string& value, bool isDefault = false);
  void SetUniqueIDs(const std::map<std::string, std::string>& ids, const std::string& defaultType);
  std::string GetUniqueID(const std::string& type = "") const;
  const std::string& GetDefaultUniqueIDType() const { return m_strDefaultUniqueId; }
  ///@}

  /*!
   * \name Releases
   */
  ///@{
  const std::vector<GameRelease>& GetReleases() const { return m_releases; }
  void SetReleases(const std::vector<GameRelease>& releases) { m_releases = releases; }
  const GameRelease* GetDefaultRelease() const;
  ///@}

  /*!
   * \name The user's own state
   */
  ///@{
  int GetPlayCount() const { return m_playCount; }
  void SetPlayCount(int count) { m_playCount = count; }

  const std::string& GetLastPlayed() const { return m_strLastPlayed; }
  void SetLastPlayed(const std::string& date) { m_strLastPlayed = date; }

  const std::string& GetDateAdded() const { return m_strDateAdded; }
  void SetDateAdded(const std::string& date) { m_strDateAdded = date; }

  int GetUserRating() const { return m_userRating; }
  void SetUserRating(int rating) { m_userRating = rating; }

  bool IsFavourite() const { return m_favourite; }
  void SetFavourite(bool favourite) { m_favourite = favourite; }

  bool IsCompleted() const { return m_completed; }
  void SetCompleted(bool completed) { m_completed = completed; }

  int GetAchievementsTotal() const { return m_achievementsTotal; }
  int GetAchievementsEarned() const { return m_achievementsEarned; }
  int GetAchievementsHardcore() const { return m_achievementsHardcore; }
  void SetAchievements(int total, int earned, int hardcore);
  bool HasAchievements() const { return m_achievementsTotal > 0; }

  const std::string& GetLastUnlock() const { return m_strLastUnlock; }
  void SetLastUnlock(const std::string& date) { m_strLastUnlock = date; }
  ///@}

  void Archive(CArchive& ar) override;
  void Serialize(CVariant& value) const override;
  void ToSortable(SortItem& sortable, Field field) const override;

private:
  bool m_bLoaded;
  std::string m_strURL;
  std::string m_strTitle;
  std::string m_strPlatform;
  std::vector<std::string> m_genres;
  std::string m_strDeveloper;
  std::string m_strOverview;
  unsigned int m_year;
  std::string m_strID;
  std::string m_strRegion;
  std::string m_strPublisher;
  std::string m_strFormat;
  std::string m_strCartridgeType;
  std::string m_strGameClient;

  // Library identity
  int m_databaseId;
  int m_platformId;
  std::string m_strPlatformSlug;
  int m_defaultReleaseId;
  MatchMethod m_matchMethod;

  // Titles and text
  std::string m_strSortTitle;
  std::string m_strOriginalTitle;
  std::string m_strReleaseDate;
  std::string m_strTrailer;
  std::string m_strManual;

  // People and groupings
  std::vector<std::string> m_developers;
  std::vector<std::string> m_publishers;
  std::vector<std::string> m_collections;
  std::vector<std::string> m_tags;

  // Play facts
  int m_playersMin;
  int m_playersMax;
  bool m_coop;
  GameCategory m_category;
  int m_parentGameId;

  // Ratings and identifiers
  std::map<std::string, GameRating> m_ratings;
  std::string m_strDefaultRating;
  std::vector<GameAgeRating> m_ageRatings;
  std::map<std::string, std::string> m_uniqueIds;
  std::string m_strDefaultUniqueId;

  // Releases
  std::vector<GameRelease> m_releases;

  // The user's own state
  int m_playCount;
  std::string m_strLastPlayed;
  std::string m_strDateAdded;
  int m_userRating;
  bool m_favourite;
  bool m_completed;
  int m_achievementsTotal;
  int m_achievementsEarned;
  int m_achievementsHardcore;
  std::string m_strLastUnlock;
};
} // namespace GAME
} // namespace KODI
