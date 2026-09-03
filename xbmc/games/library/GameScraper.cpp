/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "GameScraper.h"

#include "FileItem.h"
#include "FileItemList.h"
#include "ServiceBroker.h"
#include "URL.h"
#include "addons/AddonManager.h"
#include "addons/Scraper.h"
#include "addons/addoninfo/AddonType.h"
#include "filesystem/Directory.h"
#include "filesystem/PluginDirectory.h"
#include "games/tags/GameInfoTag.h"
#include "utils/JSONVariantParser.h"
#include "utils/JSONVariantWriter.h"
#include "utils/StringUtils.h"
#include "utils/Variant.h"
#include "utils/log.h"

using namespace KODI;
using namespace GAME;

namespace
{
constexpr int PROTOCOL_VERSION = 1;
constexpr const char* PROPERTY_CANDIDATE = "gamelibrary.candidate";
constexpr const char* PROPERTY_DETAILS = "gamelibrary.details";
constexpr const char* PROPERTY_PLATFORM = "gamelibrary.platform";

std::vector<std::string> Strings(const CVariant& value)
{
  std::vector<std::string> out;
  if (value.isArray())
  {
    for (auto it = value.begin_array(); it != value.end_array(); ++it)
    {
      if (it->isString() && !it->asString().empty())
        out.emplace_back(it->asString());
    }
  }
  else if (value.isString() && !value.asString().empty())
  {
    out.emplace_back(value.asString());
  }
  return out;
}

bool ParsePayload(const CFileItem& item, const char* property, CVariant& payload)
{
  const std::string json = item.GetProperty(property).asString();
  if (json.empty())
    return false;
  if (!CJSONVariantParser::Parse(json, payload) || !payload.isObject())
  {
    CLog::Log(LOGERROR, "GAME: Scraper answered with unreadable {}", property);
    return false;
  }
  return true;
}

void ReadArt(const CVariant& value, std::map<std::string, std::vector<GameScrapeArt>>& art)
{
  if (!value.isObject())
    return;

  for (auto it = value.begin_map(); it != value.end_map(); ++it)
  {
    const std::string& type = it->first;
    if (type.empty() || !it->second.isArray())
      continue;

    for (auto entry = it->second.begin_array(); entry != it->second.end_array(); ++entry)
    {
      if (!entry->isObject())
        continue;
      GameScrapeArt piece;
      piece.type = type;
      piece.url = (*entry)["url"].asString();
      piece.region = (*entry)["region"].asString();
      piece.width = static_cast<int>((*entry)["width"].asInteger());
      piece.height = static_cast<int>((*entry)["height"].asInteger());
      if (!piece.url.empty())
        art[type].emplace_back(std::move(piece));
    }
  }
}
} // namespace

CGameScraper::CGameScraper(std::shared_ptr<ADDON::CScraper> addon) : m_addon(std::move(addon))
{
}

CGameScraper::~CGameScraper() = default;

std::unique_ptr<CGameScraper> CGameScraper::Create(const std::string& addonId)
{
  if (addonId.empty())
    return nullptr;

  ADDON::AddonPtr addon;
  if (!CServiceBroker::GetAddonMgr().GetAddon(addonId, addon, ADDON::AddonType::SCRAPER_GAMES,
                                              ADDON::OnlyEnabled::CHOICE_YES))
    return nullptr;

  auto scraper = std::dynamic_pointer_cast<ADDON::CScraper>(addon);
  if (!scraper)
    return nullptr;

  return std::make_unique<CGameScraper>(std::move(scraper));
}

std::unique_ptr<CGameScraper> CGameScraper::CreateDefault()
{
  ADDON::VECADDONS addons;
  if (!CServiceBroker::GetAddonMgr().GetAddons(addons, ADDON::AddonType::SCRAPER_GAMES))
    return nullptr;

  for (const ADDON::AddonPtr& addon : addons)
  {
    if (auto scraper = std::dynamic_pointer_cast<ADDON::CScraper>(addon))
      return std::make_unique<CGameScraper>(std::move(scraper));
  }
  return nullptr;
}

