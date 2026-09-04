/*
 *  Copyright (C) 2012-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "guilib/guiinfo/GamesGUIInfo.h"

#include "FileItem.h"
#include "XBDateTime.h"
#include "GUIInfoManager.h"
#include "ServiceBroker.h"
#include "Util.h"
#include "addons/AddonManager.h"
#include "addons/IAddon.h"
#include "addons/addoninfo/AddonType.h"
#include "application/ApplicationComponents.h"
#include "application/ApplicationPlayer.h"
#include "cores/RetroPlayer/RetroPlayerUtils.h"
#include "games/AchievementRuntime.h"
#include "games/GameServices.h"
#include "games/cheats/CheatRuntime.h"
#include "games/GameSettings.h"
#include "games/addons/GameClient.h"
#include "games/library/GameLibraryTypes.h"
#include "games/tags/GameInfoTag.h"
#include "guilib/GUIComponent.h"
#include "guilib/guiinfo/GUIInfo.h"
#include "guilib/guiinfo/GUIInfoLabels.h"
#include "settings/MediaSettings.h"
#include "settings/Settings.h"
#include "settings/SettingsComponent.h"
#include "utils/StringUtils.h"
#include "utils/URIUtils.h"
#include "utils/Variant.h"
#include "utils/log.h"

using namespace KODI::GUILIB::GUIINFO;
using namespace KODI::GAME;
using namespace KODI::RETRO;

namespace
{
/*!
 * \brief Helper to get the currently-playing game tag
 *
 * This bypasses the global application player by using GUIInfoManager instead.
 *
 * The currently-playing item flow:
 *
 *   - Arrives via play command
 *   - Sent to the application player, which copies the item for itself
 *   - Sent to the GUIInfoManager via GUI_MSG_PLAYBACK_STARTED from the player
 *     calling CApplicationPlayerCallback::OnPlayBackStarted(). Also copies any
 *     updates back to the player.
 *   - The item can be updated at runtime via TMSG_UPDATE_PLAYER_ITEM, which
 *     updates the application's item silently and GUIInfoManager directly
 */
const CGameInfoTag* GetGUIGameTag()
{
  // Use const access because infotags can accidentally be created
  // when querying a tag that isn't set, which can completely break
  // all downstream is-video/audio/game checks
  if (const auto* gui = CServiceBroker::GetGUIConst(); gui != nullptr)
    return gui->GetInfoManager().GetCurrentGameTag();

  return nullptr;
}
} // namespace

//! @todo Savestates were removed from v18
//#define FILEITEM_PROPERTY_SAVESTATE_DURATION  "duration"

const CAchievementRuntime& CGamesGUIInfo::AchievementRuntime() const
{
  if (m_achievementRuntime)
    return *m_achievementRuntime;

  return CServiceBroker::GetGameServices().AchievementRuntime();
}

bool CGamesGUIInfo::ShowIndicators()
{
  const auto settingsComponent = CServiceBroker::GetSettingsComponent();
  if (settingsComponent == nullptr)
    return true;

  const auto settings = settingsComponent->GetSettings();
  if (settings == nullptr)
    return true;

  // Answered here rather than recorded when the runtime reports one, so turning
  // it off during an attempt clears the screen at once and the achievements
  // dialog still knows what is being attempted
  return settings->GetBool(SETTING_GAMES_ACHIEVEMENTS_ONSCREEN_INDICATORS);
}

bool CGamesGUIInfo::InitCurrentItem(CFileItem* item)
{
  if (item && item->IsGame())
  {
    CLog::Log(LOGDEBUG, "CGamesGUIInfo::InitCurrentItem({})", item->GetPath());

    item->LoadGameTag();
    CGameInfoTag* tag =
        item->GetGameInfoTag(); // creates item if not yet set, so no nullptr checks needed

    if (tag->GetTitle().empty())
    {
      // No title in tag, derive one from the item path
      std::string title = CUtil::GetTitleFromPath(item->GetPath(), item->IsFolder());
      if (!title.empty())
        tag->SetTitle(title);
    }

    return true;
  }

  return false;
}

