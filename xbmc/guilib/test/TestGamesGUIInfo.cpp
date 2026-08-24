/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "FileItem.h"
#include "GUIInfoManager.h"
#include "ServiceBroker.h"
#include "games/AchievementRuntime.h"
#include "games/tags/GameInfoTag.h"
#include "guilib/guiinfo/GUIInfo.h"
#include "guilib/guiinfo/GUIInfoLabels.h"
#include "guilib/guiinfo/GamesGUIInfo.h"
#include "settings/Settings.h"
#include "settings/SettingsComponent.h"
#include "utils/FileExtensionProvider.h"

#include <gtest/gtest.h>

using namespace KODI::GAME;
using namespace KODI::GUILIB::GUIINFO;

class TestGamesGUIInfo : public testing::Test
{
protected:
  void SetUp() override
  {
    auto settings = CServiceBroker::GetSettingsComponent()->GetSettings();
    m_showExtensionsOriginal = settings->GetBool(CSettings::SETTING_FILELISTS_SHOWEXTENSIONS);
    settings->SetBool(CSettings::SETTING_FILELISTS_SHOWEXTENSIONS, false);

    CServiceBroker::GetFileExtensionProvider().RegisterGameExtensions({".rom"});
  }

  void TearDown() override
  {
    CServiceBroker::GetFileExtensionProvider().UnregisterGameExtensions({".rom"});

    CServiceBroker::GetSettingsComponent()->GetSettings()->SetBool(
        CSettings::SETTING_FILELISTS_SHOWEXTENSIONS, m_showExtensionsOriginal);
  }

  bool m_showExtensionsOriginal{false};
};

TEST_F(TestGamesGUIInfo, TranslatesRetroPlayerLabels)
{
  CGUIInfoManager infoManager;

  EXPECT_EQ(infoManager.TranslateString("RetroPlayer.Title"), RETROPLAYER_TITLE);
  EXPECT_EQ(infoManager.TranslateString("RetroPlayer.Platform"), RETROPLAYER_PLATFORM);
  EXPECT_EQ(infoManager.TranslateString("RetroPlayer.Genres"), RETROPLAYER_GENRES);
  EXPECT_EQ(infoManager.TranslateString("RetroPlayer.Publisher"), RETROPLAYER_PUBLISHER);
  EXPECT_EQ(infoManager.TranslateString("RetroPlayer.Developer"), RETROPLAYER_DEVELOPER);
  EXPECT_EQ(infoManager.TranslateString("RetroPlayer.Overview"), RETROPLAYER_OVERVIEW);
  EXPECT_EQ(infoManager.TranslateString("RetroPlayer.GameClient"), RETROPLAYER_GAME_CLIENT);
  EXPECT_EQ(infoManager.TranslateString("RetroPlayer.GameClientName"),
            RETROPLAYER_GAME_CLIENT_NAME);
  EXPECT_EQ(infoManager.TranslateString("RetroPlayer.GameClientPlatforms"),
            RETROPLAYER_GAME_CLIENT_PLATFORMS);
  EXPECT_EQ(infoManager.TranslateString("RetroPlayer.RichPresence"), RETROPLAYER_RICH_PRESENCE);
  EXPECT_EQ(infoManager.TranslateString("RetroPlayer.AchievementsLoggedIn"),
            RETROPLAYER_ACHIEVEMENTS_LOGGED_IN);
  EXPECT_EQ(infoManager.TranslateString("RetroPlayer.AchievementsProgress"),
            RETROPLAYER_ACHIEVEMENTS_PROGRESS);
  EXPECT_EQ(infoManager.TranslateString("RetroPlayer.LeaderboardTracker"),
            RETROPLAYER_LEADERBOARD_TRACKER);
  EXPECT_EQ(infoManager.TranslateString("RetroPlayer.AchievementsIndicatorTitle"),
            RETROPLAYER_ACHIEVEMENTS_INDICATOR_TITLE);
  EXPECT_EQ(infoManager.TranslateString("RetroPlayer.AchievementsIndicatorPercent"),
            RETROPLAYER_ACHIEVEMENTS_INDICATOR_PERCENT);
}

