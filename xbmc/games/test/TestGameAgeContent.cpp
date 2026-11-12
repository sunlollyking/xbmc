/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "FileItem.h"
#include "games/database/GameDatabase.h"
#include "games/tags/GameInfoTag.h"

#include <gtest/gtest.h>

using namespace KODI::GAME;

TEST(TestGameAgeContent, TheFirstBoardsReasonsAreShown)
{
  CGameInfoTag game;
  game.SetAgeRatings({{"PEGI", "12", "Violence, Bad Language"}, {"ESRB", "T", "Blood"}});

  CFileItem item;
  CGameDatabase::SetAgeContentProperties(item, game);
  EXPECT_EQ(item.GetProperty("ageratingcontent").asString(), "Violence, Bad Language");
}

TEST(TestGameAgeContent, AnotherBoardsReasonsAreNamed)
{
  CGameInfoTag game;
  game.SetAgeRatings({{"PEGI", "12", ""}, {"ESRB", "T", "Blood"}});

  CFileItem item;
  CGameDatabase::SetAgeContentProperties(item, game);
  EXPECT_EQ(item.GetProperty("ageratingcontent").asString(), "ESRB: Blood");
}

TEST(TestGameAgeContent, NoReasonsShowNothing)
{
  CGameInfoTag game;
  game.SetAgeRatings({{"PEGI", "3", ""}});

  CFileItem item;
  item.SetProperty("ageratingcontent", "stale");
  CGameDatabase::SetAgeContentProperties(item, game);
  EXPECT_FALSE(item.HasProperty("ageratingcontent"));
}