std::string CGameScraper::ID() const
{
  return m_addon->ID();
}

std::string CGameScraper::Name() const
{
  return m_addon->Name();
}

void CGameScraper::SetPathSettings(const std::string& settingsXml)
{
  m_addon->SetPathSettings(ADDON::ContentType::GAMES, settingsXml);
}

std::string CGameScraper::BuildUrl(const std::string& action, const GameScrapeRequest& request) const
{
  CURL url("plugin://" + m_addon->ID() + "/");
  url.SetOption("action", action);
  url.SetOption("protocol", std::to_string(PROTOCOL_VERSION));

  if (!request.title.empty())
    url.SetOption("title", request.title);
  if (!request.fileName.empty())
    url.SetOption("filename", request.fileName);
  if (!request.platformSlug.empty())
    url.SetOption("platform", request.platformSlug);
  if (!request.platformIds.empty())
  {
    CVariant ids(CVariant::VariantTypeObject);
    for (const auto& [provider, id] : request.platformIds)
      ids[provider] = id;
    std::string json;
    if (CJSONVariantWriter::Write(ids, json, true))
      url.SetOption("platformids", json);
  }
  if (!request.crc32.empty())
    url.SetOption("crc32", request.crc32);
  if (!request.md5.empty())
    url.SetOption("md5", request.md5);
  if (!request.sha1.empty())
    url.SetOption("sha1", request.sha1);
  if (!request.serial.empty())
    url.SetOption("serial", request.serial);
  if (!request.raHash.empty())
    url.SetOption("rahash", request.raHash);
  if (request.size > 0)
    url.SetOption("size", std::to_string(request.size));
  if (!request.regions.empty())
    url.SetOption("regions", StringUtils::Join(request.regions, ","));
  if (!request.languages.empty())
    url.SetOption("languages", StringUtils::Join(request.languages, ","));
  if (request.year > 0)
    url.SetOption("year", std::to_string(request.year));

  const std::string settings = m_addon->GetPathSettingsAsJSON();
  url.SetOption("pathSettings", settings.empty() ? "{}" : settings);

  return url.Get();
}

std::vector<GameScrapeCandidate> CGameScraper::Find(const GameScrapeRequest& request)
{
  std::vector<GameScrapeCandidate> candidates;

  CFileItemList items;
  const std::string url = BuildUrl("find", request);
  if (!XFILE::CDirectory::GetDirectory(url, items, "", XFILE::DIR_FLAG_DEFAULTS))
  {
    CLog::Log(LOGDEBUG, "GAME: {} found nothing for {}", m_addon->ID(), request.fileName);
    return candidates;
  }

  for (const auto& item : items)
  {
    CVariant payload;
    if (!ParsePayload(*item, PROPERTY_CANDIDATE, payload))
      continue;

    GameScrapeCandidate candidate;
    candidate.id = payload["id"].asString();
    if (candidate.id.empty())
      candidate.id = item->GetPath();
    candidate.title = payload["title"].asString();
    if (candidate.title.empty())
      candidate.title = item->GetLabel();
    candidate.year = static_cast<int>(payload["year"].asInteger());
    candidate.score = static_cast<float>(payload["score"].asDouble());
    candidate.matchedBy = CGameLibraryTypes::MatchMethodFromString(payload["matchedby"].asString());
    if (candidate.matchedBy == MatchMethod::NONE)
      candidate.matchedBy = MatchMethod::NAME;

    candidate.subtitle = payload["subtitle"].asString();
    candidate.regions = Strings(payload["regions"]);
    if (candidate.regions.empty() && payload["regions"].isString())
      candidate.regions = StringUtils::Split(payload["regions"].asString(), ",");
    candidate.provider = payload["provider"].asString();
    candidate.thumb = payload["thumb"].asString();

    candidate.details = item->GetProperty(PROPERTY_DETAILS).asString();

    if (!candidate.id.empty())
      candidates.emplace_back(std::move(candidate));
  }

  std::ranges::stable_sort(candidates,
                           [](const auto& a, const auto& b) { return a.score > b.score; });
  return candidates;
}