TEST_F(TestGamesGUIInfo, ProgressIndicatorIsEmptyWithNothingBeingWorkedTowards)
{
  CAchievementRuntime achievementRuntime;

  CGamesGUIInfo gamesGUIInfo{achievementRuntime};
  std::string value{"stale"};

  EXPECT_TRUE(gamesGUIInfo.GetLabel(value, nullptr, 0,
                                    CGUIInfo(RETROPLAYER_ACHIEVEMENTS_INDICATOR_TITLE), nullptr));
  EXPECT_TRUE(value.empty());

  // The percentage must come back empty too, not "0": the skin hides the whole
  // indicator on the title being empty, and a stray "0%" would sit on screen
  value = "stale";
  EXPECT_TRUE(gamesGUIInfo.GetLabel(value, nullptr, 0,
                                    CGUIInfo(RETROPLAYER_ACHIEVEMENTS_INDICATOR_PERCENT), nullptr));
  EXPECT_TRUE(value.empty());
}

TEST_F(TestGamesGUIInfo, ProgressIndicatorShowsWhatIsBeingWorkedTowards)
{
  CAchievementRuntime achievementRuntime;

  AchievementProgressIndicator indicator;
  indicator.id = 123615;
  indicator.title = "Ring Collector";
  indicator.badgeUrl = "https://example.invalid/badge.png";
  indicator.measuredProgress = "13/180";
  indicator.measuredPercent = 7.2f;
  achievementRuntime.SetProgressIndicator(indicator, true);

  CGamesGUIInfo gamesGUIInfo{achievementRuntime};
  std::string value;

  EXPECT_TRUE(gamesGUIInfo.GetLabel(value, nullptr, 0,
                                    CGUIInfo(RETROPLAYER_ACHIEVEMENTS_INDICATOR_TITLE), nullptr));
  EXPECT_EQ(value, "Ring Collector");

  EXPECT_TRUE(gamesGUIInfo.GetLabel(
      value, nullptr, 0, CGUIInfo(RETROPLAYER_ACHIEVEMENTS_INDICATOR_PROGRESS), nullptr));
  EXPECT_EQ(value, "13/180");

  // A progress control reads the value through GetInt, not GetLabel, so the
  // bar would never move if only the string path answered
  int percent = -1;
  EXPECT_TRUE(gamesGUIInfo.GetInt(percent, nullptr, 0,
                                  CGUIInfo(RETROPLAYER_ACHIEVEMENTS_INDICATOR_PERCENT)));
  EXPECT_EQ(percent, 7);

  // An update replaces the value rather than stacking a second indicator
  indicator.measuredProgress = "90/180";
  indicator.measuredPercent = 50.0f;
  achievementRuntime.SetProgressIndicator(indicator, true);

  EXPECT_TRUE(gamesGUIInfo.GetInt(percent, nullptr, 0,
                                  CGUIInfo(RETROPLAYER_ACHIEVEMENTS_INDICATOR_PERCENT)));
  EXPECT_EQ(percent, 50);

  // The runtime sends no achievement with a hide, so this is what actually
  // arrives - and matching on the id alone left the indicator on screen for the
  // rest of the session
  achievementRuntime.SetProgressIndicator(AchievementProgressIndicator{}, false);

  EXPECT_TRUE(gamesGUIInfo.GetLabel(value, nullptr, 0,
                                    CGUIInfo(RETROPLAYER_ACHIEVEMENTS_INDICATOR_TITLE), nullptr));
  EXPECT_TRUE(value.empty());
  EXPECT_TRUE(gamesGUIInfo.GetInt(percent, nullptr, 0,
                                  CGUIInfo(RETROPLAYER_ACHIEVEMENTS_INDICATOR_PERCENT)));
  EXPECT_EQ(percent, 0);
}

