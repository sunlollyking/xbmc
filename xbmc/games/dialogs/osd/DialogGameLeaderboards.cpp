/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "DialogGameLeaderboards.h"

#include "FileItem.h"
#include "ServiceBroker.h"
#include "URL.h"
#include "dialogs/GUIDialogKaiToast.h"
#include "filesystem/CurlFile.h"
#include "games/AchievementRuntime.h"
#include "games/GameServices.h"
#include "games/GameSettings.h"
#include "games/dialogs/DialogGameDefines.h"
#include "guilib/GUIComponent.h"
#include "guilib/GUIMessage.h"
#include "guilib/GUIWindowManager.h"
#include "guilib/WindowIDs.h"
#include "input/actions/ActionIDs.h"
#include "jobs/Job.h"
#include "resources/LocalizeStrings.h"
#include "resources/ResourcesComponent.h"
#include "utils/JSONVariantParser.h"
#include "utils/StringUtils.h"
#include "utils/Variant.h"
#include "utils/log.h"
#include "view/ViewState.h"

#include <cmath>
#include <mutex>

using namespace KODI::GAME;

namespace
{
constexpr int CONTROL_LEADERBOARD_LIST = 3;

//! How long to wait on RetroAchievements before giving up on one leaderboard
constexpr int REQUEST_TIMEOUT_SECS = 10;

constexpr const char* LBINFO_URL =
    "https://retroachievements.org/dorequest.php?r=lbinfo&i={}&u={}&t={}&c=1&o=0";

/*!
 * \brief Format a raw leaderboard value the way the server would
 *
 * Scores arrive as plain integers and mean nothing without the leaderboard's
 * format: 9000 is either nine thousand points or two and a half minutes.
 */
std::string FormatScore(unsigned int score, const std::string& format)
{
  if (format == "TIME" || format == "FRAMES")
  {
    // Counted in frames at 60Hz, which is how RetroAchievements measures a
    // time trial regardless of what the console actually ran at
    const double totalSeconds = static_cast<double>(score) / 60.0;
    const auto minutes = static_cast<unsigned int>(totalSeconds) / 60;
    const auto seconds = static_cast<unsigned int>(totalSeconds) % 60;
    const auto centiseconds =
        static_cast<unsigned int>((totalSeconds - std::floor(totalSeconds)) * 100);

    return StringUtils::Format("{}:{:02d}.{:02d}", minutes, seconds, centiseconds);
  }

  if (format == "TIMESECS")
    return StringUtils::Format("{}:{:02d}", score / 60, score % 60);

  if (format == "FIXED1")
    return StringUtils::Format("{:.1f}", static_cast<double>(score) / 10.0);
  if (format == "FIXED2")
    return StringUtils::Format("{:.2f}", static_cast<double>(score) / 100.0);
  if (format == "FIXED3")
    return StringUtils::Format("{:.3f}", static_cast<double>(score) / 1000.0);

  return std::to_string(score);
}

/*!
 * \brief Fetches the standings of one leaderboard, off the GUI thread
 *
 * One job per leaderboard rather than one for all of them, so that a game with
 * thirty of them fills its list in as the answers come rather than after the
 * last one, and so that closing the dialog abandons what is left.
 */
class CLeaderboardStandingsJob : public CJob
{
public:
  CLeaderboardStandingsJob(unsigned int leaderboardId,
                           std::string format,
                           std::string username,
                           std::string token)
    : m_id(leaderboardId),
      m_format(std::move(format)),
      m_username(std::move(username)),
      m_token(std::move(token))
  {
  }

  const char* GetType() const override { return "leaderboard-standings"; }

  bool DoWork() override
  {
    const std::string url = StringUtils::Format(LBINFO_URL, m_id, CURL::Encode(m_username),
                                                CURL::Encode(m_token));

    XFILE::CCurlFile curl;
    curl.SetTimeout(REQUEST_TIMEOUT_SECS);

    std::string response;
    if (!curl.Get(url, response))
      return false;

    CVariant data;
    if (!CJSONVariantParser::Parse(response, data) || !data["Success"].asBoolean())
      return false;

    const CVariant& leaderboard = data["LeaderboardData"];
    m_totalEntries = static_cast<unsigned int>(leaderboard["TotalEntries"].asUnsignedInteger());

    const CVariant& entries = leaderboard["Entries"];
    if (entries.begin_array() != entries.end_array())
    {
      const CVariant& top = *entries.begin_array();
      m_topUsername = top["User"].asString();
      m_topScore =
          FormatScore(static_cast<unsigned int>(top["Score"].asUnsignedInteger()), m_format);
    }

    // Only present once the player has submitted to this leaderboard
    const CVariant& playerEntry = leaderboard["UserEntry"];
    if (playerEntry.isObject() && !playerEntry["Rank"].isNull())
    {
      m_playerRank = static_cast<unsigned int>(playerEntry["Rank"].asUnsignedInteger());
      m_playerScore =
          FormatScore(static_cast<unsigned int>(playerEntry["Score"].asUnsignedInteger()), m_format);
    }

    return true;
  }

