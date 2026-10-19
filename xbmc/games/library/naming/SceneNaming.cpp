/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "SceneNaming.h"

#include "utils/RegExp.h"

#include <array>
#include <string>
#include <utility>

using namespace KODI;
using namespace GAME;

namespace
{
// The territory each code is, as BCP 47 codes it. KS is South Korea, and EU
// UN M.49's Europe rather than BCP 47's European Union.
constexpr auto territories = std::to_array<std::pair<std::string_view, std::string_view>>({
    {"AU", "AU"},
    {"DE", "DE"},
    {"ES", "ES"},
    {"EU", "150"},
    {"FR", "FR"},
    {"IT", "IT"},
    {"JP", "JP"},
    {"KS", "KR"},
    {"NL", "NL"},
    {"US", "US"},
});
} // namespace

size_t CSceneNaming::ReleaseNumberLength(std::string_view name)
{
  // CRegExp keeps the last match, so each thread needs its own
  thread_local CRegExp releaseNumber = []
  {
    CRegExp re;
    re.RegComp("^[0-9]{4} +(?=.*\\((?:JP|EU|US|FR|KS|DE|IT|ES|NL|AU)\\))(?!.*\\([0-9]{4}\\))");
    return re;
  }();

  if (releaseNumber.RegFind(std::string(name)) < 0)
    return 0;
  return static_cast<size_t>(releaseNumber.GetFindLen());
}

std::optional<GameRegion> CSceneNaming::Region(std::string_view code)
{
  for (const auto& [c, territory] : territories)
  {
    if (c == code)
      return GameRegion::Make(territory);
  }
  return std::nullopt;
}
