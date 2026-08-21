/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "games/GameManual.h"

#include <gtest/gtest.h>

using namespace KODI::GAME;

// The path is derived without touching the filesystem, so these cases can be
// stated directly. Whether the file is actually there is a separate question,
// answered by GetManualPath().

TEST(TestGameManual, ReplacesTheGameExtension)
{
  EXPECT_EQ(CGameManual::BuildManualPath("/roms/Game.rom"), "/roms/Game.pdf");
}

TEST(TestGameManual, KeepsRegionAndRevisionTags)
{
  // The tags are part of the name the player chose, and two regions of the
  // same game are different games as far as a manual is concerned
  EXPECT_EQ(CGameManual::BuildManualPath("/roms/Sonic The Hedgehog (USA).md"),
            "/roms/Sonic The Hedgehog (USA).pdf");
  EXPECT_EQ(CGameManual::BuildManualPath("/roms/Zelda (Europe) (Rev A) [!].nes"),
            "/roms/Zelda (Europe) (Rev A) [!].pdf");
}

TEST(TestGameManual, KeepsSpacesAndPunctuation)
{
  EXPECT_EQ(CGameManual::BuildManualPath("/roms/Mega Man X2 - The Sequel!.sfc"),
            "/roms/Mega Man X2 - The Sequel!.pdf");
}

TEST(TestGameManual, KeepsDotsInsideTheName)
{
  // Only the final extension is replaced
  EXPECT_EQ(CGameManual::BuildManualPath("/roms/Sonic 3.0 (USA).md"), "/roms/Sonic 3.0 (USA).pdf");
}

TEST(TestGameManual, WorksOnNetworkPaths)
{
  // The manual sits beside the game wherever the game lives
  EXPECT_EQ(CGameManual::BuildManualPath("smb://nas/roms/Game.rom"), "smb://nas/roms/Game.pdf");
  EXPECT_EQ(CGameManual::BuildManualPath("nfs://10.0.0.1/roms/Game.rom"),
            "nfs://10.0.0.1/roms/Game.pdf");
}

TEST(TestGameManual, RefusesAGameWithNoExtension)
{
  // Appending would invent a basename the player never chose
  EXPECT_TRUE(CGameManual::BuildManualPath("/roms/Game").empty());
}

TEST(TestGameManual, RefusesAGameInsideAnArchive)
{
  // There is no directory alongside the game to hold a manual
  EXPECT_TRUE(CGameManual::BuildManualPath("zip://%2froms%2fgames.zip/Game.rom").empty());
}

TEST(TestGameManual, RefusesAnEmptyPath)
{
  EXPECT_TRUE(CGameManual::BuildManualPath("").empty());
}

TEST(TestGameManual, DerivesAPathThatDiffersFromAMismatchedManual)
{
  // The cases the design calls out as needing to fail. They fail because the
  // derived path simply isn't the mismatched one - no comparison logic is
  // involved, which is what keeps the wrong manual from ever being opened.
  const std::string derived = CGameManual::BuildManualPath("/roms/Sonic The Hedgehog (USA).md");

  EXPECT_NE(derived, "/roms/Sonic The Hedgehog.pdf");
  EXPECT_NE(derived, "/roms/Sonic The Hedgehog (Europe).pdf");
  EXPECT_NE(derived, "/roms/Other.pdf");
  EXPECT_NE(derived, "/roms/Sonic The Hedgehog (USA) Manual.pdf");
}