  unsigned int GetLeaderboardId() const { return m_id; }
  unsigned int GetTotalEntries() const { return m_totalEntries; }
  unsigned int GetPlayerRank() const { return m_playerRank; }
  const std::string& GetPlayerScore() const { return m_playerScore; }
  const std::string& GetTopUsername() const { return m_topUsername; }
  const std::string& GetTopScore() const { return m_topScore; }

private:
  const unsigned int m_id;
  const std::string m_format;
  const std::string m_username;
  const std::string m_token;

  unsigned int m_totalEntries{0};
  unsigned int m_playerRank{0};
  std::string m_playerScore;
  std::string m_topUsername;
  std::string m_topScore;
};
} // namespace

CDialogGameLeaderboards::CDialogGameLeaderboards()
  : CGUIDialog(WINDOW_DIALOG_GAME_LEADERBOARDS, "DialogGameControllers.xml"),
    CJobQueue(false, 1)
{
}

CDialogGameLeaderboards::~CDialogGameLeaderboards() = default;

void CDialogGameLeaderboards::OnWindowLoaded()
{
  CGUIDialog::OnWindowLoaded();

  m_viewControl.Reset();
  m_viewControl.SetParentWindow(GetID());
  m_viewControl.AddView(GetControl(CONTROL_LEADERBOARD_LIST));
}

void CDialogGameLeaderboards::OnWindowUnload()
{
  m_viewControl.Reset();

  CGUIDialog::OnWindowUnload();
}

void CDialogGameLeaderboards::OnInitWindow()
{
  const auto& strings = CServiceBroker::GetResourcesComponent().GetLocalizeStrings();
  const auto& gameSettings = CServiceBroker::GetGameServices().GameSettings();

  if (!gameSettings.GetAchievementsLoggedIn())
  {
    // "Leaderboards", "Sign in to RetroAchievements to see leaderboards"
    CGUIDialogKaiToast::QueueNotification(CGUIDialogKaiToast::Warning, strings.Get(35331),
                                          strings.Get(35333));
    Close();
    return;
  }

  const LeaderboardState state =
      CServiceBroker::GetGameServices().AchievementRuntime().GetLeaderboardState();

  if (!state.loaded || state.leaderboards.empty())
  {
    // "Leaderboards", "This game has no leaderboards"
    CGUIDialogKaiToast::QueueNotification(CGUIDialogKaiToast::Info, strings.Get(35331),
                                          strings.Get(35334));
    Close();
    return;
  }

  m_viewControl.SetCurrentView(DEFAULT_VIEW_LIST);

  PopulateList();

  CGUIDialog::OnInitWindow();

  if (m_lastSelected >= 0)
    m_viewControl.SetSelectedItem(m_lastSelected);

  FetchStandings();
}

void CDialogGameLeaderboards::OnDeinitWindow(int nextWindowID)
{
  // Whatever is still in flight is for a screen that is going away
  CancelJobs();

  {
    std::unique_lock lock(m_section);
    m_items.Clear();
  }

  m_viewControl.Clear();

  CGUIDialog::OnDeinitWindow(nextWindowID);
}

void CDialogGameLeaderboards::FetchStandings()
{
  const auto& gameSettings = CServiceBroker::GetGameServices().GameSettings();

  const std::string username = gameSettings.GetRAUsername();
  const std::string token = gameSettings.GetRAToken();
  if (username.empty() || token.empty())
    return;

  const LeaderboardState state =
      CServiceBroker::GetGameServices().AchievementRuntime().GetLeaderboardState();

  for (const LeaderboardInfo& leaderboard : state.leaderboards)
  {
    // Already fetched this session, so the list is filled in already
    if (leaderboard.totalEntries > 0)
      continue;

    AddJob(new CLeaderboardStandingsJob(leaderboard.id, leaderboard.format, username, token));
  }
}

