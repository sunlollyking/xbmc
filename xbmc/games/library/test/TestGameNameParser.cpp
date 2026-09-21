/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "games/library/GameNameParser.h"

#include <string>

#include <gtest/gtest.h>

using namespace KODI;
using namespace GAME;

TEST(TestGameNameParser, SeparatesTheWordsOfAWhdLoadName)
{
  EXPECT_EQ(CGameNameParser::ParseWhdLoadName("SecretOfMonkeyIsland_v3.4_1625"),
            "Secret Of Monkey Island");
  EXPECT_EQ(CGameNameParser::ParseWhdLoadName("StuntCarRacerTNT_v1.3"), "Stunt Car Racer TNT");
  EXPECT_EQ(CGameNameParser::ParseWhdLoadName("SabreTeam_v1.1.lha"), "Sabre Team");
}

TEST(TestGameNameParser, KeepsAnAcronymWhole)
{
  // The split goes before the last capital of a run, not through the middle of it
  EXPECT_EQ(CGameNameParser::ParseWhdLoadName("UFOEnemyUnknown_v1.0_AGA_0157"), "UFO Enemy Unknown");
}

TEST(TestGameNameParser, SeparatesADigitFromTheWordBeforeIt)
{
  EXPECT_EQ(CGameNameParser::ParseWhdLoadName("Turrican3_v1.4_2633"), "Turrican 3");
  EXPECT_EQ(CGameNameParser::ParseWhdLoadName("AbandonedPlaces2_v2.0"), "Abandoned Places 2");
  EXPECT_EQ(CGameNameParser::ParseWhdLoadName("A10TankKiller_v2.0_3Disk"), "A10 Tank Killer");
}

TEST(TestGameNameParser, DropsATagThatNeverCloses)
{
  // Extracted sets routinely hit a 64 character filename limit, which cuts a
  // name mid-tag. What follows the stray bracket is half an attribute, not part
  // of the title, and keeping it stopped the whole set from matching.
  EXPECT_EQ(CGameNameParser::Parse("Bulldog (1988)(Ariolasoft UK LTD)(Tape 2.tap").title, "Bulldog");
  EXPECT_EQ(CGameNameParser::Parse("Samurai Trilogy (1988)(Ariolasoft UK LT2.tap").title,
            "Samurai Trilogy");
  EXPECT_EQ(CGameNameParser::Parse("Auf Wiedersehen Monty (1988)(Ariolasoft .tap").title,
            "Auf Wiedersehen Monty");
  EXPECT_EQ(CGameNameParser::Parse("Elite [cr TWD.d64").title, "Elite");
}

TEST(TestGameNameParser, LeavesEveryOtherConventionAlone)
{
  // A catalogue name has spaces, so it must never be taken for a WHDLoad slave
  EXPECT_EQ(CGameNameParser::ParseWhdLoadName("Sonic The Hedgehog (USA, Europe).md"), "");
  EXPECT_EQ(CGameNameParser::ParseWhdLoadName("Savage (1988)(Probe Software).tap"), "");
  // no version, so not a slave name
  EXPECT_EQ(CGameNameParser::ParseWhdLoadName("Turrican3.lha"), "");
}

TEST(TestGameNameParser, ReadsAWhdLoadNameThroughParse)
{
  const ParsedGameName parsed = CGameNameParser::Parse("BodyBlows_v1.2_AGA_1322");
  EXPECT_EQ(parsed.displayTitle, "Body Blows");
}

TEST(TestGameNameParser, StillReadsATosecName)
{
  const ParsedGameName parsed = CGameNameParser::Parse("Savage (1988)(Probe Software).tap");
  EXPECT_EQ(parsed.displayTitle, "Savage");
}