bool CGamesGUIInfo::GetLabel(std::string& value,
                             const CFileItem* item,
                             int contextWindow,
                             const CGUIInfo& info,
                             std::string* fallback) const
{
  switch (info.GetInfo())
  {
    ///////////////////////////////////////////////////////////////////////////////////////////////
    // RETROPLAYER_*
    ///////////////////////////////////////////////////////////////////////////////////////////////
    case RETROPLAYER_VIDEO_FILTER:
    {
      value = CMediaSettings::GetInstance().GetCurrentGameSettings().VideoFilter();
      return true;
    }
    case RETROPLAYER_STRETCH_MODE:
    {
      STRETCHMODE stretchMode =
          CMediaSettings::GetInstance().GetCurrentGameSettings().StretchMode();
      value = CRetroPlayerUtils::StretchModeToIdentifier(stretchMode);
      return true;
    }
    case RETROPLAYER_VIDEO_ROTATION:
    {
      const unsigned int rotationDegCCW =
          CMediaSettings::GetInstance().GetCurrentGameSettings().RotationDegCCW();
      value = std::to_string(rotationDegCCW);
      return true;
    }
    ///////////////////////////////////////////////////////////////////////////////////////////////
    // LISTITEM_*
    //
    // A row in a listing, answered from the item's own tag rather than from the
    // game being played. RetroPlayer.* deliberately describes the latter, the
    // way VideoPlayer.* does, so a browsing skin asks through ListItem.* as it
    // would for anything else.
    //
    // Refused where the item carries no game tag, so the question passes to the
    // provider that can answer it.
    ///////////////////////////////////////////////////////////////////////////////////////////////
    case LISTITEM_TITLE:
    case LISTITEM_PLOT:
    case LISTITEM_GENRE:
    case LISTITEM_YEAR:
    case LISTITEM_STUDIO:
    case LISTITEM_ORIGINALTITLE:
    case LISTITEM_RATING:
    case LISTITEM_VOTES:
    case LISTITEM_USER_RATING:
    case LISTITEM_PLAYCOUNT:
    case LISTITEM_LASTPLAYED:
    case LISTITEM_DATE_ADDED:
    case LISTITEM_MPAA:
    case LISTITEM_SET:
    case LISTITEM_TAG:
    case LISTITEM_TRAILER:
    case LISTITEM_PREMIERED:
    case LISTITEM_PLATFORM:
    case LISTITEM_DEVELOPER:
    case LISTITEM_PUBLISHER:
    case LISTITEM_PLAYERS:
    case LISTITEM_REGION:
    case LISTITEM_RELEASE_COUNT:
    case LISTITEM_ACHIEVEMENTS_TOTAL:
    case LISTITEM_ACHIEVEMENTS_EARNED:
    case LISTITEM_ACHIEVEMENTS_PERCENT:
    case LISTITEM_ACHIEVEMENTS_PROGRESS:
    case LISTITEM_GAME_CATEGORY:
    case LISTITEM_GAME_CLIENT:
    {
      // Const access, so asking does not create a tag on an item that has none
      if (item == nullptr || !item->HasGameInfoTag())
        break;

      const CGameInfoTag* tag = item->GetGameInfoTag();

      switch (info.GetInfo())
      {
        case LISTITEM_TITLE:
          value = tag->GetTitle();
          return !value.empty();
        case LISTITEM_PLOT:
          value = tag->GetOverview();
          return !value.empty();
        case LISTITEM_GENRE:
          value = StringUtils::Join(tag->GetGenres(), ", ");
          return !value.empty();
        case LISTITEM_PREMIERED:
        {
          // The day it was sold where the library knows it, the year otherwise
          CDateTime date;
          date.SetFromDBDate(tag->GetReleaseDate());
          if (date.IsValid())
          {
            value = date.GetAsLocalizedDate();
            return true;
          }
          if (tag->GetYear() > 0)
          {
            value = std::to_string(tag->GetYear());
            return true;
          }
          break;
        }
        case LISTITEM_YEAR:
          if (tag->GetYear() > 0)
          {
            value = std::to_string(tag->GetYear());
            return true;
          }
          break;
        case LISTITEM_STUDIO:
        case LISTITEM_PUBLISHER:
          value = tag->GetPublishers().empty() ? tag->GetPublisher()
                                               : StringUtils::Join(tag->GetPublishers(), ", ");
          return !value.empty();
        case LISTITEM_DEVELOPER:
          value = tag->GetDevelopers().empty() ? tag->GetDeveloper()
                                               : StringUtils::Join(tag->GetDevelopers(), ", ");
          return !value.empty();
        case LISTITEM_ORIGINALTITLE:
          value = tag->GetOriginalTitle();
          return !value.empty();
        case LISTITEM_RATING:
          if (tag->GetRating().rating > 0.0f)
          {
            value = StringUtils::FormatNumber(tag->GetRating().rating);
            return true;
          }
          break;
        case LISTITEM_VOTES:
          if (tag->GetRating().votes > 0)
          {
            value = std::to_string(tag->GetRating().votes);
            return true;
          }
          break;
        case LISTITEM_USER_RATING:
          if (tag->GetUserRating() > 0)
          {
            value = std::to_string(tag->GetUserRating());
            return true;
          }
          break;
        case LISTITEM_PLAYCOUNT:
          if (tag->GetPlayCount() > 0)
          {
            value = std::to_string(tag->GetPlayCount());
            return true;
          }
          break;
        case LISTITEM_LASTPLAYED:
        case LISTITEM_DATE_ADDED:
        {
          CDateTime date;
          date.SetFromDBDateTime(info.GetInfo() == LISTITEM_LASTPLAYED ? tag->GetLastPlayed()
                                                                        : tag->GetDateAdded());
          if (date.IsValid())
          {
            value = date.GetAsLocalizedDate();
            return true;
          }
          break;
        }
        case LISTITEM_MPAA:
          value = tag->GetAgeRating();
          return !value.empty();
        case LISTITEM_SET:
          value = tag->GetCollections().empty() ? "" : tag->GetCollections().front();
          return !value.empty();
        case LISTITEM_TAG:
          value = StringUtils::Join(tag->GetTags(), ", ");
          return !value.empty();
        case LISTITEM_TRAILER:
          value = tag->GetTrailer();
          return !value.empty();
        case LISTITEM_PLATFORM:
          value = tag->GetPlatform();
          return !value.empty();
        case LISTITEM_PLAYERS:
          if (tag->GetPlayersMax() > 0)
          {
            value = tag->GetPlayersMin() > 0 && tag->GetPlayersMin() < tag->GetPlayersMax()
                        ? StringUtils::Format("{}-{}", tag->GetPlayersMin(), tag->GetPlayersMax())
                        : std::to_string(tag->GetPlayersMax());
            return true;
          }
          break;
        case LISTITEM_REGION:
          // Stored as a list; read as a sentence
          value = StringUtils::Join(StringUtils::Split(tag->GetRegion(), ","), ", ");
          return !value.empty();
        case LISTITEM_RELEASE_COUNT:
          if (tag->GetReleaseCount() > 0)
          {
            value = std::to_string(tag->GetReleaseCount());
            return true;
          }
          break;
        case LISTITEM_ACHIEVEMENTS_TOTAL:
          if (tag->HasAchievements())
          {
            value = std::to_string(tag->GetAchievementsTotal());
            return true;
          }
          break;
        case LISTITEM_ACHIEVEMENTS_EARNED:
          if (tag->HasAchievements())
          {
            value = std::to_string(tag->GetAchievementsEarned());
            return true;
          }
          break;
        case LISTITEM_ACHIEVEMENTS_PROGRESS:
          // How much of the set is earned, or how big it is where none is
          if (tag->HasAchievements())
          {
            value = tag->GetAchievementsEarned() > 0
                        ? StringUtils::Format("{} / {}", tag->GetAchievementsEarned(),
                                              tag->GetAchievementsTotal())
                        : std::to_string(tag->GetAchievementsTotal());
            return true;
          }
          break;
        case LISTITEM_ACHIEVEMENTS_PERCENT:
          // What a profile on the service shows: how much of the set is earned
          if (tag->GetAchievementsTotal() > 0 && tag->GetAchievementsEarned() > 0)
          {
            value = std::to_string(100 * tag->GetAchievementsEarned() / tag->GetAchievementsTotal());
            return true;
          }
          break;
        case LISTITEM_GAME_CATEGORY:
          value = std::string(CGameLibraryTypes::ToString(tag->GetCategory()));
          return !value.empty();
        case LISTITEM_GAME_CLIENT:
          value = tag->GetGameClient();
          return !value.empty();
        default:
          break;
      }
      break;
    }
    case RETROPLAYER_TITLE:
    {
      if (const auto* tag = GetGUIGameTag(); tag != nullptr)
      {
        value = tag->GetTitle();
        return true;
      }
      break;
    }
    case RETROPLAYER_PLATFORM:
    {
      if (const auto* tag = GetGUIGameTag(); tag != nullptr)
      {
        value = tag->GetPlatform();
        return true;
      }
      break;
    }
    case RETROPLAYER_GENRES:
    {
      if (const auto* tag = GetGUIGameTag(); tag != nullptr)
      {
        value = StringUtils::Join(tag->GetGenres(), ", ");
        return true;
      }
      break;
    }
    case RETROPLAYER_PUBLISHER:
    {
      if (const auto* tag = GetGUIGameTag(); tag != nullptr)
      {
        value = tag->GetPublisher();
        return true;
      }
      break;
    }
    case RETROPLAYER_DEVELOPER:
    {
      if (const auto* tag = GetGUIGameTag(); tag != nullptr)
      {
        value = tag->GetDeveloper();
        return true;
      }
      break;
    }
    case RETROPLAYER_OVERVIEW:
    {
      if (const auto* tag = GetGUIGameTag(); tag != nullptr)
      {
        value = tag->GetOverview();
        return true;
      }
      break;
    }
    case RETROPLAYER_GAME_CLIENT:
    {
      if (const auto* tag = GetGUIGameTag(); tag != nullptr)
      {
        value = tag->GetGameClient();
        return true;
      }
      break;
    }
    case RETROPLAYER_GAME_CLIENT_NAME:
    {
      if (const auto* tag = GetGUIGameTag(); tag != nullptr)
      {
        ADDON::AddonPtr addon;
        if (CServiceBroker::GetAddonMgr().GetAddon(tag->GetGameClient(), addon,
                                                   ADDON::AddonType::GAMEDLL,
                                                   ADDON::OnlyEnabled::CHOICE_YES))
        {
          value = std::static_pointer_cast<CGameClient>(addon)->GetEmulatorName();
          return true;
        }
      }
      break;
    }
    case RETROPLAYER_GAME_CLIENT_PLATFORMS:
    {
      if (const auto* tag = GetGUIGameTag(); tag != nullptr)
      {
        ADDON::AddonPtr addon;
        if (CServiceBroker::GetAddonMgr().GetAddon(tag->GetGameClient(), addon,
                                                   ADDON::AddonType::GAMEDLL,
                                                   ADDON::OnlyEnabled::CHOICE_YES))
        {
          value = std::static_pointer_cast<CGameClient>(addon)->GetPlatforms();
          return true;
        }
      }
      break;
    }
    case RETROPLAYER_DISC_LABEL:
    {
      const auto& components = CServiceBroker::GetAppComponents();
      const auto appPlayer = components.GetComponent<CApplicationPlayer>();

      if (appPlayer)
        value = appPlayer->DiscLabel();

      return true;
    }
    case RETROPLAYER_RICH_PRESENCE:
    {
      value = AchievementRuntime().GetRichPresence();
      return true;
    }
    case RETROPLAYER_ACHIEVEMENTS_CHALLENGE_TITLE:
    {
      value = ShowIndicators() ? AchievementRuntime().GetChallengeAchievementTitle() : "";
      return true;
    }
    case RETROPLAYER_ACHIEVEMENTS_CHALLENGE_BADGE:
    {
      value = ShowIndicators() ? AchievementRuntime().GetChallengeAchievementBadge() : "";
      return true;
    }
    case RETROPLAYER_ACHIEVEMENTS_INDICATOR_TITLE:
    {
      value = ShowIndicators() ? AchievementRuntime().GetTrackedAchievementTitle() : "";
      return true;
    }
    case RETROPLAYER_ACHIEVEMENTS_INDICATOR_PROGRESS:
    {
      value = ShowIndicators() ? AchievementRuntime().GetTrackedAchievementProgress() : "";
      return true;
    }
    case RETROPLAYER_ACHIEVEMENTS_INDICATOR_BADGE:
    {
      value = ShowIndicators() ? AchievementRuntime().GetTrackedAchievementBadge() : "";
      return true;
    }
    case RETROPLAYER_ACHIEVEMENTS_PROGRESS:
    {
      const CAchievementRuntime& runtime = AchievementRuntime();
      const unsigned int total = runtime.GetTotalAchievements();

      // An empty label lets skins hide the control instead of showing "0 / 0"
      if (total == 0)
        value.clear();
      else
        value = StringUtils::Format("{} / {}", runtime.GetUnlockedAchievements(), total);

      return true;
    }
    case RETROPLAYER_ACHIEVEMENTS_CHALLENGE_TITLE:
    case RETROPLAYER_ACHIEVEMENTS_CHALLENGE_BADGE:
    {
      // Answered empty rather than never recorded, so that turning it off while
      // an attempt is live takes the indicator off screen at once
      if (!CServiceBroker::GetGameServices().GameSettings().GetChallengeIndicator())
      {
        value.clear();
        return true;
      }

      // Only the first attempt is surfaced. More than one at a time is rare,
      // and a corner indicator has room for one.
      const std::vector<AchievementChallenge> challenges = AchievementRuntime().GetChallenges();
      if (challenges.empty())
      {
        value.clear();
        return true;
      }

      value = (info.GetInfo() == RETROPLAYER_ACHIEVEMENTS_CHALLENGE_TITLE)
                  ? challenges.front().title
                  : challenges.front().badgeUrl;
      return true;
    }
    case RETROPLAYER_LEADERBOARD_TRACKER:
    {
      // First only, as with the challenge indicator above
      const std::vector<LeaderboardTracker> trackers =
          AchievementRuntime().GetLeaderboardTrackers();

      value = trackers.empty() ? "" : trackers.front().display;
      return true;
    }
    case RETROPLAYER_ACHIEVEMENTS_INDICATOR_TITLE:
    case RETROPLAYER_ACHIEVEMENTS_INDICATOR_BADGE:
    case RETROPLAYER_ACHIEVEMENTS_INDICATOR_PROGRESS:
    case RETROPLAYER_ACHIEVEMENTS_INDICATOR_PERCENT:
    {
      const AchievementProgressIndicator indicator = AchievementRuntime().GetProgressIndicator();

      // The title being empty is what keeps the whole indicator hidden, so
      // every field of an inactive one must come back empty rather than "0"
      if (indicator.id == 0)
      {
        value.clear();
        return true;
      }

      switch (info.GetInfo())
      {
        case RETROPLAYER_ACHIEVEMENTS_INDICATOR_TITLE:
          value = indicator.title;
          break;
        case RETROPLAYER_ACHIEVEMENTS_INDICATOR_BADGE:
          value = indicator.badgeUrl;
          break;
        case RETROPLAYER_ACHIEVEMENTS_INDICATOR_PROGRESS:
          value = indicator.measuredProgress;
          break;
        default:
          value = std::to_string(static_cast<int>(indicator.measuredPercent));
          break;
      }
      return true;
    }
    default:
      break;
  }

  return false;
}

