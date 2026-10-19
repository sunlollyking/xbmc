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
  EXPECT_EQ(infoManager.TranslateString("RetroPlayer.HasCheats"), RETROPLAYER_HAS_CHEATS);
  EXPECT_EQ(infoManager.TranslateString("RetroPlayer.SupportsCheats"), RETROPLAYER_SUPPORTS_CHEATS);
  EXPECT_EQ(infoManager.TranslateString("RetroPlayer.AchievementsIndicatorTitle"),
            RETROPLAYER_ACHIEVEMENTS_INDICATOR_TITLE);
  EXPECT_EQ(infoManager.TranslateString("RetroPlayer.AchievementsIndicatorProgress"),
            RETROPLAYER_ACHIEVEMENTS_INDICATOR_PROGRESS);
  EXPECT_EQ(infoManager.TranslateString("RetroPlayer.AchievementsIndicatorPercent"),
            RETROPLAYER_ACHIEVEMENTS_INDICATOR_PERCENT);
  EXPECT_EQ(infoManager.TranslateString("RetroPlayer.AchievementsIndicatorBadge"),
            RETROPLAYER_ACHIEVEMENTS_INDICATOR_BADGE);
  EXPECT_EQ(infoManager.TranslateString("RetroPlayer.AchievementsChallengeTitle"),
            RETROPLAYER_ACHIEVEMENTS_CHALLENGE_TITLE);
  EXPECT_EQ(infoManager.TranslateString("RetroPlayer.AchievementsChallengeBadge"),
            RETROPLAYER_ACHIEVEMENTS_CHALLENGE_BADGE);
  EXPECT_EQ(infoManager.TranslateString("RetroPlayer.AchievementsProgress"),
            RETROPLAYER_ACHIEVEMENTS_PROGRESS);
  EXPECT_EQ(infoManager.TranslateString("RetroPlayer.AchievementsHardcore"),
            RETROPLAYER_ACHIEVEMENTS_HARDCORE);
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

  const CDateTime firstUnlock{2026, 8, 5, 19, 20, 0};
  const CDateTime laterUnlock{2026, 8, 6, 8, 15, 0};

  bool newlyEarned = false;
  AchievementState state = achievementRuntime.MarkEarned(2, firstUnlock, false, newlyEarned);
  EXPECT_TRUE(newlyEarned);
  EXPECT_EQ(state.unlockedAchievements, 2U);
  EXPECT_EQ(state.achievements[1].unlockedDate, firstUnlock);

  // The achievement runtime re-reports achievements earned in an earlier
  // session, which must not inflate the count or replace the unlock date
  state = achievementRuntime.MarkEarned(2, laterUnlock, false, newlyEarned);
  EXPECT_FALSE(newlyEarned);
  EXPECT_EQ(state.unlockedAchievements, 2U);
  EXPECT_EQ(state.achievements[1].unlockedDate, firstUnlock);

  // An unknown ID must not change anything
  state = achievementRuntime.MarkEarned(99, CDateTime{2026, 8, 7, 12, 0, 0}, false, newlyEarned);
  EXPECT_FALSE(newlyEarned);
  EXPECT_EQ(state.unlockedAchievements, 2U);
}