bool CGameScraper::GetDetails(const std::string& id,
                              const GameScrapeRequest& request,
                              CGameInfoTag& details,
                              std::map<std::string, std::vector<GameScrapeArt>>& art)
{
  CURL url(BuildUrl("getdetails", request));
  url.SetOption("id", id);

  CFileItem result;
  if (!XFILE::CPluginDirectory::GetPluginResult(url.Get(), result, false))
    return false;

  return ReadDetails(id, result.GetProperty(PROPERTY_DETAILS).asString(), details, art);
}

bool CGameScraper::GetDetails(const GameScrapeCandidate& candidate,
                              const GameScrapeRequest& request,
                              CGameInfoTag& details,
                              std::map<std::string, std::vector<GameScrapeArt>>& art)
{
  if (!candidate.details.empty() && ReadDetails(candidate.id, candidate.details, details, art))
    return true;
  return GetDetails(candidate.id, request, details, art);
}

bool CGameScraper::ReadDetails(const std::string& id,
                               const std::string& json,
                               CGameInfoTag& details,
                               std::map<std::string, std::vector<GameScrapeArt>>& art)
{
  if (json.empty())
    return false;

  CVariant payload;
  if (!CJSONVariantParser::Parse(json, payload) || !payload.isObject())
  {
    CLog::Log(LOGERROR, "GAME: Scraper answered with unreadable details");
    return false;
  }

  if (payload["version"].asInteger() > PROTOCOL_VERSION)
  {
    CLog::Log(LOGWARNING, "GAME: {} speaks protocol {}, newer than {}; reading what is understood",
              m_addon->ID(), payload["version"].asInteger(), PROTOCOL_VERSION);
  }

  const std::string title = payload["title"].asString();
  if (title.empty())
    return false;

  details.SetTitle(title);
  if (payload.isMember("sorttitle"))
    details.SetSortTitle(payload["sorttitle"].asString());
  if (payload.isMember("originaltitle"))
    details.SetOriginalTitle(payload["originaltitle"].asString());
  if (payload.isMember("overview"))
    details.SetOverview(payload["overview"].asString());
  if (payload.isMember("releasedate"))
    details.SetReleaseDate(payload["releasedate"].asString());
  if (payload.isMember("year"))
    details.SetYear(static_cast<unsigned int>(std::max<int64_t>(payload["year"].asInteger(), 0)));
  if (payload.isMember("developers"))
    details.SetDevelopers(Strings(payload["developers"]));
  if (payload.isMember("publishers"))
    details.SetPublishers(Strings(payload["publishers"]));
  if (payload.isMember("genres"))
    details.SetGenres(Strings(payload["genres"]));
  if (payload.isMember("collections"))
    details.SetCollections(Strings(payload["collections"]));
  if (payload.isMember("tags"))
    details.SetTags(Strings(payload["tags"]));
  if (payload["players"].isObject())
  {
    details.SetPlayers(static_cast<int>(payload["players"]["min"].asInteger()),
                       static_cast<int>(payload["players"]["max"].asInteger()));
  }
  if (payload.isMember("coop"))
    details.SetCoop(payload["coop"].asBoolean());
  if (payload.isMember("category"))
    details.SetCategory(CGameLibraryTypes::GameCategoryFromString(payload["category"].asString()));
  if (payload.isMember("trailer"))
    details.SetTrailer(payload["trailer"].asString());
  if (payload.isMember("manual"))
    details.SetManual(payload["manual"].asString());

  if (payload["ratings"].isObject())
  {
    for (auto it = payload["ratings"].begin_map(); it != payload["ratings"].end_map(); ++it)
    {
      if (!it->second.isObject())
        continue;
      GameRating rating;
      rating.rating = static_cast<float>(it->second["rating"].asDouble());
      rating.max = static_cast<float>(it->second["max"].asDouble(10.0));
      rating.votes = static_cast<int>(it->second["votes"].asInteger());
      if (rating.max > 0.0f)
        details.SetRating(it->first, rating);
    }
  }

  if (payload["ageratings"].isArray())
  {
    std::vector<GameAgeRating> ratings;
    for (auto it = payload["ageratings"].begin_array(); it != payload["ageratings"].end_array(); ++it)
    {
      if (!it->isObject())
        continue;
      GameAgeRating rating;
      rating.board = (*it)["board"].asString();
      rating.value = (*it)["value"].asString();
      rating.descriptors = (*it)["descriptors"].asString();
      if (!rating.value.empty())
        ratings.emplace_back(std::move(rating));
    }
    details.SetAgeRatings(ratings);
  }

  if (payload["uniqueids"].isObject())
  {
    bool first = true;
    for (auto it = payload["uniqueids"].begin_map(); it != payload["uniqueids"].end_map(); ++it)
    {
      details.SetUniqueID(it->first, it->second.asString(), first);
      first = false;
    }
  }
  // The scraper's own id is always kept, so the game can be refreshed
  details.SetUniqueID(m_addon->ID(), id, details.GetDefaultUniqueIDType().empty());

  if (payload["releases"].isArray())
  {
    std::vector<GameRelease> releases;
    for (auto it = payload["releases"].begin_array(); it != payload["releases"].end_array(); ++it)
    {
      if (!it->isObject())
        continue;
      GameRelease release;
      release.title = (*it)["title"].asString();
      release.regions = Strings((*it)["regions"]);
      release.languages = Strings((*it)["languages"]);
      release.revision = (*it)["revision"].asString();
      release.status = CGameLibraryTypes::ReleaseStatusFromString((*it)["status"].asString());
      release.licence = CGameLibraryTypes::LicenceFromString((*it)["licence"].asString());
      release.serial = (*it)["serial"].asString();
      release.releaseDate = (*it)["releasedate"].asString();
      GameFile file;
      file.crc32 = (*it)["crc32"].asString();
      file.md5 = (*it)["md5"].asString();
      file.sha1 = (*it)["sha1"].asString();
      file.size = static_cast<uint64_t>(std::max<int64_t>((*it)["size"].asInteger(), 0));
      if (!file.crc32.empty() || !file.md5.empty() || !file.sha1.empty())
        release.files.emplace_back(std::move(file));
      releases.emplace_back(std::move(release));
    }
    details.SetReleases(releases);
  }

  ReadArt(payload["art"], art);

  details.SetLoaded(true);
  return true;
}

bool CGameScraper::GetPlatform(const GameScrapeRequest& request,
                               PlatformInfo& platform,
                               std::map<std::string, std::vector<GameScrapeArt>>& art)
{
  CFileItem result;
  if (!XFILE::CPluginDirectory::GetPluginResult(BuildUrl("getplatform", request), result, false))
    return false;

  CVariant payload;
  if (!ParsePayload(result, PROPERTY_PLATFORM, payload))
    return false;

  if (payload.isMember("name") && !payload["name"].asString().empty())
    platform.name = payload["name"].asString();
  if (payload.isMember("manufacturer"))
    platform.manufacturer = payload["manufacturer"].asString();
  if (payload.isMember("released"))
    platform.released = static_cast<int>(payload["released"].asInteger());
  if (payload.isMember("discontinued"))
    platform.discontinued = static_cast<int>(payload["discontinued"].asInteger());
  if (payload.isMember("overview"))
    platform.overview = payload["overview"].asString();

  ReadArt(payload["art"], art);
  return true;
}
