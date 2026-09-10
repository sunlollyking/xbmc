/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "DialogGameAchievements.h"

#include "FileItem.h"
#include "FileItemList.h"
#include "ServiceBroker.h"
#include "URL.h"
#include "application/ApplicationComponents.h"
#include "application/ApplicationPlayer.h"
#include "cores/RetroPlayer/guibridge/GUIGameRenderManager.h"
#include "cores/RetroPlayer/guibridge/GUIGameSettingsHandle.h"
#include "dialogs/GUIDialogKaiToast.h"
#include "filesystem/CurlFile.h"
#include "dialogs/GUIDialogYesNo.h"
#include "games/AchievementRuntime.h"
#include "games/GameServices.h"
#include "games/GameSettings.h"
#include "games/dialogs/DialogGameDefines.h"
#include "games/tags/GameInfoTag.h"
#include "guilib/GUIComponent.h"
#include "guilib/GUIMessage.h"
#include "guilib/GUIWindowManager.h"
#include "guilib/WindowIDs.h"
#include "guilib/guiinfo/GUIInfoLabels.h"
#include "resources/LocalizeStrings.h"
#include "resources/ResourcesComponent.h"
#include "settings/Settings.h"
#include "settings/SettingsComponent.h"
#include "utils/JSONVariantParser.h"
#include "utils/StringUtils.h"
#include "utils/Variant.h"
#include "utils/log.h"
#include "view/GUIViewControl.h"
#include "view/ViewState.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

using namespace KODI;
using namespace GAME;

namespace
{
//! The set and the player's standing in it, in one answer
constexpr const char* GAME_PROGRESS_URL =
    "https://retroachievements.org/API/API_GetGameInfoAndUserProgress.php?g={}&u={}&y={}";

//! Where RetroAchievements serves achievement badges from
constexpr const char* BADGE_URL = "https://media.retroachievements.org/Badge/{}.png";
constexpr const char* LOCKED_BADGE_URL = "https://media.retroachievements.org/Badge/{}_lock.png";

constexpr unsigned int REQUEST_TIMEOUT_SECS = 10;

/*!
 * \brief Fetches one game's achievements and the player's progress, off the GUI thread
 */
class CLibraryAchievementsJob : public CJob
{
public:
  CLibraryAchievementsJob(std::string gameId, std::string username, std::string apiKey)
    : m_gameId(std::move(gameId)), m_username(std::move(username)), m_apiKey(std::move(apiKey))
  {
  }

  const char* GetType() const override { return "library-achievements"; }

  bool DoWork() override
  {
    const std::string url = StringUtils::Format(GAME_PROGRESS_URL, CURL::Encode(m_gameId),
                                                CURL::Encode(m_username), CURL::Encode(m_apiKey));

    XFILE::CCurlFile curl;
    curl.SetTimeout(REQUEST_TIMEOUT_SECS);

    std::string response;
    if (!curl.Get(url, response))
    {
      CLog::Log(LOGERROR, "CDialogGameAchievements: no answer for game {}", m_gameId);
      return false;
    }

    CVariant data;
    if (!CJSONVariantParser::Parse(response, data) || !data.isObject())
    {
      CLog::Log(LOGERROR, "CDialogGameAchievements: game {} answered {} bytes that are not an object",
                m_gameId, response.size());
      return false;
    }

    m_state.gameTitle = data["Title"].asString();
    m_state.gameId = static_cast<unsigned int>(data["ID"].asUnsignedInteger());

    const CVariant& achievements = data["Achievements"];
    for (auto it = achievements.begin_map(); it != achievements.end_map(); ++it)
    {
      const CVariant& row = it->second;

      AchievementInfo info;
      info.id = static_cast<unsigned int>(row["ID"].asUnsignedInteger());
      info.title = row["Title"].asString();
      info.description = row["Description"].asString();
      info.points = static_cast<unsigned int>(row["Points"].asUnsignedInteger());

      const std::string badge = row["BadgeName"].asString();
      if (!badge.empty())
      {
        info.badgeUrl = StringUtils::Format(BADGE_URL, badge);
        info.lockedBadgeUrl = StringUtils::Format(LOCKED_BADGE_URL, badge);
      }

      // Earned at all, in either mode; the date is the softcore one where both
      // exist, which is when the achievement was first met.
      const std::string earnedDate = row["DateEarned"].asString();
      const std::string hardcoreDate = row["DateEarnedHardcore"].asString();
      info.earned = !earnedDate.empty() || !hardcoreDate.empty();
      if (info.earned)
        info.unlockedDate.SetFromDBDateTime(earnedDate.empty() ? hardcoreDate : earnedDate);

      // Rarity is published as the count of players who have it against the
      // count who have played the game at all.
      const auto awarded = static_cast<double>(row["NumAwarded"].asUnsignedInteger());
      const auto players = static_cast<double>(data["NumDistinctPlayers"].asUnsignedInteger());
      if (awarded > 0.0 && players > 0.0)
        info.rarity = static_cast<float>(100.0 * awarded / players);

      if (info.earned)
        ++m_state.unlockedAchievements;
      ++m_state.totalAchievements;

      m_state.achievements.push_back(std::move(info));
    }

    m_state.loaded = true;
    CLog::Log(LOGINFO, "CDialogGameAchievements: game {} has {} achievements, {} earned",
              m_gameId, m_state.totalAchievements, m_state.unlockedAchievements);
    return true;
  }