TEST_F(TestGamesGUIInfo, MarkEarnedInHardcoreAfterCasualIsNew)
{
  CAchievementRuntime achievementRuntime;
  achievementRuntime.SetState(MakeAchievementState());

  const CDateTime casualUnlock{2026, 8, 5, 19, 20, 0};
  const CDateTime hardcoreUnlock{2026, 8, 6, 8, 15, 0};

  bool newlyEarned = false;
  AchievementState state = achievementRuntime.MarkEarned(2, casualUnlock, false, newlyEarned);
  EXPECT_TRUE(newlyEarned);
  EXPECT_FALSE(state.achievements[1].earnedHardcore);

  // RetroAchievements counts the modes apart, so this is a new unlock, but the
  // achievement was already counted as earned
  state = achievementRuntime.MarkEarned(2, hardcoreUnlock, true, newlyEarned);
  EXPECT_TRUE(newlyEarned);
  EXPECT_TRUE(state.achievements[1].earnedHardcore);
  EXPECT_EQ(state.unlockedAchievements, 2U);
  EXPECT_EQ(state.achievements[1].unlockedDate, hardcoreUnlock);

  state = achievementRuntime.MarkEarned(2, CDateTime{2026, 8, 7, 12, 0, 0}, true, newlyEarned);
  EXPECT_FALSE(newlyEarned);
  EXPECT_EQ(state.achievements[1].unlockedDate, hardcoreUnlock);
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

TEST_F(TestGamesGUIInfo, ShowsTheAchievementTheRuntimeIndicated)
{
  CAchievementRuntime achievementRuntime;
  achievementRuntime.SetState(MakeAchievementState());

  AchievementProgressIndicator indicator;
  indicator.id = 3;
  indicator.title = "Collect 180 rings";
  indicator.badgeUrl = "https://example.invalid/badge.png";
  indicator.measuredProgress = "130/180";
  indicator.measuredPercent = 72.0f;
  achievementRuntime.SetProgressIndicator(indicator, true);

  CGamesGUIInfo gamesGUIInfo{achievementRuntime};

  std::string value;
  EXPECT_TRUE(gamesGUIInfo.GetLabel(
      value, nullptr, 0, CGUIInfo(RETROPLAYER_ACHIEVEMENTS_INDICATOR_TITLE), nullptr));
  EXPECT_EQ(value, "Collect 180 rings");

  EXPECT_TRUE(gamesGUIInfo.GetLabel(
      value, nullptr, 0, CGUIInfo(RETROPLAYER_ACHIEVEMENTS_INDICATOR_PROGRESS), nullptr));
  EXPECT_EQ(value, "130/180");

  EXPECT_TRUE(gamesGUIInfo.GetLabel(
      value, nullptr, 0, CGUIInfo(RETROPLAYER_ACHIEVEMENTS_INDICATOR_BADGE), nullptr));
  EXPECT_EQ(value, "https://example.invalid/badge.png");

  int percent = 0;
  EXPECT_TRUE(gamesGUIInfo.GetInt(percent, nullptr, 0,
                                  CGUIInfo(RETROPLAYER_ACHIEVEMENTS_INDICATOR_PERCENT)));
  EXPECT_EQ(percent, 72);
}

TEST_F(TestGamesGUIInfo, ShowsTheAchievementBeingAttempted)
{
  CAchievementRuntime achievementRuntime;
  achievementRuntime.SetState(MakeAchievementState());

  AchievementChallenge indicator;
  indicator.id = 5;
  indicator.title = "Beat Lavos without dying";
  indicator.badgeUrl = "https://example.invalid/badge.png";
  achievementRuntime.SetChallenge(indicator, true);

  CGamesGUIInfo gamesGUIInfo{achievementRuntime};

  std::string value;
  EXPECT_TRUE(gamesGUIInfo.GetLabel(
      value, nullptr, 0, CGUIInfo(RETROPLAYER_ACHIEVEMENTS_CHALLENGE_TITLE), nullptr));
  EXPECT_EQ(value, "Beat Lavos without dying");

  EXPECT_TRUE(gamesGUIInfo.GetLabel(
      value, nullptr, 0, CGUIInfo(RETROPLAYER_ACHIEVEMENTS_CHALLENGE_BADGE), nullptr));
  EXPECT_EQ(value, "https://example.invalid/badge.png");
}

TEST_F(TestGamesGUIInfo, ShowsNothingOnceTheAttemptEnds)
{
  CAchievementRuntime achievementRuntime;
  achievementRuntime.SetState(MakeAchievementState());

  AchievementChallenge indicator;
  indicator.id = 5;
  indicator.title = "Beat Lavos without dying";
  achievementRuntime.SetChallenge(indicator, true);

  // The runtime names the achievement whose attempt ended, which is how the
  // add-on reports it: the challenge is matched by id and removed
  achievementRuntime.SetChallenge(indicator, false);

  CGamesGUIInfo gamesGUIInfo{achievementRuntime};

  std::string value;
  EXPECT_TRUE(gamesGUIInfo.GetLabel(
      value, nullptr, 0, CGUIInfo(RETROPLAYER_ACHIEVEMENTS_CHALLENGE_TITLE), nullptr));
  EXPECT_TRUE(value.empty());
}

TEST_F(TestGamesGUIInfo, ShowsNothingOnceTheIndicatorIsCleared)
{
  CAchievementRuntime achievementRuntime;
  achievementRuntime.SetState(MakeAchievementState());

  AchievementProgressIndicator indicator;
  indicator.id = 3;
  indicator.title = "Collect 180 rings";
  indicator.measuredProgress = "130/180";
  indicator.measuredPercent = 72.0f;
  achievementRuntime.SetProgressIndicator(indicator, true);
  achievementRuntime.SetProgressIndicator({}, false);

  CGamesGUIInfo gamesGUIInfo{achievementRuntime};

  std::string value;
  EXPECT_TRUE(gamesGUIInfo.GetLabel(
      value, nullptr, 0, CGUIInfo(RETROPLAYER_ACHIEVEMENTS_INDICATOR_TITLE), nullptr));
  EXPECT_TRUE(value.empty());

  int percent = -1;
  EXPECT_TRUE(gamesGUIInfo.GetInt(percent, nullptr, 0,
                                  CGUIInfo(RETROPLAYER_ACHIEVEMENTS_INDICATOR_PERCENT)));
  EXPECT_EQ(percent, 0);
}

TEST_F(TestGamesGUIInfo, ShowsTheClosestOfTwoAchievementsCountingAtOnce)
{
  CAchievementRuntime achievementRuntime;
  achievementRuntime.SetState(MakeAchievementState());

  AchievementProgressIndicator behind;
  behind.id = 4;
  behind.title = "Trip Pop Pro";
  behind.measuredProgress = "1/25";
  behind.measuredPercent = 4.0f;

  AchievementProgressIndicator ahead;
  ahead.id = 5;
  ahead.title = "Orange Ace";
  ahead.measuredProgress = "18/20";
  ahead.measuredPercent = 90.0f;

  // Announced separately, as the runtime does, and in the order that would
  // leave the wrong one showing if the latest simply replaced the last
  achievementRuntime.SetProgressIndicator(ahead, true);
  achievementRuntime.SetProgressIndicator(behind, true);

  CGamesGUIInfo gamesGUIInfo{achievementRuntime};

  std::string value;
  EXPECT_TRUE(gamesGUIInfo.GetLabel(
      value, nullptr, 0, CGUIInfo(RETROPLAYER_ACHIEVEMENTS_INDICATOR_TITLE), nullptr));
  EXPECT_EQ(value, "Orange Ace");

  // The one that finished stops counting without taking the other with it
  achievementRuntime.SetProgressIndicator(ahead, false);
  EXPECT_TRUE(gamesGUIInfo.GetLabel(
      value, nullptr, 0, CGUIInfo(RETROPLAYER_ACHIEVEMENTS_INDICATOR_TITLE), nullptr));
  EXPECT_EQ(value, "Trip Pop Pro");
}

TEST_F(TestGamesGUIInfo, AListItemAnswersForItsOwnGame)
{
  //
  // Spec: a row describes the game it holds. RetroPlayer.* deliberately
  // describes the game being played instead, so a listing asks through
  // ListItem.* the way it would for anything else
  //
  CAchievementRuntime achievementRuntime;
  CGamesGUIInfo gamesGUIInfo{achievementRuntime};

  CFileItem item{"/roms/zelda.n64", false};
  item.GetGameInfoTag()->SetOverview("A boy, a sword, and a great deal of walking.");
  item.GetGameInfoTag()->SetGenres({"Adventure", "Action"});
  item.GetGameInfoTag()->SetPublisher("Nintendo");
  item.GetGameInfoTag()->SetYear(1998);

  std::string value;
  EXPECT_TRUE(gamesGUIInfo.GetLabel(value, &item, 0, CGUIInfo(LISTITEM_PLOT), nullptr));
  EXPECT_EQ(value, "A boy, a sword, and a great deal of walking.");

  EXPECT_TRUE(gamesGUIInfo.GetLabel(value, &item, 0, CGUIInfo(LISTITEM_GENRE), nullptr));
  EXPECT_EQ(value, "Adventure, Action");

  EXPECT_TRUE(gamesGUIInfo.GetLabel(value, &item, 0, CGUIInfo(LISTITEM_STUDIO), nullptr));
  EXPECT_EQ(value, "Nintendo");

  EXPECT_TRUE(gamesGUIInfo.GetLabel(value, &item, 0, CGUIInfo(LISTITEM_YEAR), nullptr));
  EXPECT_EQ(value, "1998");
}

TEST_F(TestGamesGUIInfo, ARatingCanBeAskedForBySource)
{
  //
  // Spec: a game keeps every source's rating. ListItem.Rating gives the one
  // to show, and ListItem.Rating(source) gives that source's, both out of 10
  //
  CAchievementRuntime achievementRuntime;
  CGamesGUIInfo gamesGUIInfo{achievementRuntime};

  CFileItem item{"/roms/zelda.n64", false};
  item.GetGameInfoTag()->SetRatings(
      {{"igdb", GameRating{72.0f, 100.0f, 400}}, {"igdbcritic", GameRating{91.0f, 100.0f, 12}}},
      "igdb");

  std::string value;
  EXPECT_TRUE(gamesGUIInfo.GetLabel(value, &item, 0, CGUIInfo(LISTITEM_RATING), nullptr));
  EXPECT_EQ(value, "7.2");

  EXPECT_TRUE(
      gamesGUIInfo.GetLabel(value, &item, 0, CGUIInfo(LISTITEM_RATING, "igdbcritic"), nullptr));
  EXPECT_EQ(value, "9.1");

  EXPECT_TRUE(
      gamesGUIInfo.GetLabel(value, &item, 0, CGUIInfo(LISTITEM_VOTES, "igdbcritic"), nullptr));
  EXPECT_EQ(value, "12");

  EXPECT_FALSE(
      gamesGUIInfo.GetLabel(value, &item, 0, CGUIInfo(LISTITEM_RATING, "screenscraper"), nullptr));
}

TEST_F(TestGamesGUIInfo, AnItemWithNoGameTagIsPassedOn)
{
  //
  // Spec: most of a listing is not games. The question has to be refused so it
  // reaches the provider that can answer it, and asking must not create a tag,
  // since anything carrying one answers CFileItem::IsGame()
  //
  CAchievementRuntime achievementRuntime;
  CGamesGUIInfo gamesGUIInfo{achievementRuntime};

  CFileItem item{"/roms/Some Folder/", true};
  ASSERT_FALSE(item.HasGameInfoTag());

  std::string value{"untouched"};
  EXPECT_FALSE(gamesGUIInfo.GetLabel(value, &item, 0, CGUIInfo(LISTITEM_PLOT), nullptr));
  EXPECT_EQ(value, "untouched");
  EXPECT_FALSE(item.HasGameInfoTag());
}

TEST_F(TestGamesGUIInfo, AnEmptyFieldIsPassedOn)
{
  //
  // Spec: a game tag carrying nothing for a field must not claim the label,
  // or a row would show blank where another provider had something to say
  //
  CAchievementRuntime achievementRuntime;
  CGamesGUIInfo gamesGUIInfo{achievementRuntime};

  CFileItem item{"/roms/zelda.n64", false};
  item.GetGameInfoTag()->SetTitle("Has a title, nothing else");

  std::string value;
  EXPECT_FALSE(gamesGUIInfo.GetLabel(value, &item, 0, CGUIInfo(LISTITEM_PLOT), nullptr));
  EXPECT_FALSE(gamesGUIInfo.GetLabel(value, &item, 0, CGUIInfo(LISTITEM_GENRE), nullptr));
  EXPECT_FALSE(gamesGUIInfo.GetLabel(value, &item, 0, CGUIInfo(LISTITEM_YEAR), nullptr));
  EXPECT_FALSE(gamesGUIInfo.GetLabel(value, &item, 0, CGUIInfo(LISTITEM_STUDIO), nullptr));
}

TEST_F(TestGamesGUIInfo, AnswersListItemLabelsFromTheGameTag)
{
  CFileItem item{"/roms/test.rom", false};
  CGameInfoTag* tag = item.GetGameInfoTag();
  tag->SetTitle("Chrono Trigger");
  tag->SetYear(1995);
  tag->SetGenres({"Role-playing", "Adventure"});
  tag->SetOverview("A band of adventurers travels through time.");

  CGUIInfoManager infoManager;

  EXPECT_EQ(infoManager.GetItemLabel(&item, 0, LISTITEM_TITLE), "Chrono Trigger");
  EXPECT_EQ(infoManager.GetItemLabel(&item, 0, LISTITEM_YEAR), "1995");
  EXPECT_EQ(infoManager.GetItemLabel(&item, 0, LISTITEM_GENRE), "Role-playing, Adventure");
  EXPECT_EQ(infoManager.GetItemLabel(&item, 0, LISTITEM_PLOT),
            "A band of adventurers travels through time.");
}

TEST_F(TestGamesGUIInfo, AnswersListItemLabelsForAGamesMakers)
{
  CFileItem item{"/roms/test.rom", false};
  CGameInfoTag* tag = item.GetGameInfoTag();
  tag->SetPlatform("Super Nintendo");
  tag->SetDeveloper("Square");
  tag->SetPublisher("Square");

  CGUIInfoManager infoManager;

  EXPECT_EQ(infoManager.GetItemLabel(&item, 0, infoManager.TranslateString("ListItem.Platform")),
            "Super Nintendo");
  EXPECT_EQ(infoManager.GetItemLabel(&item, 0, infoManager.TranslateString("ListItem.Developer")),
            "Square");
  EXPECT_EQ(infoManager.GetItemLabel(&item, 0, infoManager.TranslateString("ListItem.Publisher")),
            "Square");
}

TEST_F(TestGamesGUIInfo, AnswersListItemLabelsForAGamesWiderDetails)
{
  CFileItem item{"/roms/test.rom", false};
  CGameInfoTag* tag = item.GetGameInfoTag();
  tag->SetOriginalTitle("Seiken Densetsu 2");
  tag->SetRating(8.5f);
  tag->SetPlayers(3);
  tag->SetAgeRating("PEGI 7");
  tag->SetTrailer("https://example.com/trailer.mp4");
  tag->SetTags({"Co-op", "JRPG"});

  CGUIInfoManager infoManager;

  EXPECT_EQ(infoManager.GetItemLabel(&item, 0, LISTITEM_ORIGINALTITLE), "Seiken Densetsu 2");
  EXPECT_EQ(infoManager.GetItemLabel(&item, 0, LISTITEM_RATING), "8.5");
  EXPECT_EQ(infoManager.GetItemLabel(&item, 0, infoManager.TranslateString("ListItem.Players")),
            "3");
  EXPECT_EQ(infoManager.GetItemLabel(&item, 0, infoManager.TranslateString("ListItem.AgeRating")),
            "PEGI 7");
  EXPECT_EQ(infoManager.GetItemLabel(&item, 0, LISTITEM_TRAILER),
            "https://example.com/trailer.mp4");
  EXPECT_EQ(infoManager.GetItemLabel(&item, 0, LISTITEM_TAG), "Co-op, JRPG");
}

TEST_F(TestGamesGUIInfo, JoinsGenresWithTheSkinsSeparator)
{
  CFileItem item{"/roms/test.rom", false};
  item.GetGameInfoTag()->SetGenres({"Role-playing", "Adventure"});

  CGamesGUIInfo gamesGUIInfo;
  std::string value;

  EXPECT_TRUE(gamesGUIInfo.GetLabel(value, &item, 0, CGUIInfo(LISTITEM_GENRE, " / ", 0), nullptr));
  EXPECT_EQ(value, "Role-playing / Adventure");
}

TEST_F(TestGamesGUIInfo, LeavesItemsWithoutAGameTagAlone)
{
  CFileItem item{"/videos/test.mkv", false};

  CGamesGUIInfo gamesGUIInfo;
  std::string value;

  EXPECT_FALSE(gamesGUIInfo.GetLabel(value, &item, 0, CGUIInfo(LISTITEM_TITLE), nullptr));
  EXPECT_FALSE(gamesGUIInfo.GetLabel(value, &item, 0, CGUIInfo(LISTITEM_PLATFORM), nullptr));
  EXPECT_FALSE(item.HasGameInfoTag());
}
