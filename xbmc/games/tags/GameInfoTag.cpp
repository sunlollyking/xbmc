/*
 *  Copyright (C) 2012-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "GameInfoTag.h"

#include "utils/Archive.h"
#include "utils/StringUtils.h"
#include "utils/Variant.h"

#include <algorithm>
#include <string>

using namespace KODI;
using namespace GAME;

namespace
{
constexpr int ARCHIVE_VERSION = 2;
} // namespace

void CGameInfoTag::Reset()
{
  m_bLoaded = false;
  m_strURL.clear();
  m_strTitle.clear();
  m_strPlatform.clear();
  m_genres.clear();
  m_strDeveloper.clear();
  m_strOverview.clear();
  m_year = 0;
  m_strID.clear();
  m_strRegion.clear();
  m_strPublisher.clear();
  m_strFormat.clear();
  m_strCartridgeType.clear();
  m_strGameClient.clear();

  m_databaseId = -1;
  m_platformId = -1;
  m_strPlatformSlug.clear();
  m_defaultReleaseId = -1;
  m_matchMethod = MatchMethod::NONE;

  m_strSortTitle.clear();
  m_strOriginalTitle.clear();
  m_strReleaseDate.clear();
  m_strTrailer.clear();
  m_strManual.clear();

  m_developers.clear();
  m_publishers.clear();
  m_collections.clear();
  m_tags.clear();

  m_playersMin = 0;
  m_playersMax = 0;
  m_coop = false;
  m_category = GameCategory::RETAIL;
  m_parentGameId = -1;

  m_ratings.clear();
  m_strDefaultRating.clear();
  m_ageRatings.clear();
  m_uniqueIds.clear();
  m_strDefaultUniqueId.clear();

  m_releases.clear();
  m_releaseCount = 0;

  m_playCount = 0;
  m_strLastPlayed.clear();
  m_strDateAdded.clear();
  m_userRating = 0;
  m_favourite = false;
  m_completed = false;
  m_achievementsTotal = 0;
  m_achievementsEarned = 0;
  m_achievementsHardcore = 0;
  m_strLastUnlock.clear();
}

CGameInfoTag& CGameInfoTag::operator=(const CGameInfoTag& tag)
{
  if (this != &tag)
  {
    m_bLoaded = tag.m_bLoaded;
    m_strURL = tag.m_strURL;
    m_strTitle = tag.m_strTitle;
    m_strPlatform = tag.m_strPlatform;
    m_genres = tag.m_genres;
    m_strDeveloper = tag.m_strDeveloper;
    m_strOverview = tag.m_strOverview;
    m_year = tag.m_year;
    m_strID = tag.m_strID;
    m_strRegion = tag.m_strRegion;
    m_strPublisher = tag.m_strPublisher;
    m_strFormat = tag.m_strFormat;
    m_strCartridgeType = tag.m_strCartridgeType;
    m_strGameClient = tag.m_strGameClient;

    m_databaseId = tag.m_databaseId;
    m_platformId = tag.m_platformId;
    m_strPlatformSlug = tag.m_strPlatformSlug;
    m_defaultReleaseId = tag.m_defaultReleaseId;
    m_matchMethod = tag.m_matchMethod;

    m_strSortTitle = tag.m_strSortTitle;
    m_strOriginalTitle = tag.m_strOriginalTitle;
    m_strReleaseDate = tag.m_strReleaseDate;
    m_strTrailer = tag.m_strTrailer;
    m_strManual = tag.m_strManual;

    m_developers = tag.m_developers;
    m_publishers = tag.m_publishers;
    m_collections = tag.m_collections;
    m_tags = tag.m_tags;

    m_playersMin = tag.m_playersMin;
    m_playersMax = tag.m_playersMax;
    m_coop = tag.m_coop;
    m_category = tag.m_category;
    m_parentGameId = tag.m_parentGameId;

    m_ratings = tag.m_ratings;
    m_strDefaultRating = tag.m_strDefaultRating;
    m_ageRatings = tag.m_ageRatings;
    m_uniqueIds = tag.m_uniqueIds;
    m_strDefaultUniqueId = tag.m_strDefaultUniqueId;

    m_releases = tag.m_releases;
    m_releaseCount = tag.m_releaseCount;

    m_playCount = tag.m_playCount;
    m_strLastPlayed = tag.m_strLastPlayed;
    m_strDateAdded = tag.m_strDateAdded;
    m_userRating = tag.m_userRating;
    m_favourite = tag.m_favourite;
    m_completed = tag.m_completed;
    m_achievementsTotal = tag.m_achievementsTotal;
    m_achievementsEarned = tag.m_achievementsEarned;
    m_achievementsHardcore = tag.m_achievementsHardcore;
    m_strLastUnlock = tag.m_strLastUnlock;
  }
  return *this;
}

bool CGameInfoTag::operator==(const CGameInfoTag& tag) const
{
  if (this == &tag)
    return true;

  if (m_bLoaded != tag.m_bLoaded)
    return false;

  if (!m_bLoaded)
    return true;

  return m_strURL == tag.m_strURL && m_strTitle == tag.m_strTitle &&
         m_strPlatform == tag.m_strPlatform && m_genres == tag.m_genres &&
         m_strDeveloper == tag.m_strDeveloper && m_strOverview == tag.m_strOverview &&
         m_year == tag.m_year && m_strID == tag.m_strID && m_strRegion == tag.m_strRegion &&
         m_strPublisher == tag.m_strPublisher && m_strFormat == tag.m_strFormat &&
         m_strCartridgeType == tag.m_strCartridgeType && m_strGameClient == tag.m_strGameClient &&
         m_databaseId == tag.m_databaseId && m_platformId == tag.m_platformId &&
         m_defaultReleaseId == tag.m_defaultReleaseId && m_strSortTitle == tag.m_strSortTitle &&
         m_strOriginalTitle == tag.m_strOriginalTitle &&
         m_strReleaseDate == tag.m_strReleaseDate && m_developers == tag.m_developers &&
         m_publishers == tag.m_publishers && m_collections == tag.m_collections &&
         m_tags == tag.m_tags && m_playersMin == tag.m_playersMin &&
         m_playersMax == tag.m_playersMax && m_coop == tag.m_coop &&
         m_category == tag.m_category && m_uniqueIds == tag.m_uniqueIds &&
         m_playCount == tag.m_playCount && m_strLastPlayed == tag.m_strLastPlayed &&
         m_userRating == tag.m_userRating && m_favourite == tag.m_favourite &&
         m_completed == tag.m_completed;
}

void CGameInfoTag::SetDevelopers(const std::vector<std::string>& developers)
{
  m_developers = developers;
  if (m_strDeveloper.empty() && !developers.empty())
    m_strDeveloper = StringUtils::Join(developers, ", ");
}

void CGameInfoTag::SetPublishers(const std::vector<std::string>& publishers)
{
  m_publishers = publishers;
  if (m_strPublisher.empty() && !publishers.empty())
    m_strPublisher = StringUtils::Join(publishers, ", ");
}

void CGameInfoTag::SetPlayers(int min, int max)
{
  m_playersMin = std::max(min, 0);
  m_playersMax = std::max(max, m_playersMin);
}

void CGameInfoTag::SetRating(const std::string& type, const GameRating& rating)
{
  m_ratings[type] = rating;
  if (m_strDefaultRating.empty())
    m_strDefaultRating = type;
}

void CGameInfoTag::SetRatings(const std::map<std::string, GameRating>& ratings,
                              const std::string& defaultType)
{
  m_ratings = ratings;
  if (ratings.contains(defaultType))
    m_strDefaultRating = defaultType;
  else
    m_strDefaultRating = ratings.empty() ? "" : ratings.begin()->first;
}

GameRating CGameInfoTag::GetRating() const
{
  const auto it = m_ratings.find(m_strDefaultRating);
  if (it == m_ratings.end())
    return {};

  GameRating rating = it->second;
  if (rating.max > 0.0f && rating.max != 10.0f)
  {
    rating.rating = rating.rating * 10.0f / rating.max;
    rating.max = 10.0f;
  }
  return rating;
}

std::string CGameInfoTag::GetAgeRating() const
{
  if (m_ageRatings.empty())
    return "";

  const GameAgeRating& rating = m_ageRatings.front();
  if (rating.board.empty())
    return rating.value;

  return rating.board + " " + rating.value;
}

void CGameInfoTag::SetUniqueID(const std::string& type, const std::string& value, bool isDefault)
{
  if (type.empty() || value.empty())
    return;

  m_uniqueIds[type] = value;
  if (isDefault || m_strDefaultUniqueId.empty())
    m_strDefaultUniqueId = type;
}

void CGameInfoTag::SetUniqueIDs(const std::map<std::string, std::string>& ids,
                                const std::string& defaultType)
{
  m_uniqueIds = ids;
  if (ids.contains(defaultType))
    m_strDefaultUniqueId = defaultType;
  else
    m_strDefaultUniqueId = ids.empty() ? "" : ids.begin()->first;
}

std::string CGameInfoTag::GetUniqueID(const std::string& type) const
{
  const auto it = m_uniqueIds.find(type.empty() ? m_strDefaultUniqueId : type);
  if (it == m_uniqueIds.end())
    return "";
  return it->second;
}

void CGameInfoTag::SetReleases(const std::vector<GameRelease>& releases)
{
  m_releases = releases;
  m_releaseCount = static_cast<int>(releases.size());
}

const GameRelease* CGameInfoTag::GetDefaultRelease() const
{
  for (const GameRelease& release : m_releases)
  {
    if (release.id == m_defaultReleaseId || (m_defaultReleaseId < 0 && release.isDefault))
      return &release;
  }
  return m_releases.empty() ? nullptr : &m_releases.front();
}

void CGameInfoTag::SetAchievements(int total, int earned, int hardcore)
{
  m_achievementsTotal = std::max(total, 0);
  m_achievementsEarned = std::clamp(earned, 0, m_achievementsTotal);
  m_achievementsHardcore = std::clamp(hardcore, 0, m_achievementsTotal);
}

void CGameInfoTag::Archive(CArchive& ar)
{
  if (ar.IsStoring())
  {
    ar << m_bLoaded;
    ar << m_strURL;
    ar << m_strTitle;
    ar << m_strPlatform;
    ar << m_genres;
    ar << m_strDeveloper;
    ar << m_strOverview;
    ar << m_year;
    ar << m_strID;
    ar << m_strRegion;
    ar << m_strPublisher;
    ar << m_strFormat;
    ar << m_strCartridgeType;
    ar << m_strGameClient;

    ar << ARCHIVE_VERSION;
    ar << m_databaseId;
    ar << m_platformId;
    ar << m_strPlatformSlug;
    ar << m_defaultReleaseId;
    ar << static_cast<int>(m_matchMethod);
    ar << m_strSortTitle;
    ar << m_strOriginalTitle;
    ar << m_strReleaseDate;
    ar << m_strTrailer;
    ar << m_strManual;
    ar << m_developers;
    ar << m_publishers;
    ar << m_collections;
    ar << m_tags;
    ar << m_playersMin;
    ar << m_playersMax;
    ar << m_coop;
    ar << static_cast<int>(m_category);
    ar << m_parentGameId;
    ar << static_cast<int>(m_ratings.size());
    for (const auto& [type, rating] : m_ratings)
    {
      ar << type;
      ar << rating.rating;
      ar << rating.max;
      ar << rating.votes;
    }
    ar << m_strDefaultRating;
    ar << static_cast<int>(m_ageRatings.size());
    for (const GameAgeRating& rating : m_ageRatings)
    {
      ar << rating.board;
      ar << rating.value;
      ar << rating.descriptors;
    }
    ar << static_cast<int>(m_uniqueIds.size());
    for (const auto& [type, value] : m_uniqueIds)
    {
      ar << type;
      ar << value;
    }
    ar << m_strDefaultUniqueId;
    ar << m_playCount;
    ar << m_strLastPlayed;
    ar << m_strDateAdded;
    ar << m_userRating;
    ar << m_favourite;
    ar << m_completed;
    ar << m_achievementsTotal;
    ar << m_achievementsEarned;
    ar << m_achievementsHardcore;
    ar << m_strLastUnlock;
  }
  else
  {
    ar >> m_bLoaded;
    ar >> m_strURL;
    ar >> m_strTitle;
    ar >> m_strPlatform;
    ar >> m_genres;
    ar >> m_strDeveloper;
    ar >> m_strOverview;
    ar >> m_year;
    ar >> m_strID;
    ar >> m_strRegion;
    ar >> m_strPublisher;
    ar >> m_strFormat;
    ar >> m_strCartridgeType;
    ar >> m_strGameClient;

    int version = 0;
    ar >> version;
    if (version < ARCHIVE_VERSION)
      return;

    int value = 0;
    ar >> m_databaseId;
    ar >> m_platformId;
    ar >> m_strPlatformSlug;
    ar >> m_defaultReleaseId;
    ar >> value;
    m_matchMethod = static_cast<MatchMethod>(value);
    ar >> m_strSortTitle;
    ar >> m_strOriginalTitle;
    ar >> m_strReleaseDate;
    ar >> m_strTrailer;
    ar >> m_strManual;
    ar >> m_developers;
    ar >> m_publishers;
    ar >> m_collections;
    ar >> m_tags;
    ar >> m_playersMin;
    ar >> m_playersMax;
    ar >> m_coop;
    ar >> value;
    m_category = static_cast<GameCategory>(value);
    ar >> m_parentGameId;
    int count = 0;
    ar >> count;
    m_ratings.clear();
    for (int i = 0; i < count; ++i)
    {
      std::string type;
      GameRating rating;
      ar >> type;
      ar >> rating.rating;
      ar >> rating.max;
      ar >> rating.votes;
      m_ratings[type] = rating;
    }
    ar >> m_strDefaultRating;
    ar >> count;
    m_ageRatings.clear();
    for (int i = 0; i < count; ++i)
    {
      GameAgeRating rating;
      ar >> rating.board;
      ar >> rating.value;
      ar >> rating.descriptors;
      m_ageRatings.emplace_back(std::move(rating));
    }
    ar >> count;
    m_uniqueIds.clear();
    for (int i = 0; i < count; ++i)
    {
      std::string type;
      std::string id;
      ar >> type;
      ar >> id;
      m_uniqueIds[type] = id;
    }
    ar >> m_strDefaultUniqueId;
    ar >> m_playCount;
    ar >> m_strLastPlayed;
    ar >> m_strDateAdded;
    ar >> m_userRating;
    ar >> m_favourite;
    ar >> m_completed;
    ar >> m_achievementsTotal;
    ar >> m_achievementsEarned;
    ar >> m_achievementsHardcore;
    ar >> m_strLastUnlock;
  }
}

void CGameInfoTag::Serialize(CVariant& value) const
{
  value["loaded"] = m_bLoaded;
  value["url"] = m_strURL;
  value["name"] = m_strTitle;
  value["platform"] = m_strPlatform;
  value["genres"] = m_genres;
  value["developer"] = m_strDeveloper;
  value["overview"] = m_strOverview;
  value["year"] = m_year;
  value["id"] = m_strID;
  value["region"] = m_strRegion;
  value["publisher"] = m_strPublisher;
  value["format"] = m_strFormat;
  value["cartridgetype"] = m_strCartridgeType;
  value["gameclient"] = m_strGameClient;

  if (HasDatabaseId())
    value["gameid"] = m_databaseId;
  if (m_platformId > 0)
    value["platformid"] = m_platformId;
  value["platformslug"] = m_strPlatformSlug;
  value["sorttitle"] = m_strSortTitle;
  value["originaltitle"] = m_strOriginalTitle;
  value["releasedate"] = m_strReleaseDate;
  value["trailer"] = m_strTrailer;
  value["manual"] = m_strManual;
  value["developers"] = m_developers;
  value["publishers"] = m_publishers;
  value["collections"] = m_collections;
  value["tags"] = m_tags;
  value["playersmin"] = m_playersMin;
  value["playersmax"] = m_playersMax;
  value["coop"] = m_coop;
  value["category"] = std::string(CGameLibraryTypes::ToString(m_category));
  value["matchedby"] = std::string(CGameLibraryTypes::ToString(m_matchMethod));

  const GameRating rating = GetRating();
  value["rating"] = rating.rating;
  value["votes"] = rating.votes;
  value["ratings"] = CVariant(CVariant::VariantTypeObject);
  for (const auto& [type, r] : m_ratings)
  {
    value["ratings"][type]["rating"] = r.rating;
    value["ratings"][type]["max"] = r.max;
    value["ratings"][type]["votes"] = r.votes;
    value["ratings"][type]["default"] = (type == m_strDefaultRating);
  }
  value["agerating"] = GetAgeRating();
  value["ageratings"] = CVariant(CVariant::VariantTypeArray);
  for (const GameAgeRating& age : m_ageRatings)
  {
    CVariant entry;
    entry["board"] = age.board;
    entry["value"] = age.value;
    entry["descriptors"] = age.descriptors;
    value["ageratings"].push_back(entry);
  }
  value["uniqueid"] = CVariant(CVariant::VariantTypeObject);
  for (const auto& [type, id] : m_uniqueIds)
    value["uniqueid"][type] = id;

  value["releasecount"] = m_releaseCount;
  value["releases"] = CVariant(CVariant::VariantTypeArray);
  for (const GameRelease& release : m_releases)
  {
    CVariant entry;
    entry["releaseid"] = release.id;
    entry["title"] = release.title;
    entry["regions"] = release.regions;
    entry["languages"] = release.languages;
    entry["revision"] = release.revision;
    entry["status"] = std::string(CGameLibraryTypes::ToString(release.status));
    entry["licence"] = std::string(CGameLibraryTypes::ToString(release.licence));
    entry["default"] = release.isDefault;
    entry["files"] = CVariant(CVariant::VariantTypeArray);
    for (const GameFile& file : release.files)
      entry["files"].push_back(file.path);
    value["releases"].push_back(entry);
  }

  value["playcount"] = m_playCount;
  value["lastplayed"] = m_strLastPlayed;
  value["dateadded"] = m_strDateAdded;
  value["userrating"] = m_userRating;
  value["favourite"] = m_favourite;
  value["completed"] = m_completed;
  value["achievementstotal"] = m_achievementsTotal;
  value["achievementsearned"] = m_achievementsEarned;
  value["achievementshardcore"] = m_achievementsHardcore;
  value["lastunlock"] = m_strLastUnlock;
}

void CGameInfoTag::ToSortable(SortItem& sortable, Field field) const
{
  switch (field)
  {
    case Field::TITLE:
      if (!m_strTitle.empty() || !sortable.contains(Field::TITLE))
        sortable[Field::TITLE] = m_strTitle;
      break;
    case Field::SORT_TITLE:
      sortable[Field::SORT_TITLE] = m_strSortTitle.empty() ? m_strTitle : m_strSortTitle;
      break;
    case Field::ORIGINAL_TITLE:
      sortable[Field::ORIGINAL_TITLE] = m_strOriginalTitle;
      break;
    case Field::YEAR:
      sortable[Field::YEAR] = m_year;
      break;
    case Field::GENRE:
      sortable[Field::GENRE] = m_genres;
      break;
    case Field::STUDIO:
      sortable[Field::STUDIO] = m_publishers;
      break;
    case Field::PLOT:
      sortable[Field::PLOT] = m_strOverview;
      break;
    case Field::RATING:
      sortable[Field::RATING] = GetRating().rating;
      break;
    case Field::VOTES:
      sortable[Field::VOTES] = GetRating().votes;
      break;
    case Field::USER_RATING:
      sortable[Field::USER_RATING] = m_userRating;
      break;
    case Field::MPAA:
      sortable[Field::MPAA] = GetAgeRating();
      break;
    case Field::PLAYCOUNT:
      sortable[Field::PLAYCOUNT] = m_playCount;
      break;
    case Field::LAST_PLAYED:
      sortable[Field::LAST_PLAYED] = m_strLastPlayed;
      break;
    case Field::DATE_ADDED:
      sortable[Field::DATE_ADDED] = m_strDateAdded;
      break;
    case Field::SET:
      sortable[Field::SET] = m_collections.empty() ? "" : m_collections.front();
      break;
    case Field::TAG:
      sortable[Field::TAG] = m_tags;
      break;
    case Field::PATH:
      if (!m_strURL.empty() || !sortable.contains(Field::PATH))
        sortable[Field::PATH] = m_strURL;
      break;
    default:
      break;
  }
}
