/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "games/database/GameDatabase.h"
#include "games/library/GameLibraryTypes.h"

#include <vector>

#include <gtest/gtest.h>

using namespace KODI::GAME;

TEST(TestGameLanguages, KeptLanguagesReadBack)
{
  const std::vector<GameLanguage> languages{{"English", true, true, true}, {"Japanese", true}};
  EXPECT_EQ(CGameDatabase::DeserializeLanguages(CGameDatabase::SerializeLanguages(languages)),
            languages);
  EXPECT_TRUE(CGameDatabase::DeserializeLanguages("").empty());
}