TEST_F(TestGamesGUIInfo, TwoCountingAchievementsDoNotFightOverTheIndicator)
{
  CAchievementRuntime achievementRuntime;

  // A game counting two things at once announces each separately, milliseconds
  // apart. Holding one slot meant they overwrote each other and neither could
  // be read.
  AchievementProgressIndicator rings;
  rings.id = 41785;
  rings.title = "Orange Ace";
  rings.measuredProgress = "1/20";
  rings.measuredPercent = 5.0f;

  AchievementProgressIndicator survive;
  survive.id = 3415;
  survive.title = "Trip Pop Pro";
  survive.measuredProgress = "60%";
  survive.measuredPercent = 60.0f;

  achievementRuntime.SetProgressIndicator(rings, true);
  achievementRuntime.SetProgressIndicator(survive, true);

  CGamesGUIInfo gamesGUIInfo{achievementRuntime};
  std::string value;

  // The one closest to being earned, not whichever ticked last
  EXPECT_TRUE(gamesGUIInfo.GetLabel(value, nullptr, 0,
                                    CGUIInfo(RETROPLAYER_ACHIEVEMENTS_INDICATOR_TITLE), nullptr));
  EXPECT_EQ(value, "Trip Pop Pro");

  // ...and it stays that way when the other one ticks again
  rings.measuredProgress = "2/20";
  rings.measuredPercent = 10.0f;
  achievementRuntime.SetProgressIndicator(rings, true);

  EXPECT_TRUE(gamesGUIInfo.GetLabel(value, nullptr, 0,
                                    CGUIInfo(RETROPLAYER_ACHIEVEMENTS_INDICATOR_TITLE), nullptr));
  EXPECT_EQ(value, "Trip Pop Pro");

  // Overtaking swaps which is shown
  rings.measuredPercent = 90.0f;
  achievementRuntime.SetProgressIndicator(rings, true);

  EXPECT_TRUE(gamesGUIInfo.GetLabel(value, nullptr, 0,
                                    CGUIInfo(RETROPLAYER_ACHIEVEMENTS_INDICATOR_TITLE), nullptr));
  EXPECT_EQ(value, "Orange Ace");

  // Retiring one leaves the other showing rather than clearing both
  achievementRuntime.SetProgressIndicator(rings, false);

  EXPECT_TRUE(gamesGUIInfo.GetLabel(value, nullptr, 0,
                                    CGUIInfo(RETROPLAYER_ACHIEVEMENTS_INDICATOR_TITLE), nullptr));
  EXPECT_EQ(value, "Trip Pop Pro");

  // A hide with no achievement means everything has stopped
  achievementRuntime.SetProgressIndicator(AchievementProgressIndicator{}, false);

  EXPECT_TRUE(gamesGUIInfo.GetLabel(value, nullptr, 0,
                                    CGUIInfo(RETROPLAYER_ACHIEVEMENTS_INDICATOR_TITLE), nullptr));
  EXPECT_TRUE(value.empty());
}

TEST_F(TestGamesGUIInfo, IndicatorChangesAreAnnouncedOnce)
{
  CAchievementRuntime achievementRuntime;

  int announced = 0;
  achievementRuntime.SetIndicatorCallback([&announced]() { ++announced; });

  AchievementProgressIndicator indicator;
  indicator.id = 1;
  achievementRuntime.SetProgressIndicator(indicator, true);

  AchievementChallenge challenge;
  challenge.id = 2;
  achievementRuntime.SetChallenge(challenge, true);

  LeaderboardTracker tracker;
  tracker.id = 3;
  achievementRuntime.SetLeaderboardTracker(tracker, true);

  // Every indicator reports through the one path, so a fourth added later is
  // announced without anyone having to remember to wire it up
  EXPECT_EQ(announced, 3);
}

TEST_F(TestGamesGUIInfo, LeaderboardTrackerIsEmptyWithNoAttemptRunning)
{
  CAchievementRuntime achievementRuntime;

  CGamesGUIInfo gamesGUIInfo{achievementRuntime};
  std::string value{"stale"};

  EXPECT_TRUE(
      gamesGUIInfo.GetLabel(value, nullptr, 0, CGUIInfo(RETROPLAYER_LEADERBOARD_TRACKER), nullptr));
  EXPECT_TRUE(value.empty());
}

