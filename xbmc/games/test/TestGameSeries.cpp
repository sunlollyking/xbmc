/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "FileItem.h"
#include "FileItemList.h"
#include "games/database/GameDatabase.h"
#include "games/library/GameDbUrl.h"
#include "games/tags/GameInfoTag.h"

#include <memory>
#include <string>

#include <gtest/gtest.h>

using namespace KODI::GAME;

namespace
{
std::shared_ptr<CFileItem> Game(const std::string& title,
                                unsigned int year,
                                int idSeries = -1,
                                const std::string& series = "")
{
  auto item = std::make_shared<CFileItem>(title);
  item->SetPath("/games/" + title + ".zip");
  CGameInfoTag* tag = item->GetGameInfoTag();
  tag->SetTitle(title);
  tag->SetYear(year);
  tag->SetSeriesId(idSeries);
  tag->SetSeries(series);
  item->SetArt("poster", "/art/" + title + ".png");
  return item;
}

CGameDbUrl Url(const std::string& path)
{
  CGameDbUrl url;
  EXPECT_TRUE(url.FromString(path)) << path;
  return url;
}
} // namespace

TEST(TestGameSeries, OnlyTheTitlesListFolds)
{
  EXPECT_TRUE(Url("gamedb://titles/").IsTitles());
  EXPECT_TRUE(Url("gamedb://platforms/3/titles/").IsTitles());
  EXPECT_FALSE(Url("gamedb://titles/12/").IsTitles());
  EXPECT_FALSE(Url("gamedb://recentlyadded/").IsTitles());
  EXPECT_FALSE(Url("gamedb://series/7/").IsTitles());
  EXPECT_FALSE(Url("gamedb://titles/?xsp=%7B%7D").IsTitles());
  EXPECT_FALSE(Url("gamedb://titles/?similarto=12").IsTitles());
  EXPECT_FALSE(Url("gamedb://titles/?relatedto=12").IsTitles());
}

TEST(TestGameSeries, PopularityIsAList)
{
  EXPECT_EQ(Url("gamedb://popular/").GetList(), "popular");
  EXPECT_EQ(Url("gamedb://platforms/3/mostwanted/").GetList(), "mostwanted");
  EXPECT_EQ(GamePopularity::Valid(-1.0), 0.0);
  EXPECT_EQ(GamePopularity::Valid(std::nan("")), 0.0);
  EXPECT_EQ(GamePopularity::Valid(0.25), 0.25);
}

TEST(TestGameSeries, ModesAreAFacet)
{
  EXPECT_EQ(Url("gamedb://modes/").GetNode(), GameDbNode::MODES);
  CVariant id;
  ASSERT_TRUE(Url("gamedb://modes/3/").GetOption("modeid", id));
  EXPECT_EQ(id.asInteger(), 3);
}

TEST(TestGameSeries, ASeriesIsAFacet)
{
  const CGameDbUrl list = Url("gamedb://series/");
  EXPECT_EQ(list.GetNode(), GameDbNode::SERIES);

  const CGameDbUrl one = Url("gamedb://platforms/3/series/7/");
  EXPECT_EQ(one.GetNode(), GameDbNode::GAMES);
  CVariant id;
  ASSERT_TRUE(one.GetOption("seriesid", id));
  EXPECT_EQ(id.asInteger(), 7);
}

TEST(TestGameSeries, TheGamesOfASeriesFoldIntoOneEntry)
{
  CFileItemList items;
  items.Add(Game("Mario Kart: Double Dash!!", 2003, 7, "Mario Kart"));
  items.Add(Game("Super Mario Kart", 1992, 7, "Mario Kart"));
  items.Add(Game("Mario Kart 64", 0, 7, "Mario Kart"));
  items.Add(Game("F-Zero", 1990));
  items.Add(Game("Wave Race 64", 1996, 9, "Wave Race"));

  CGameDatabase::FoldSeries(Url("gamedb://platforms/3/titles/"), items);

  ASSERT_EQ(items.Size(), 3);
  const auto series = items.Get("gamedb://platforms/3/series/7/");
  ASSERT_NE(series, nullptr);
  EXPECT_TRUE(series->IsFolder());
  EXPECT_EQ(series->GetLabel(), "Mario Kart");
  EXPECT_EQ(series->GetLabel2(), "3");
  EXPECT_EQ(series->GetProperty("seriesid").asInteger(), 7);
  // Pictured by the first game out, not by one with no year
  EXPECT_EQ(series->GetArt("poster"), "/art/Super Mario Kart.png");

  // A series of one is still just the game
  EXPECT_NE(items.Get("/games/Wave Race 64.zip"), nullptr);
  EXPECT_NE(items.Get("/games/F-Zero.zip"), nullptr);
}