  const AchievementState& GetState() const { return m_state; }

private:
  const std::string m_gameId;
  const std::string m_username;
  const std::string m_apiKey;
  AchievementState m_state;
};

constexpr unsigned int TOAST_DISPLAY_TIME_MS = 6000;
constexpr unsigned int TOAST_MESSAGE_TIME_MS = 500;

// Rarity thresholds, as a percentage of players who have earned it
constexpr float RARITY_COMMON = 50.0f;
constexpr float RARITY_UNCOMMON = 10.0f;
constexpr float RARITY_RARE = 2.0f;

std::string Localize(uint32_t stringId)
{
  return CServiceBroker::GetResourcesComponent().GetLocalizeStrings().Get(stringId);
}

/*!
 * \brief Translate a rarity percentage into a localized category
 *
 * \return The category, or an empty string if the rarity is unknown
 */
std::string RarityCategory(float rarity)
{
  if (rarity <= 0.0f)
    return {};

  if (rarity > RARITY_COMMON)
    return Localize(35290); // "Common"
  if (rarity > RARITY_UNCOMMON)
    return Localize(35291); // "Uncommon"
  if (rarity > RARITY_RARE)
    return Localize(35292); // "Rare"

  return Localize(35293); // "Ultra rare"
}

/*!
 * \brief Render a rarity percentage as one to four stars
 *
 * Kept apart from the category name so that the name stays translatable and
 * the stars stay out of the translated strings.
 *
 * \return The stars, or an empty string if the rarity is unknown
 */
std::string RarityStars(float rarity)
{
  if (rarity <= 0.0f)
    return {};

  if (rarity > RARITY_COMMON)
    return "★";
  if (rarity > RARITY_UNCOMMON)
    return "★★";
  if (rarity > RARITY_RARE)
    return "★★★";

  return "★★★★";
}
} // namespace

CDialogGameAchievements::CDialogGameAchievements()
  : CGUIDialog(WINDOW_DIALOG_GAME_ACHIEVEMENTS, "DialogGameControllers.xml"),
    CJobQueue(false, 1),
    m_items(std::make_unique<CFileItemList>()),
    m_viewControl(std::make_unique<CGUIViewControl>())
{
}

CDialogGameAchievements::~CDialogGameAchievements() = default;

void CDialogGameAchievements::OnWindowLoaded()
{
  CGUIDialog::OnWindowLoaded();

  m_viewControl->Reset();
  m_viewControl->SetParentWindow(GetID());
  m_viewControl->AddView(GetControl(CONTROL_CHEEVOS_LIST));
}

void CDialogGameAchievements::OnWindowUnload()
{
  m_viewControl->Reset();

  CGUIDialog::OnWindowUnload();
}

