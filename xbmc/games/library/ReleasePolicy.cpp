/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "ReleasePolicy.h"

#include "ServiceBroker.h"
#include "settings/Settings.h"
#include "settings/SettingsComponent.h"
#include "utils/StringUtils.h"

#include <algorithm>
#include <cctype>

using namespace KODI;
using namespace GAME;

namespace
{
std::vector<std::string> SplitRegions(const std::string& list)
{
  std::vector<std::string> regions;
  for (std::string code : StringUtils::Split(list, ','))
  {
    StringUtils::Trim(code);
    StringUtils::ToLower(code);
    if (!code.empty() && std::ranges::find(regions, code) == regions.end())
      regions.emplace_back(std::move(code));
  }
  return regions;
}
} // namespace

CReleasePolicy::CReleasePolicy()
{
  const auto settings = CServiceBroker::GetSettingsComponent()->GetSettings();
  if (settings)
  {
    m_regionPriority = SplitRegions(settings->GetString(SETTING_GAMELIBRARY_REGIONPRIORITY));
    m_preferRetail = settings->GetBool(SETTING_GAMELIBRARY_PREFERRETAIL);
    m_preferNewestRevision = settings->GetBool(SETTING_GAMELIBRARY_PREFERNEWESTREVISION);
    m_preferVerified = settings->GetBool(SETTING_GAMELIBRARY_PREFERVERIFIED);
  }
  if (m_regionPriority.empty())
    m_regionPriority = SplitRegions(SETTING_GAMELIBRARY_REGIONPRIORITY_DEFAULT);
}

CReleasePolicy::CReleasePolicy(std::vector<std::string> regionPriority,
                               bool preferRetail,
                               bool preferNewestRevision,
                               bool preferVerified)
  : m_regionPriority(std::move(regionPriority)),
    m_preferRetail(preferRetail),
    m_preferNewestRevision(preferNewestRevision),
    m_preferVerified(preferVerified)
{
}

int CReleasePolicy::RegionRank(const GameRelease& release) const
{
  // The best-placed region a release covers; a release with no region sits
  // behind every listed one, and an unlisted region behind that
  int best = static_cast<int>(m_regionPriority.size()) + 1;
  for (const std::string& code : release.regions)
  {
    const auto it = std::ranges::find(m_regionPriority, code);
    if (it != m_regionPriority.end())
      best = std::min(best, static_cast<int>(it - m_regionPriority.begin()));
    else if (code == "wor")
      best = std::min(best, static_cast<int>(m_regionPriority.size()));
  }
  if (release.regions.empty())
    best = static_cast<int>(m_regionPriority.size());
  return best;
}

int CReleasePolicy::StatusRank(const GameRelease& release)
{
  int rank = 0;
  if (release.status != ReleaseStatus::RETAIL)
    rank += 2;
  if (release.licence == Licence::PIRATE)
    rank += 4;
  if (release.alternate)
    rank += 1;
  return rank;
}

int CReleasePolicy::RevisionRank(const GameRelease& release)
{
  // "Rev 2" > "Rev 1" > none; "v1.1" > "v1.0"; a letter counts as its position
  const std::string& rev = release.revision;
  if (rev.empty())
    return 0;
  int value = 0;
  bool seenDigit = false;
  for (const char c : rev)
  {
    if (std::isdigit(static_cast<unsigned char>(c)))
    {
      value = value * 10 + (c - '0');
      seenDigit = true;
    }
    else if (std::isalpha(static_cast<unsigned char>(c)) && !seenDigit &&
             std::string_view("rev").find(static_cast<char>(std::tolower(c))) == std::string_view::npos &&
             std::string_view("version").find(static_cast<char>(std::tolower(c))) == std::string_view::npos)
    {
      value = std::tolower(static_cast<unsigned char>(c)) - 'a' + 1;
    }
  }
  return value + 1;
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
