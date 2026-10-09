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

TEST(TestGameNameParser, TellsADataDiskFromTheGamesOwn)
{
  EXPECT_TRUE(CGameNameParser::Parse("Ashe (Data disk).d88").dataDisk);
  EXPECT_TRUE(CGameNameParser::Parse("Abyss (User disk) {V1 mode}.d88").dataDisk);
  EXPECT_TRUE(CGameNameParser::Parse("Arcush (Scenario disk).d88").dataDisk);
  EXPECT_TRUE(CGameNameParser::Parse("Fushigi no Umi no Nadia (Music Disk) [FD].zip").dataDisk);
  EXPECT_FALSE(CGameNameParser::Parse("Ashe (Game disk).d88").dataDisk);
  EXPECT_FALSE(CGameNameParser::Parse("Ys (Disk A).d88").dataDisk);
  EXPECT_EQ(CGameNameParser::Parse("Ashe (Data disk).d88").title, "Ashe");
}

TEST(TestGameNameParser, TellsAnotherDumpFromTheCleanOne)
{
  EXPECT_TRUE(CGameNameParser::Parse("Super Mario Bros. Special [Set 1].d88").alternate);
  EXPECT_TRUE(CGameNameParser::Parse("Super Mario Bros. Special [Alt 2].d88").alternate);
  EXPECT_TRUE(CGameNameParser::Parse("Thunder Force (SR) [Set 2] [bad sectors].d88").bad);
  EXPECT_TRUE(CGameNameParser::Parse("Gradius (Kai hack).d88").hack);
  EXPECT_TRUE(CGameNameParser::Parse("Thexder (invincibility hack) [Set 1].d88").hack);
  const ParsedGameName clean = CGameNameParser::Parse("Thexder.d88");
  EXPECT_FALSE(clean.alternate || clean.bad || clean.hack);
  EXPECT_EQ(CGameNameParser::Parse("Thexder (invincibility hack) [Set 1].d88").title, "Thexder");
}

TEST(TestGameNameParser, LeavesTheLoadingInstructionsOutOfTheTitle)
{
  EXPECT_EQ(CGameNameParser::Parse("Apploon {V1 mode, MON R G8F00}.t88").title, "Apploon");
  EXPECT_EQ(CGameNameParser::Parse("Melt Down  {4MHz}.d88").title, "Melt Down");
  EXPECT_EQ(CGameNameParser::Parse("Abyss (User disk) {V1 mode}.d88").title, "Abyss");
}

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

TEST(TestGameNameParser, DropsAScenesReleaseNumber)
{
  EXPECT_EQ(CGameNameParser::Parse("2797 Kimi no Yusha (JP).zip").title, "Kimi no Yusha");
  EXPECT_EQ(CGameNameParser::Parse("4737 Super Robot Taisen OG Saga (JP).zip").title,
            "Super Robot Taisen OG Saga");
  EXPECT_EQ(CGameNameParser::Parse("0414 My Pet Hotel (EU)(M2).zip").title, "My Pet Hotel");
}

TEST(TestGameNameParser, KeepsATitleThatStartsWithANumber)
{
  EXPECT_EQ(CGameNameParser::Parse("1943 Kai (Japan).zip").title, "1943 Kai");
  EXPECT_EQ(CGameNameParser::Parse("2002 FIFA World Cup (USA) (En,Es).cue").title,
            "2002 FIFA World Cup");
  EXPECT_EQ(CGameNameParser::Parse("1942 (1986)(Elite)(GB).tap").title, "1942");
  EXPECT_EQ(CGameNameParser::Parse("1943 Kai (1991)(Namco)(JP).pce").title, "1943 Kai");
  EXPECT_EQ(CGameNameParser::Parse("2010 - The Graphic Action Game.col").title,
            "2010 - The Graphic Action Game");
}

TEST(TestGameNameParser, KeepsTheDotsInAFoldersName)
{
  EXPECT_EQ(CGameNameParser::Parse("G.R", false).title, "G.R");
  EXPECT_EQ(CGameNameParser::Parse("Flight Simulator Ver.5.0", false).title,
            "Flight Simulator Ver.5.0");
  EXPECT_EQ(CGameNameParser::Parse("G.R").title, "G");
}

TEST(TestGameNameParser, CreditsTheCompanyInATagOfItsOwn)
{
  EXPECT_EQ(CGameNameParser::Credits(CGameNameParser::Parse("Tetris (Doujin - Noripy)", false)),
            std::vector<std::string>{"Noripy"});
  EXPECT_EQ(CGameNameParser::Credits(
                CGameNameParser::Parse("Teki wa Kaizoku - Kaizokuban (D-Photon - Victor)", false)),
            (std::vector<std::string>{"D-Photon", "Victor"}));
  EXPECT_TRUE(CGameNameParser::Credits(CGameNameParser::Parse("Sonic (USA, Europe).md")).empty());
  EXPECT_TRUE(
      CGameNameParser::Credits(CGameNameParser::Parse("Elite (1985)(Firebird)[a].d64")).empty());
  // A flag in square brackets describes the dump
  EXPECT_TRUE(CGameNameParser::Credits(CGameNameParser::Parse("Artemis [HD].zip")).empty());
  EXPECT_TRUE(CGameNameParser::Credits(CGameNameParser::Parse("Mirage [extras].zip")).empty());
  // A company whose name starts like a disc label is still a company
  EXPECT_EQ(CGameNameParser::Credits(CGameNameParser::Parse("Mirage (Discovery)", false)),
            std::vector<std::string>{"Discovery"});
}

TEST(TestGameNameParser, TellsCompaniesApart)
{
  EXPECT_TRUE(CGameNameParser::SameCompany("Micro Cabin", "Microcabin"));
  EXPECT_TRUE(CGameNameParser::SameCompany("Riverhill Soft", "Riverhillsoft"));
  EXPECT_TRUE(CGameNameParser::SameCompany("Onion Soft", "Doujin - Onion Soft"));
  EXPECT_FALSE(CGameNameParser::SameCompany("Noripy", "BPS"));
  EXPECT_FALSE(CGameNameParser::SameCompany("Apollo Technica", "Tsukumo"));
}