void CDialogGameAchievements::OnInitWindow()
{
  const CGameSettings& gameSettings = CServiceBroker::GetGameServices().GameSettings();
  const auto appPlayer = CServiceBroker::GetAppComponents().GetComponent<CApplicationPlayer>();
  if (!gameSettings.GetAchievementsLoggedIn() || !appPlayer->IsPlayingGame())
  {
    CGUIDialog::OnInitWindow();
    return;
  }

  m_fetched.reset();

  AchievementState state = CServiceBroker::GetGameServices().AchievementRuntime().GetState();

  // Nothing is playing, so the list is being opened from the library. Ask the
  // service about the game on screen; the answer arrives in OnJobComplete.
  if (!state.loaded && FetchForLibraryGame())
  {
    m_viewControl->SetCurrentView(DEFAULT_VIEW_LIST);
    CGUIDialog::OnInitWindow();
    return;
  }

  // Identification is a network round trip, so a game opened moments ago has
  // not been answered for yet. Saying it has no achievements would be a guess,
  // and usually the wrong one.
  if (!state.loaded)
  {
    // "RetroAchievements", "Still looking this game up on RetroAchievements..."
    CGUIDialogKaiToast::QueueNotification(
        CServiceBroker::GetGameServices().GameSettings().GetRAUserPicUrl(), Localize(35264),
        Localize(35299), TOAST_DISPLAY_TIME_MS, false, TOAST_MESSAGE_TIME_MS);
    Abort();
    return;
  }

  if (state.achievements.empty())
  {
    // "RetroAchievements", "This game doesn't support RetroAchievements"
    CGUIDialogKaiToast::QueueNotification(
        CServiceBroker::GetGameServices().GameSettings().GetRAUserPicUrl(), Localize(35264),
        Localize(35286), TOAST_DISPLAY_TIME_MS, false, TOAST_MESSAGE_TIME_MS);
    Abort();
    return;
  }

  // Before the base class, which focuses the list and can't focus an empty one
  m_viewControl->SetCurrentView(DEFAULT_VIEW_LIST);
  RefreshList();

  CGUIDialog::OnInitWindow();
}

void CDialogGameAchievements::Abort()
{
  // Forced: Open() makes the dialog active before OnInitWindow() runs, so a
  // plain Close() would animate out and flash
  Close(true);
}

void CDialogGameAchievements::OnDeinitWindow(int nextWindowID)
{
  m_viewControl->Clear();
  m_items->Clear();

  CGUIDialog::OnDeinitWindow(nextWindowID);
}

bool CDialogGameAchievements::OnMessage(CGUIMessage& message)
{
  switch (message.GetMessage())
  {
    case GUI_MSG_CLICKED:
    {
      const int control = message.GetSenderId();
      if (control == CONTROL_CHEEVOS_HARDCORE)
      {
        OnHardcoreToggled();
        return true;
      }
      if (control == CONTROL_CHEEVOS_ENCORE || control == CONTROL_CHEEVOS_CHALLENGE_INDICATOR)
      {
        const auto settings = CServiceBroker::GetSettingsComponent()->GetSettings();
        settings->ToggleBool(control == CONTROL_CHEEVOS_ENCORE
                                 ? "gamesachievements.encore"
                                 : "gamesachievements.challengeindicator");
        settings->Save();
        return true;
      }
      break;
    }
    case GUI_MSG_NOTIFY_ALL:
    {
      if (message.GetParam1() == GUI_MSG_REFRESH_LIST)
      {
        // The service has answered about a game it knows but has no set for.
        // An empty list says nothing; the toast the runtime path uses says it.
        if (m_fetched && m_fetched->achievements.empty())
        {
          // "RetroAchievements", "This game doesn't support RetroAchievements"
          CGUIDialogKaiToast::QueueNotification(CGUIDialogKaiToast::Info, Localize(35264),
                                                Localize(35286), TOAST_DISPLAY_TIME_MS, false,
                                                TOAST_MESSAGE_TIME_MS);
          Close();
          return true;
        }
        // A targeted thread message reaches this window whether or not it is
        // open, and every unlock sends one. Rebuilding would sort the whole
        // set and construct a CFileItem per achievement on the GUI thread
        // during ordinary play. OnInitWindow() rebuilds when it is opened.
        if (IsActive())
          RefreshList();
        return true;
      }
      break;
    }
    default:
      break;
  }

  return CGUIDialog::OnMessage(message);
}