TEST_F(TestGamesGUIInfo, LeaderboardTrackerShowsTheRunningAttempt)
{
  CAchievementRuntime achievementRuntime;

  LeaderboardTracker tracker;
  tracker.id = 4;
  tracker.display = "1:24.60";
  achievementRuntime.SetLeaderboardTracker(tracker, true);

  CGamesGUIInfo gamesGUIInfo{achievementRuntime};
  std::string value;

  EXPECT_TRUE(
      gamesGUIInfo.GetLabel(value, nullptr, 0, CGUIInfo(RETROPLAYER_LEADERBOARD_TRACKER), nullptr));
  EXPECT_EQ(value, "1:24.60");

  // An update to an attempt already showing replaces its value rather than
  // adding a second tracker
  tracker.display = "1:31.02";
  achievementRuntime.SetLeaderboardTracker(tracker, true);

  EXPECT_EQ(achievementRuntime.GetLeaderboardTrackers().size(), 1u);
  EXPECT_TRUE(
      gamesGUIInfo.GetLabel(value, nullptr, 0, CGUIInfo(RETROPLAYER_LEADERBOARD_TRACKER), nullptr));
  EXPECT_EQ(value, "1:31.02");

  achievementRuntime.SetLeaderboardTracker(tracker, false);

  EXPECT_TRUE(achievementRuntime.GetLeaderboardTrackers().empty());
  EXPECT_TRUE(
      gamesGUIInfo.GetLabel(value, nullptr, 0, CGUIInfo(RETROPLAYER_LEADERBOARD_TRACKER), nullptr));
  EXPECT_TRUE(value.empty());
}

TEST_F(TestGamesGUIInfo, ScoreboardWritesThePlayersNewStandingBack)
{
  LeaderboardInfo leaderboard;
  leaderboard.id = 7;
  leaderboard.title = "Green Hill Zone - Act 1";
  leaderboard.playerRank = 274;
  leaderboard.playerScore = "0:41.88";
  leaderboard.totalEntries = 1483;

  // Standings fetched before the submission, which the new one is not in
  LeaderboardEntry entry;
  entry.rank = 1;
  entry.username = "Sonikku";
  leaderboard.entries = {entry};

  LeaderboardState state;
  state.leaderboards = {leaderboard};

  CAchievementRuntime achievementRuntime;
  achievementRuntime.SetLeaderboardState(state);

  EXPECT_TRUE(achievementRuntime.SetLeaderboardStanding(7, 12, "0:26.11", 1484));

  const LeaderboardInfo& updated = achievementRuntime.GetLeaderboardState().leaderboards.front();
  EXPECT_EQ(updated.playerRank, 12u);
  EXPECT_EQ(updated.playerScore, "0:26.11");
  EXPECT_EQ(updated.totalEntries, 1484u);

  // Guessing where the submission slots into the fetched page would be wrong as
  // often as right, so the page is dropped and refetched on next open
  EXPECT_TRUE(updated.entries.empty());

  // A leaderboard belonging to a game that has since been unloaded
  EXPECT_FALSE(achievementRuntime.SetLeaderboardStanding(999, 1, "0:01.00", 2));
}

namespace
{
AchievementState MakeAchievementState()
{
  AchievementInfo earned;
  earned.id = 1;
  earned.title = "Fated Hour";
  earned.earned = true;

  AchievementInfo locked;
  locked.id = 2;
  locked.title = "The Fall of Guardia";

  AchievementState state;
  state.gameTitle = "Chrono Trigger";
  state.totalAchievements = 2;
  state.unlockedAchievements = 1;
  state.achievements = {earned, locked};
  state.loaded = true;

  return state;
}
} // namespace

TEST_F(TestGamesGUIInfo, GetsAchievementProgressFromAchievementState)
{
  CAchievementRuntime achievementRuntime;
  achievementRuntime.SetState(MakeAchievementState());

  CGamesGUIInfo gamesGUIInfo{achievementRuntime};
  std::string value;

  EXPECT_TRUE(gamesGUIInfo.GetLabel(value, nullptr, 0, CGUIInfo(RETROPLAYER_ACHIEVEMENTS_PROGRESS),
                                    nullptr));
  EXPECT_EQ(value, "1 / 2");
}

