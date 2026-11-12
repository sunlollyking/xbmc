/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "games/library/GameHighScores.h"
#include "games/library/HighScoreDecoder.h"

#include <string>
#include <vector>

#include <gtest/gtest.h>

using namespace KODI::GAME;

namespace
{
// Two places of a three-byte packed BCD score and a three-letter name, the
// letters numbered from 10 for A
constexpr auto TWO_PLACES = R"(<hi2txt>
  <structure file=".hi">
    <check><size>12</size></check>
    <loop count="2">
      <elt size="3" type="int"  id="SCORE" base="16"/>
      <elt size="3" type="text" id="NAME"  charset="names" ascii-offset="55"/>
    </loop>
  </structure>
  <output>
    <table>
      <column id="RANK" src="index" format="+1"/>
      <column id="SCORE" format="*10"/>
      <column id="NAME"/>
      <column id="SCORE" format="0x" display="debug"/>
    </table>
  </output>
  <charset id="names"><char src="0x24" dst=" "/></charset>
</hi2txt>)";

const std::vector<uint8_t> TWO_PLACES_FILE{0x01, 0x23, 0x45, 0x0a, 0x0b, 0x0c,
                                           0x00, 0x50, 0x00, 0x0d, 0x24, 0x0e};

bool Decode(const std::string& description,
            const std::vector<uint8_t>& data,
            HighScores& scores,
            HighScoreDetail detail = HighScoreDetail::ALWAYS,
            const CHighScoreDecoder::DescriptionLoader& load = nullptr,
            const std::string& dtd = "")
{
  CHighScoreDecoder decoder;
  decoder.SetEntities(dtd);
  return decoder.Load(description, load) && decoder.Decode("game.hi", data, detail, scores);
}
} // namespace

TEST(TestHighScoreDecoder, ReadsTheScoresAGameSaved)
{
  HighScores scores;
  ASSERT_TRUE(Decode(TWO_PLACES, TWO_PLACES_FILE, scores));
  ASSERT_EQ(scores.parts.size(), 1u);
  ASSERT_TRUE(scores.parts[0].isTable);

  const HighScoreTable& table = scores.parts[0].table;
  EXPECT_EQ(table.columns, (std::vector<std::string>{"RANK", "SCORE", "NAME"}));
  ASSERT_EQ(table.rows.size(), 2u);
  EXPECT_EQ(table.rows[0], (std::vector<std::string>{"1", "123450", "ABC"}));
  EXPECT_EQ(table.rows[1], (std::vector<std::string>{"2", "50000", "D E"}));
}

TEST(TestHighScoreDecoder, ShowsWhatIsForDebuggingOnlyWhenAsked)
{
  HighScores scores;
  ASSERT_TRUE(Decode(TWO_PLACES, TWO_PLACES_FILE, scores, HighScoreDetail::DEBUG));
  ASSERT_EQ(scores.parts[0].table.columns.size(), 4u);
  EXPECT_EQ(scores.parts[0].table.rows[0][3], "0x3039");
}

TEST(TestHighScoreDecoder, AFileOfAnotherSizeIsNotRead)
{
  HighScores scores;
  const std::vector<uint8_t> shorter(TWO_PLACES_FILE.begin(), TWO_PLACES_FILE.end() - 1);
  EXPECT_FALSE(Decode(TWO_PLACES, shorter, scores));
  EXPECT_TRUE(scores.parts.empty());
}

TEST(TestHighScoreDecoder, ACloneIsReadAsItsParent)
{
  const auto load = [](const std::string& set) { return set == "parent" ? TWO_PLACES : ""; };
  HighScores scores;
  ASSERT_TRUE(Decode(R"(<hi2txt><sameas id="parent"/></hi2txt>)", TWO_PLACES_FILE, scores,
                     HighScoreDetail::ALWAYS, load));
  EXPECT_EQ(scores.parts[0].table.rows[0][2], "ABC");

  EXPECT_FALSE(Decode(R"(<hi2txt><sameas id="missing"/></hi2txt>)", TWO_PLACES_FILE, scores,
                      HighScoreDetail::ALWAYS, load));
}

TEST(TestHighScoreDecoder, LayoutsOnlyHiscoreDatTellsApartAreNotGuessedBetween)
{
  const std::string two = R"(<hi2txt>
  <structure file=".hi"><check><size>2</size></check><elt size="2" type="int" id="SCORE"/></structure>
  <structure file=".hi"><check><size>2</size></check><elt size="2" type="int" id="SCORE" base="16"/></structure>
  <output><field id="SCORE"/></output>
</hi2txt>)";
  HighScores scores;
  EXPECT_FALSE(Decode(two, {0x12, 0x34}, scores));
}

TEST(TestHighScoreDecoder, WhatItCannotFollowIsNotShown)
{
  std::string unknown = TWO_PLACES;
  unknown.replace(unknown.find("format=\"*10\""), 12, "format=\"Wibble\"");
  HighScores scores;
  EXPECT_FALSE(Decode(unknown, TWO_PLACES_FILE, scores));
}

TEST(TestHighScoreDecoder, NamedCharactersComeFromTheDTD)
{
  std::string hearts = TWO_PLACES;
  hearts.replace(hearts.find("dst=\" \""), 7, "dst=\"&heart;\"");
  HighScores scores;
  ASSERT_TRUE(Decode(hearts, TWO_PLACES_FILE, scores, HighScoreDetail::ALWAYS, nullptr,
                     R"(<!ENTITY heart "&#9829;">)"));
  EXPECT_EQ(scores.parts[0].table.rows[1][2], "D♥E");
}

TEST(TestGameHighScores, TheBestIsTheTopScoreAndWhoSetIt)
{
  HighScoreTable table;
  table.columns = {"RANK", "SCORE", "NAME", "STAGE"};
  table.rows = {{"1", "123450", "ABC ", "5"}, {"2", "50000", "D E", "3"}};
  EXPECT_EQ(CGameHighScores::Best(table), "123450 · ABC");

  table.columns = {"RANK", "TIME"};
  table.rows = {{"1", "01'24''16"}};
  EXPECT_EQ(CGameHighScores::Best(table), "01'24''16");

  EXPECT_EQ(CGameHighScores::Best(HighScoreTable{}), "");
}

TEST(TestGameHighScores, KeptTablesReadBack)
{
  HighScoreTable table;
  table.columns = {"RANK", "SCORE", "NAME"};
  table.rows = {{"1", "123450", "ABC"}, {"2", "50000", "D \"E\""}};

  EXPECT_EQ(CGameHighScores::Deserialize(CGameHighScores::Serialize(table)), table);
  EXPECT_TRUE(CGameHighScores::Deserialize("not json").rows.empty());
  EXPECT_EQ(CGameHighScores::Text(table),
            "[B]RANK   SCORE   NAME[/B]\n1   123450   ABC\n2   50000   D \"E\"");
}
