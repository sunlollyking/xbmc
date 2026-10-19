/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "games/database/GameDatabase.h"
#include "imagefiles/ImageFileURL.h"

#include <gtest/gtest.h>

using namespace KODI::GAME;

TEST(TestGameArt, SpinesAreAskedForUpright)
{
  KODI::ART::Artwork art{{"boxspine", "https://example.com/spine.png"},
                         {"boxspine1", "special://profile/spine-jp.png"},
                         {"boxfront", "https://example.com/front.png"}};

  CGameDatabase::StandSpinesUpright(art);

  for (const char* type : {"boxspine", "boxspine1"})
  {
    const IMAGE_FILES::CImageFileURL url{art[type]};
    EXPECT_EQ(url.GetOption("orientation"), "portrait") << type;
  }
  EXPECT_EQ(art["boxfront"], "https://example.com/front.png");

  // Asking twice doesn't wrap a spine twice
  const std::string once = art["boxspine"];
  CGameDatabase::StandSpinesUpright(art);
  EXPECT_EQ(art["boxspine"], once);
}