void CDialogGameAchievements::RefreshList()
{
  const AchievementState state = CurrentState();

  // Remembered by achievement rather than by row: an unlock moves its
  // achievement from the locked group into the earned one, so the row that
  // was selected is not the row it ends up on
  const int previousItem = m_viewControl->GetSelectedItem();
  unsigned int selectedId = 0;
  if (previousItem >= 0 && previousItem < m_items->Size())
    selectedId = (*m_items)[previousItem]->GetProperty(ACHIEVEMENT_ID).asUnsignedInteger();

  m_viewControl->Clear();
  m_items->Clear();

  std::vector<AchievementInfo> achievements = state.achievements;

  // Earned first; the runtime's order is kept within each group so the list
  // doesn't reshuffle as achievements unlock
  std::stable_sort(achievements.begin(), achievements.end(),
                   [](const AchievementInfo& lhs, const AchievementInfo& rhs)
                   { return lhs.earned && !rhs.earned; });

  uint64_t totalPoints = 0;
  uint64_t earnedPoints = 0;

  for (const AchievementInfo& achievement : achievements)
  {
    auto item = std::make_shared<CFileItem>(achievement.title);
    item->SetLabel(achievement.title);
    item->SetLabel2(achievement.description);

    const std::string& badgeUrl = (achievement.earned || achievement.lockedBadgeUrl.empty())
                                      ? achievement.badgeUrl
                                      : achievement.lockedBadgeUrl;
    if (!badgeUrl.empty())
      item->SetArt("icon", badgeUrl);

    // Not shown; carried so the selection survives a resort
    item->SetProperty(ACHIEVEMENT_ID, achievement.id);

    // "{0:d} pts"
    item->SetProperty(ACHIEVEMENT_POINTS, StringUtils::Format(Localize(35294), achievement.points));
    // Only set when true: a CVariant holding false stringifies to "false",
    // which String.IsEmpty() in the skin reads as present
    if (achievement.earned)
      item->SetProperty(ACHIEVEMENT_EARNED, true);

    if (achievement.earned && achievement.unlockedDate.IsValid())
    {
      // "Unlocked {0:s}"
      item->SetProperty(
          ACHIEVEMENT_UNLOCKED_DATE,
          StringUtils::Format(Localize(35289), achievement.unlockedDate.GetAsLocalizedDate(
                                                   std::string{"MMM dd yyyy"})));
    }

    if (achievement.rarity > 0.0f)
    {
      item->SetProperty(ACHIEVEMENT_RARITY_CATEGORY, RarityCategory(achievement.rarity));
      item->SetProperty(ACHIEVEMENT_RARITY_STARS, RarityStars(achievement.rarity));
    }

    // An earned achievement is by definition finished
    const bool measured = !achievement.earned && !achievement.measuredProgress.empty();
    if (measured)
    {
      item->SetProperty(ACHIEVEMENT_MEASURED, true);
      item->SetProperty(ACHIEVEMENT_MEASURED_PROGRESS, achievement.measuredProgress);
      if (achievement.measuredPercent == 0.0f)
        item->SetProperty(ACHIEVEMENT_MEASURED_ZERO, true);
    }

    // Set on every row: a list layout shares one progress control, which keeps
    // its last percentage when an item doesn't resolve the info, so a row
    // without this would draw the previous row's bar. Integer for asInteger().
    item->SetProperty(ACHIEVEMENT_MEASURED_PERCENT,
                      measured ? static_cast<int>(std::lround(
                                     std::clamp(achievement.measuredPercent, 0.0f, 100.0f)))
                               : 0);

    totalPoints += achievement.points;
    if (achievement.earned)
      earnedPoints += achievement.points;

    m_items->Add(std::move(item));
  }

  m_viewControl->SetItems(*m_items);

  // std::clamp() is undefined when the list is empty, since the upper bound
  // would fall below the lower one
  if (!m_items->IsEmpty())
  {
    int selectedItem = previousItem;
    if (selectedId != 0)
    {
      for (int i = 0; i < m_items->Size(); ++i)
      {
        if ((*m_items)[i]->GetProperty(ACHIEVEMENT_ID).asUnsignedInteger() == selectedId)
        {
          selectedItem = i;
          break;
        }
      }
    }

    m_viewControl->SetSelectedItem(std::clamp(selectedItem, 0, m_items->Size() - 1));
  }

  // Progress weighted by point value, which is how RetroAchievements measures
  // completion. Guard against a game whose achievements are all worth 0 points.
  std::string progress;
  if (totalPoints > 0)
  {
    const int percent = static_cast<int>((earnedPoints * 100) / totalPoints);

    // "{0:d}% complete"
    progress = StringUtils::Format(Localize(35288), percent);
  }

  // Build the header here rather than in the skin, so that skins don't have to
  // reproduce the punctuation and the empty cases
  //
  // "Achievements - Chrono Trigger (74% complete)"
  std::string header = Localize(35287);
  if (!state.gameTitle.empty())
    header += " - " + state.gameTitle;
  if (!progress.empty())
    header += " (" + progress + ")";

  SetProperty("Header", header);
  SetProperty("Achievements.GameTitle", state.gameTitle);
  SetProperty("Achievements.Progress", progress);
}

