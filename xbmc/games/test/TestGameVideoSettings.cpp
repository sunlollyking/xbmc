/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "cores/GameSettings.h"
#include "games/GameVideoSettings.h"
#include "games/library/GameLibraryTypes.h"
#include "settings/GameSettings.h"

#include <gtest/gtest.h>

using namespace KODI;
using namespace GAME;

namespace
{
GameVideoSettings Inherited()
{
  GameVideoSettings settings;
  settings.videoFilter = "crt.slangp";
  settings.stretchMode = RETRO::STRETCHMODE_NORMAL_ID;
  settings.rotationDegCCW = 0;
  settings.bezelEnabled = true;
  return settings;
}
} // namespace

TEST(TestGameVideoSettings, InheritingFillsOnlyWhatIsUnset)
{
  GameVideoSettings settings;
  settings.rotationDegCCW = 90;
  settings.Inherit(Inherited());

  EXPECT_EQ(settings.rotationDegCCW, 90u);
  EXPECT_EQ(settings.videoFilter, "crt.slangp");
  EXPECT_EQ(settings.stretchMode, RETRO::STRETCHMODE_NORMAL_ID);
  EXPECT_EQ(settings.bezelEnabled, true);
}

TEST(TestGameVideoSettings, NoFilterIsASettingOfItsOwn)
{
  GameVideoSettings settings;
  settings.videoFilter = "";
  settings.Inherit(Inherited());

  EXPECT_EQ(settings.videoFilter, "");
}

TEST(TestGameVideoSettings, AGameThatWasNotChangedKeepsNothing)
{
  EXPECT_TRUE(Inherited().Difference(Inherited()).IsEmpty());
}

TEST(TestGameVideoSettings, AGameKeepsOnlyWhatWasChanged)
{
  GameVideoSettings played = Inherited();
  played.rotationDegCCW = 270;
  played.bezelEnabled = false;

  const GameVideoSettings kept = played.Difference(Inherited());

  EXPECT_FALSE(kept.videoFilter);
  EXPECT_FALSE(kept.stretchMode);
  EXPECT_EQ(kept.rotationDegCCW, 270u);
  EXPECT_EQ(kept.bezelEnabled, false);
}

TEST(TestGameVideoSettings, AChangeBackToWhatIsInheritedIsForgotten)
{
  GameVideoSettings stored;
  stored.stretchMode = RETRO::STRETCHMODE_STRETCH_4_3_ID;

  GameVideoSettings played = stored;
  played.Inherit(Inherited());
  played.stretchMode = RETRO::STRETCHMODE_NORMAL_ID;

  EXPECT_TRUE(played.Difference(Inherited()).IsEmpty());
}

TEST(TestGameVideoSettings, APlatformGivesOnlyWhatItHas)
{
  PlatformInfo platform;
  platform.defaultRotationDegCCW = 90;

  const GameVideoSettings settings = GameVideoSettings::FromPlatform(platform);

  EXPECT_FALSE(settings.videoFilter);
  EXPECT_FALSE(settings.stretchMode);
  EXPECT_EQ(settings.rotationDegCCW, 90u);
  EXPECT_FALSE(settings.bezelEnabled);
}

TEST(TestGameVideoSettings, GameSettingsRoundTrip)
{
  GameVideoSettings settings = Inherited();
  settings.stretchMode = RETRO::STRETCHMODE_STRETCH_4_3_ID;
  settings.rotationDegCCW = 180;
  settings.bezelEnabled = false;

  ::CGameSettings gameSettings;
  settings.ApplyTo(gameSettings);

  EXPECT_EQ(GameVideoSettings::FromGameSettings(gameSettings), settings);
}