void CDialogGameLeaderboards::OnJobComplete(unsigned int jobID, bool success, CJob* job)
{
  if (success && StringUtils::EqualsNoCase(job->GetType(), "leaderboard-standings"))
  {
    const auto* standings = static_cast<CLeaderboardStandingsJob*>(job);

    auto& runtime = CServiceBroker::GetGameServices().AchievementRuntime();
    LeaderboardState state = runtime.GetLeaderboardState();

    for (LeaderboardInfo& leaderboard : state.leaderboards)
    {
      if (leaderboard.id != standings->GetLeaderboardId())
        continue;

      leaderboard.totalEntries = standings->GetTotalEntries();
      leaderboard.playerRank = standings->GetPlayerRank();
      leaderboard.playerScore = standings->GetPlayerScore();
      leaderboard.topUsername = standings->GetTopUsername();
      leaderboard.topScore = standings->GetTopScore();
      break;
    }

    runtime.SetLeaderboardState(state);

    // Rebuilding a list control is only safe on the GUI thread, so the
    // rebuild is asked for rather than done here
    CGUIMessage refresh(GUI_MSG_NOTIFY_ALL, GetID(), 0, GUI_MSG_REFRESH_LIST);
    CServiceBroker::GetGUI()->GetWindowManager().SendThreadMessage(refresh, GetID());
  }

  CJobQueue::OnJobComplete(jobID, success, job);
}

bool CDialogGameLeaderboards::OnMessage(CGUIMessage& message)
{
  switch (message.GetMessage())
  {
    case GUI_MSG_NOTIFY_ALL:
    {
      if (message.GetParam1() == GUI_MSG_REFRESH_LIST)
      {
        PopulateList();
        return true;
      }
      break;
    }

    case GUI_MSG_CLICKED:
    {
      const int action = message.GetParam1();
      if (action == ACTION_SELECT_ITEM || action == ACTION_MOUSE_LEFT_CLICK)
      {
        const int selected = m_viewControl.GetSelectedItem();

        std::unique_lock lock(m_section);
        if (selected >= 0 && selected < m_items.Size())
        {
          const auto leaderboardId =
              static_cast<unsigned int>(m_items[selected]->GetProperty("LeaderboardId").asInteger());
          lock.unlock();

          m_lastSelected = selected;

          // The entries dialog is opened by the skin, which cannot pass
          // anything, so which leaderboard is meant goes through the runtime
          CServiceBroker::GetGameServices().AchievementRuntime().SetSelectedLeaderboard(
              leaderboardId);

          CServiceBroker::GetGUI()->GetWindowManager().ActivateWindow(
              WINDOW_DIALOG_GAME_LEADERBOARD_ENTRIES);
          return true;
        }
      }
      break;
    }
    default:
      break;
  }

  return CGUIDialog::OnMessage(message);
}

std::string CDialogGameLeaderboards::DescribeFormat(const std::string& format, bool lowerIsBetter)
{
  const auto& strings = CServiceBroker::GetResourcesComponent().GetLocalizeStrings();

  if (format == "TIME" || format == "TIMESECS" || format == "FRAMES")
  {
    // "Fastest time" / "Longest time"
    return strings.Get(lowerIsBetter ? 35335 : 35336);
  }

  // "Lowest score" / "Highest score"
  return strings.Get(lowerIsBetter ? 35337 : 35338);
}

void CDialogGameLeaderboards::PopulateList()
{
  const auto& strings = CServiceBroker::GetResourcesComponent().GetLocalizeStrings();

  const LeaderboardState state =
      CServiceBroker::GetGameServices().AchievementRuntime().GetLeaderboardState();

  std::unique_lock lock(m_section);

  m_items.Clear();

  for (const LeaderboardInfo& leaderboard : state.leaderboards)
  {
    auto item = std::make_shared<CFileItem>(leaderboard.title);
    item->SetLabel(leaderboard.title);
    item->SetLabel2(leaderboard.description);

    item->SetProperty("LeaderboardId", static_cast<int>(leaderboard.id));
    item->SetProperty("Format", DescribeFormat(leaderboard.format, leaderboard.lowerIsBetter));
    item->SetProperty("TopUsername", leaderboard.topUsername);
    item->SetProperty("TopScore", leaderboard.topScore);

    // Left empty rather than shown as zero: a leaderboard whose standings have
    // not arrived yet, and one nobody has entered, should not read the same
    if (leaderboard.totalEntries > 0)
    {
      // "{0:d} entries"
      item->SetProperty("TotalEntries",
                        StringUtils::Format(strings.Get(35339), leaderboard.totalEntries));
    }

    if (leaderboard.playerRank > 0)
    {
      // "Your rank: {0:d}"
      item->SetProperty("PlayerRank",
                        StringUtils::Format(strings.Get(35340), leaderboard.playerRank));
      item->SetProperty("PlayerScore", leaderboard.playerScore);
    }
    else if (leaderboard.totalEntries > 0)
    {
      // "Not ranked"
      item->SetProperty("PlayerRank", strings.Get(35341));
    }

    m_items.Add(std::move(item));
  }

  lock.unlock();

  m_viewControl.SetItems(m_items);
}