bool CGamesGUIInfo::GetInt(int& value,
                           const CGUIListItem* gitem,
                           int contextWindow,
                           const CGUIInfo& info) const
{
  switch (info.GetInfo())
  {
    ///////////////////////////////////////////////////////////////////////////////////////////////
    // RETROPLAYER_*
    ///////////////////////////////////////////////////////////////////////////////////////////////
    case RETROPLAYER_ACHIEVEMENTS_INDICATOR_PERCENT:
    {
      // Answered here as well as on GetLabel so a progress control can be bound
      // to it directly
      value = static_cast<int>(AchievementRuntime().GetTrackedAchievementPercent());
      return true;
    }
    default:
      break;
  }

  return false;
}

bool CGamesGUIInfo::GetBool(bool& value,
                            const CGUIListItem* gitem,
                            int contextWindow,
                            const CGUIInfo& info) const
{
  switch (info.GetInfo())
  {
    ///////////////////////////////////////////////////////////////////////////////////////////////
    // RETROPLAYER_*
    ///////////////////////////////////////////////////////////////////////////////////////////////
    case RETROPLAYER_SUPPORTS_EJECT:
    {
      const auto& components = CServiceBroker::GetAppComponents();
      const auto appPlayer = components.GetComponent<CApplicationPlayer>();

      value = appPlayer && appPlayer->SupportsDiscControl();

      return true;
    }
    case RETROPLAYER_DISC_EJECTED:
    {
      const auto& components = CServiceBroker::GetAppComponents();
      const auto appPlayer = components.GetComponent<CApplicationPlayer>();

      value = appPlayer && appPlayer->IsDiscEjected();

      return true;
    }
    case RETROPLAYER_EMPTY_TRAY:
    {
      const auto& components = CServiceBroker::GetAppComponents();
      const auto appPlayer = components.GetComponent<CApplicationPlayer>();

      value = appPlayer && appPlayer->IsTrayEmpty();

      return true;
    }
    case RETROPLAYER_ACHIEVEMENTS_LOGGED_IN:
    {
      value = CServiceBroker::GetGameServices().GameSettings().GetAchievementsLoggedIn();
      return true;
    }
    case RETROPLAYER_HAS_CHEATS:
    {
      value = CServiceBroker::GetGameServices().CheatRuntime().HasCheats();
      return true;
    }
    case RETROPLAYER_ACHIEVEMENTS_HARDCORE:
    {
      value = CServiceBroker::GetGameServices().GameSettings().GetAchievementsHardcore();
      return true;
    }
    default:
      break;
  }

  return false;
}