AchievementState CDialogGameAchievements::CurrentState() const
{
  if (m_fetched)
    return *m_fetched;

  return CServiceBroker::GetGameServices().AchievementRuntime().GetState();
}

bool CDialogGameAchievements::FetchForLibraryGame()
{
  // The game whose panel the list was opened over
  CGUIWindow* const parent = CServiceBroker::GetGUI()->GetWindowManager().GetWindow(
      CServiceBroker::GetGUI()->GetWindowManager().GetActiveWindowOrDialog());
  const CFileItemPtr item = parent != nullptr ? parent->GetCurrentListItem() : CFileItemPtr();
  if (!item || !item->HasGameInfoTag())
    return false;

  // Every game the catalogues recognised carries the service's own id for it,
  // which is what the set is filed under.
  const std::string gameId = item->GetGameInfoTag()->GetUniqueID("retroachievements");
  if (gameId.empty())
    return false;
  CLog::Log(LOGINFO, "CDialogGameAchievements: asking about game {}", gameId);

  const CGameSettings& gameSettings = CServiceBroker::GetGameServices().GameSettings();
  const std::string username = gameSettings.GetRAUsername();
  const std::string apiKey = gameSettings.GetRAApiKey();
  if (username.empty() || apiKey.empty())
    return false;

  AddJob(new CLibraryAchievementsJob(gameId, username, apiKey));
  return true;
}

void CDialogGameAchievements::OnJobComplete(unsigned int jobID, bool success, CJob* job)
{
  if (StringUtils::EqualsNoCase(job->GetType(), "library-achievements"))
  {
    const auto* fetchJob = static_cast<CLibraryAchievementsJob*>(job);

    CLog::Log(LOGINFO, "CDialogGameAchievements: answer for game arrived, success={}, {} achievements",
              success, fetchJob->GetState().achievements.size());
    if (success)
      m_fetched = fetchJob->GetState();
    else
      CLog::Log(LOGERROR, "CDialogGameAchievements: could not fetch achievements from the service");

    // Rebuilding a list control is only safe on the GUI thread
    CGUIMessage refresh(GUI_MSG_NOTIFY_ALL, GetID(), 0, GUI_MSG_REFRESH_LIST);
    CServiceBroker::GetGUI()->GetWindowManager().SendThreadMessage(refresh, GetID());
  }

  CJobQueue::OnJobComplete(jobID, success, job);
}

void CDialogGameAchievements::OnHardcoreToggled()
{
  CGameSettings& gameSettings = CServiceBroker::GetGameServices().GameSettings();

  const bool enabling = !gameSettings.GetAchievementsHardcore();

  // Only turning it on is asked about, and only while a game is up: that is
  // the case that costs the player the session they are in. The radio button
  // takes its state from the setting, so a refusal here corrects it.
  //
  // "Hardcore mode", "Starting a hardcore session restarts the game..."
  const auto appPlayer = CServiceBroker::GetAppComponents().GetComponent<CApplicationPlayer>();
  if (enabling && appPlayer->IsPlayingGame() &&
      !CGUIDialogYesNo::ShowAndGetInput(CVariant{35700}, CVariant{35702}))
  {
    return;
  }

  gameSettings.SetAchievementsHardcore(enabling);
  CServiceBroker::GetSettingsComponent()->GetSettings()->Save();

  // The whole OSD goes, not just this dialog: the player asked for the game to
  // restart, and leaving them on the menu they opened hides it
  if (enabling)
    CServiceBroker::GetGameRenderManager().RegisterGameSettingsDialog()->CloseOSD();
}
