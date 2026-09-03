/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "GameDbUrl.h"

#include "URL.h"
#include "utils/StringUtils.h"
#include "utils/Variant.h"

#include <array>
#include <utility>

using namespace KODI;
using namespace GAME;

namespace
{
constexpr std::array<std::pair<GameDbNode, std::string_view>, 14> nodeSegments{{
    {GameDbNode::PLATFORMS, "platforms"},
    {GameDbNode::GAMES, "titles"},
    {GameDbNode::RELEASES, "releases"},
    {GameDbNode::GENRES, "genres"},
    {GameDbNode::YEARS, "years"},
    {GameDbNode::DEVELOPERS, "developers"},
    {GameDbNode::PUBLISHERS, "publishers"},
    {GameDbNode::COLLECTIONS, "collections"},
    {GameDbNode::TAGS, "tags"},
    {GameDbNode::REGIONS, "regions"},
    {GameDbNode::PLAYERS, "players"},
    {GameDbNode::AGERATINGS, "ageratings"},
    {GameDbNode::CATEGORIES, "categories"},
    {GameDbNode::OVERVIEW, ""},
}};

// A facet's segment, and the option its chosen value becomes
constexpr std::array<std::pair<std::string_view, std::string_view>, 10> facetOptions{{
    {"genres", "genreid"},
    {"years", "year"},
    {"developers", "developerid"},
    {"publishers", "publisherid"},
    {"collections", "collectionid"},
    {"tags", "tagid"},
    {"regions", "region"},
    {"players", "players"},
    {"ageratings", "agerating"},
    {"categories", "category"},
}};

const std::vector<std::string_view> facetNames{
    "genres",  "years",   "developers", "publishers", "collections",
    "tags",    "regions", "players",    "ageratings", "categories",
};

const std::vector<std::string_view> listNames{
    "recentlyadded", "recentlyplayed", "neverplayed", "favourites",     "completed",
    "multiplayer",   "coop",           "achievements", "inprogress",    "hacks",
    "homebrew",      "needsattention",
};

bool IsList(std::string_view segment)
{
  for (std::string_view name : listNames)
  {
    if (name == segment)
      return true;
  }
  return false;
}

std::string_view FacetOption(std::string_view segment)
{
  for (const auto& [facet, option] : facetOptions)
  {
    if (facet == segment)
      return option;
  }
  return "";
}
} // namespace

CGameDbUrl::CGameDbUrl() : CDbUrl()
{
}

CGameDbUrl::~CGameDbUrl() = default;

const std::vector<std::string_view>& CGameDbUrl::GetFacetNames()
{
  return facetNames;
}

const std::vector<std::string_view>& CGameDbUrl::GetListNames()
{
  return listNames;
}

GameDbNode CGameDbUrl::NodeFromSegment(std::string_view segment)
{
  for (const auto& [node, name] : nodeSegments)
  {
    if (name == segment)
      return node;
  }
  return GameDbNode::NONE;
}

std::string_view CGameDbUrl::SegmentFromNode(GameDbNode node)
{
  for (const auto& [n, name] : nodeSegments)
  {
    if (n == node)
      return name;
  }
  return "";
}

bool CGameDbUrl::parse()
{
  if (!m_url.IsProtocol("gamedb"))
    return false;

  m_type = "games";
  m_node = GameDbNode::OVERVIEW;
  m_itemType = "overview";
  m_list.clear();

  // Whatever the path carries as a query, a smart playlist among it
  AddOptions(m_url.GetOptions());

  // The first segment is what CURL would take for a host name
  std::string path = m_url.Get();
  path.erase(0, std::string_view("gamedb://").size());
  const size_t query = path.find('?');
  if (query != std::string::npos)
    path.erase(query);
  std::vector<std::string> segments = StringUtils::Split(path, '/');
  std::erase_if(segments, [](const std::string& s) { return s.empty(); });

  size_t i = 0;

  // An optional platform scope, then the same tree as the root
  if (i < segments.size() && segments[i] == "platforms")
  {
    m_node = GameDbNode::PLATFORMS;
    m_itemType = "platforms";
    ++i;
    if (i < segments.size())
    {
      if (!StringUtils::IsNaturalNumber(segments[i]))
        return false;
      AddOption("platformid", std::stoi(segments[i]));
      m_node = GameDbNode::OVERVIEW;
      m_itemType = "overview";
      ++i;
    }
  }

  if (i >= segments.size())
    return true;

  const std::string& segment = segments[i];

  if (segment == "titles")
  {
    m_node = GameDbNode::GAMES;
    m_itemType = "games";
    ++i;
    if (i < segments.size())
    {
      if (!StringUtils::IsNaturalNumber(segments[i]))
        return false;
      AddOption("gameid", std::stoi(segments[i]));
      m_node = GameDbNode::RELEASES;
      m_itemType = "releases";
      ++i;
    }
  }
  else if (IsList(segment))
  {
    m_node = GameDbNode::GAMES;
    m_itemType = "games";
    m_list = segment;
    ++i;
  }
  else if (const std::string_view option = FacetOption(segment); !option.empty())
  {
    m_node = NodeFromSegment(segment);
    m_itemType = segment;
    ++i;
    if (i < segments.size())
    {
      const std::string& value = segments[i];
      if (option == "year" || option == "players" || StringUtils::EndsWith(option, "id"))
      {
        if (!StringUtils::IsNaturalNumber(value))
          return false;
        AddOption(std::string(option), std::stoi(value));
      }
      else
      {
        AddOption(std::string(option), CURL::Decode(value));
      }
      m_node = GameDbNode::GAMES;
      m_itemType = "games";
      ++i;
    }
  }
  else
  {
    return false;
  }

  // Nothing may follow a leaf
  return i >= segments.size();
}

bool CGameDbUrl::validateOption(const std::string& key, const CVariant& value)
{
  return CDbUrl::validateOption(key, value);
}
