/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "GameVideoSettings.h"

#include "cores/RetroPlayer/RetroPlayerUtils.h"
#include "games/library/GameLibraryTypes.h"
#include "settings/GameSettings.h"

using namespace KODI;
using namespace GAME;

namespace
{
template<typename T>
void InheritValue(std::optional<T>& value, const std::optional<T>& fallback)
{
  if (!value)
    value = fallback;
}

template<typename T>
std::optional<T> DifferentValue(const std::optional<T>& value, const std::optional<T>& baseline)
{
  return value != baseline ? value : std::nullopt;
}
} // namespace

bool GameVideoSettings::IsEmpty() const
{
  return !videoFilter && !stretchMode && !rotationDegCCW && !bezelEnabled;
}

void GameVideoSettings::Inherit(const GameVideoSettings& fallback)
{
  InheritValue(videoFilter, fallback.videoFilter);
  InheritValue(stretchMode, fallback.stretchMode);
  InheritValue(rotationDegCCW, fallback.rotationDegCCW);
  InheritValue(bezelEnabled, fallback.bezelEnabled);
}

GameVideoSettings GameVideoSettings::Difference(const GameVideoSettings& baseline) const
{
  GameVideoSettings difference;
  difference.videoFilter = DifferentValue(videoFilter, baseline.videoFilter);
  difference.stretchMode = DifferentValue(stretchMode, baseline.stretchMode);
  difference.rotationDegCCW = DifferentValue(rotationDegCCW, baseline.rotationDegCCW);
  difference.bezelEnabled = DifferentValue(bezelEnabled, baseline.bezelEnabled);
  return difference;
}

void GameVideoSettings::ApplyTo(::CGameSettings& settings) const
{
  if (videoFilter)
    settings.SetVideoFilter(*videoFilter);
  if (stretchMode)
    settings.SetStretchMode(RETRO::CRetroPlayerUtils::IdentifierToStretchMode(*stretchMode));
  if (rotationDegCCW)
    settings.SetRotationDegCCW(*rotationDegCCW);
  if (bezelEnabled)
    settings.SetBezelEnabled(*bezelEnabled);
}

GameVideoSettings GameVideoSettings::FromGameSettings(const ::CGameSettings& settings)
{
  GameVideoSettings videoSettings;
  videoSettings.videoFilter = settings.VideoFilter();
  videoSettings.stretchMode =
      RETRO::CRetroPlayerUtils::StretchModeToIdentifier(settings.StretchMode());
  videoSettings.rotationDegCCW = settings.RotationDegCCW();
  videoSettings.bezelEnabled = settings.BezelEnabled();
  return videoSettings;
}

GameVideoSettings GameVideoSettings::FromPlatform(const PlatformInfo& platform)
{
  GameVideoSettings videoSettings;
  // A platform with no filter has none to give, rather than giving "none"
  if (!platform.defaultVideoFilter.empty())
    videoSettings.videoFilter = platform.defaultVideoFilter;
  if (!platform.defaultStretchMode.empty())
    videoSettings.stretchMode = platform.defaultStretchMode;
  videoSettings.rotationDegCCW = platform.defaultRotationDegCCW;
  videoSettings.bezelEnabled = platform.defaultBezelEnabled;
  return videoSettings;
}
