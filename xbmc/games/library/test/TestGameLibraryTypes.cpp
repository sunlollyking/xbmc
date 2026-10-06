/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "games/library/GameLibraryTypes.h"

#include <gtest/gtest.h>

using namespace KODI;
using namespace GAME;

TEST(TestGameLibraryTypes, TitleKeyIgnoresArticlesAndPunctuation)
{
  EXPECT_EQ(CGameLibraryTypes::TitleKey("The Legend of Zelda"),
            CGameLibraryTypes::TitleKey("Legend of Zelda, The"));
  EXPECT_EQ(CGameLibraryTypes::TitleKey("AD&D Curse of the Azure Bonds"),
            CGameLibraryTypes::TitleKey("AD&D Curse Of The Azure Bonds"));
}

TEST(TestGameLibraryTypes, TitleKeyKeepsLettersOutsideAscii)
{
  EXPECT_NE(CGameLibraryTypes::TitleKey("ランス3"), CGameLibraryTypes::TitleKey("3つの願い"));
  EXPECT_NE(CGameLibraryTypes::TitleKey("BURAI 上巻"), CGameLibraryTypes::TitleKey("BURAI 下巻"));
  EXPECT_NE(CGameLibraryTypes::TitleKey("CAL"), CGameLibraryTypes::TitleKey("CAL外伝"));
}
