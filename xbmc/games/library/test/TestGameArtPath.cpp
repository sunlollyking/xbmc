/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "games/library/GameInfoScanner.h"

#include <gtest/gtest.h>

using namespace KODI;
using namespace GAME;

TEST(TestGameArtPath, FilesAPictureByPlatformKindAndName)
{
  EXPECT_EQ(CGameInfoScanner::ArtPath("/art/", "snes", "boxfront", "/games/SNES/",
                                      "/games/SNES/Super Metroid (Japan, USA).sfc"),
            "/art/snes/boxfront/Super Metroid (Japan, USA)");
}

TEST(TestGameArtPath, KeepsTheFoldersWithinTheSource)
{
  // Two of these games share a file name; their folders keep them apart
  EXPECT_EQ(CGameInfoScanner::ArtPath("/art/", "atari800", "fanart", "/games/Atari 800/",
                                      "/games/Atari 800/Cassettes/Chopperoid.cas"),
            "/art/atari800/fanart/Cassettes/Chopperoid");
  EXPECT_EQ(CGameInfoScanner::ArtPath("/art/", "atari800", "fanart", "/games/Atari 800/",
                                      "/games/Atari 800/Disks/Chopperoid.atr"),
            "/art/atari800/fanart/Disks/Chopperoid");
}

TEST(TestGameArtPath, NamesAGameOutsideItsSourceByFileAlone)
{
  EXPECT_EQ(CGameInfoScanner::ArtPath("/art/", "nes", "clearlogo", "",
                                      "/elsewhere/Deep/Kirby's Adventure (USA).nes"),
            "/art/nes/clearlogo/Kirby's Adventure (USA)");
}
