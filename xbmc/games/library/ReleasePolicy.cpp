/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "ReleasePolicy.h"

#include "utils/StringUtils.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <string_view>

using namespace KODI;
using namespace GAME;

namespace
{
//! The regions tried after the player's own, in order: Europe, USA, Japan
constexpr std::array<std::string_view, 3> FALLBACK_REGIONS{"150", "US", "JP"};

//! No-Intro's market for a release sold in Japan, the USA and Europe alike
const GameRegion WORLD = GameRegion::Make({}, "World");

//! The wider region whose releases a country gets
std::string_view WiderRegion(const GameRegion& region)
{
  const std::string code = region.ToString();
  if (code == "GB" || code == "DE" || code == "FR" || code == "ES" || code == "IT" || code == "AU")
    return "150";
  if (code == "CA" || code == "BR")
    return "US";
  if (code == "KR" || code == "CN" || code == "TW" || code == "HK" || code == "IN")
    return "142";
  return "";
}
} // namespace

CReleasePolicy::CReleasePolicy(std::vector<GameRegion> regionPriority,
                               bool preferRetail,
                               bool preferNewestRevision,
                               bool preferVerified)
  : m_regionPriority(std::move(regionPriority)),
    m_preferRetail(preferRetail),
    m_preferNewestRevision(preferNewestRevision),
    m_preferVerified(preferVerified)
{
}

std::vector<GameRegion> CReleasePolicy::RegionPriority(const GameRegion& region)
{
  std::vector<GameRegion> regions;
  const auto add = [&regions](const GameRegion& added)
  {
    if (!added.IsEmpty() && std::ranges::find(regions, added) == regions.end())
      regions.emplace_back(added);
  };
  add(region);
  add(GameRegion::FromString(WiderRegion(region)));
  add(WORLD);
  for (std::string_view fallback : FALLBACK_REGIONS)
    add(GameRegion::FromString(fallback));
  return regions;
}

int CReleasePolicy::RegionRank(const GameRelease& release) const
{
  // The best-placed region a release covers; a release with no region sits
  // behind every listed one, and an unlisted region behind that
  int best = static_cast<int>(m_regionPriority.size()) + 1;
  for (const GameRegion& region : release.regions)
  {
    const auto it = std::ranges::find(m_regionPriority, region);
    if (it != m_regionPriority.end())
      best = std::min(best, static_cast<int>(it - m_regionPriority.begin()));
    else if (region == WORLD)
      best = std::min(best, static_cast<int>(m_regionPriority.size()));
  }
  if (release.regions.empty())
    best = static_cast<int>(m_regionPriority.size());
  return best;
}

int CReleasePolicy::StatusRank(const GameRelease& release)
{
  int rank = 0;
  if (release.status != ReleaseStatus::RETAIL || !release.edition.empty())
    rank += 2;
  if (release.licence == Licence::PIRATE)
    rank += 4;
  if (release.alternate)
    rank += 1;
  return rank;
}

std::vector<int> CReleasePolicy::RevisionRank(const GameRelease& release)
{
  // Compared part by part: "v1.10" is newer than "v1.9" and older than "v2.0".
  // A letter counts by its place in the alphabet: "Rev B" is newer than "Rev A".
  std::string_view rev = release.revision;
  std::vector<int> parts;
  if (rev.empty())
    return parts;

  for (std::string_view word : {"revision", "version", "rev", "v"})
  {
    if (StringUtils::StartsWithNoCase(rev, word))
    {
      rev.remove_prefix(word.size());
      break;
    }
  }

  bool inNumber = false;
  for (const char c : rev)
  {
    const auto u = static_cast<unsigned char>(c);
    if (std::isdigit(u))
    {
      if (!inNumber)
        parts.push_back(0);
      if (parts.back() < 100000000)
        parts.back() = parts.back() * 10 + (c - '0');
      inNumber = true;
    }
    else
    {
      inNumber = false;
      if (std::isalpha(u) && parts.empty())
        parts.push_back(std::tolower(u) - 'a' + 1);
    }
  }
  return parts;
}

int CReleasePolicy::DumpRank(const GameRelease& release)
{
  switch (release.dump)
  {
    case DumpStatus::VERIFIED:
      return 0;
    case DumpStatus::UNKNOWN:
    case DumpStatus::GOOD:
      return 1;
    case DumpStatus::BAD:
      return 3;
  }
  return 1;
}

bool CReleasePolicy::Prefers(const GameRelease& a, const GameRelease& b) const
{
  // A bad dump never plays when another exists
  if (DumpRank(a) != DumpRank(b) && (a.dump == DumpStatus::BAD || b.dump == DumpStatus::BAD))
    return DumpRank(a) < DumpRank(b);

  if (m_preferRetail && StatusRank(a) != StatusRank(b))
    return StatusRank(a) < StatusRank(b);

  if (RegionRank(a) != RegionRank(b))
    return RegionRank(a) < RegionRank(b);

  if (m_preferNewestRevision && RevisionRank(a) != RevisionRank(b))
    return RevisionRank(a) > RevisionRank(b);

  if (m_preferVerified && DumpRank(a) != DumpRank(b))
    return DumpRank(a) < DumpRank(b);

  return false;
}

int CReleasePolicy::PickDefault(const std::vector<GameRelease>& releases) const
{
  int best = -1;
  for (size_t i = 0; i < releases.size(); ++i)
  {
    if (releases[i].files.empty())
      continue;
    if (best < 0 || Prefers(releases[i], releases[static_cast<size_t>(best)]))
      best = static_cast<int>(i);
  }
  return best;
}