TEST_F(TestGamesGUIInfo, AchievementProgressIsEmptyWithoutAchievements)
{
  CAchievementRuntime achievementRuntime;

  CGamesGUIInfo gamesGUIInfo{achievementRuntime};
  std::string value{"stale"};

  EXPECT_TRUE(gamesGUIInfo.GetLabel(value, nullptr, 0, CGUIInfo(RETROPLAYER_ACHIEVEMENTS_PROGRESS),
                                    nullptr));
  EXPECT_TRUE(value.empty());
}

TEST_F(TestGamesGUIInfo, MarkEarnedOnlyCountsTheFirstUnlock)
{
  CAchievementRuntime achievementRuntime;
  achievementRuntime.SetState(MakeAchievementState());

  bool newlyEarned = false;
  AchievementState state = achievementRuntime.MarkEarned(2, "2026-08-05 19:20", newlyEarned);
  EXPECT_TRUE(newlyEarned);
  EXPECT_EQ(state.unlockedAchievements, 2U);
  EXPECT_EQ(state.achievements[1].unlockedDate, "2026-08-05 19:20");

  // The achievement runtime re-reports achievements earned in an earlier
  // session, which must not inflate the count or replace the unlock date
  state = achievementRuntime.MarkEarned(2, "2026-08-06 08:15", newlyEarned);
  EXPECT_FALSE(newlyEarned);
  EXPECT_EQ(state.unlockedAchievements, 2U);
  EXPECT_EQ(state.achievements[1].unlockedDate, "2026-08-05 19:20");

  // An unknown ID must not change anything
  state = achievementRuntime.MarkEarned(99, "2026-08-07 12:00", newlyEarned);
  EXPECT_FALSE(newlyEarned);
  EXPECT_EQ(state.unlockedAchievements, 2U);
}

TEST_F(TestGamesGUIInfo, GetsRichPresenceFromAchievementState)
{
  CAchievementRuntime achievementRuntime;
  achievementRuntime.SetRichPresence("Fighting Lavos");

  CGamesGUIInfo gamesGUIInfo{achievementRuntime};
  std::string value;

  EXPECT_TRUE(
      gamesGUIInfo.GetLabel(value, nullptr, 0, CGUIInfo(RETROPLAYER_RICH_PRESENCE), nullptr));
  EXPECT_EQ(value, "Fighting Lavos");
}

TEST_F(TestGamesGUIInfo, GetLabelRequiresCurrentGameInGUIInfoManager)
{
  CFileItem item{"/roms/test.rom", false};
  item.GetGameInfoTag()->SetTitle("Chrono Trigger");

  CGamesGUIInfo gamesGUIInfo;
  std::string value;

  EXPECT_FALSE(gamesGUIInfo.GetLabel(value, &item, 0, CGUIInfo(RETROPLAYER_TITLE), nullptr));
  EXPECT_TRUE(value.empty());
}

TEST_F(TestGamesGUIInfo, InitCurrentItemSetsTitleFromFilesystemPath)
{
  CFileItem item{"/roms/test.rom", false};
  item.GetGameInfoTag();

  CGamesGUIInfo gamesGUIInfo;

  EXPECT_TRUE(gamesGUIInfo.InitCurrentItem(&item));

  const CGameInfoTag* tag = item.GetGameInfoTag();
  ASSERT_NE(tag, nullptr);
  EXPECT_EQ(tag->GetTitle(), "test");
}

TEST_F(TestGamesGUIInfo, InitCurrentItemSetsTitleFromVfsHostnamePath)
{
  CFileItem item{"zip://test.rom/", false};
  item.GetGameInfoTag();

  CGamesGUIInfo gamesGUIInfo;

  EXPECT_TRUE(gamesGUIInfo.InitCurrentItem(&item));

  const CGameInfoTag* tag = item.GetGameInfoTag();
  ASSERT_NE(tag, nullptr);
  EXPECT_EQ(tag->GetTitle(), "test");
}
