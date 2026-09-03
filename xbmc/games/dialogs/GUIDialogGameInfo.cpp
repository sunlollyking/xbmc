/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "GUIDialogGameInfo.h"

#include "FileItem.h"
#include "GUIUserMessages.h"
#include "ServiceBroker.h"
#include "dialogs/GUIDialogSelect.h"
#include "games/GameUtils.h"
#include "games/database/GameDatabase.h"
#include "games/library/GameLibraryQueue.h"
#include "games/tags/GameInfoTag.h"
#include "guilib/GUIComponent.h"
#include "guilib/GUIMessage.h"
#include "guilib/GUIWindowManager.h"
#include "guilib/WindowIDs.h"
#include "resources/LocalizeStrings.h"
#include "resources/ResourcesComponent.h"
#include "application/ApplicationComponents.h"
#include "messaging/ApplicationMessenger.h"
#include "utils/URIUtils.h"
#include "utils/Variant.h"

#include <array>
#include <set>
#include <utility>
#include <vector>

using namespace KODI;
using namespace GAME;

namespace
{
constexpr int CONTROL_BTN_RELEASES = 5;
constexpr int CONTROL_BTN_REFRESH = 6;
constexpr int CONTROL_BTN_USERRATING = 7;
constexpr int CONTROL_BTN_PLAY = 8;
constexpr int CONTROL_BTN_FAVOURITE = 12;
constexpr int CONTROL_BTN_PLAY_STATE = 13;
constexpr int CONTROL_BTN_GAME_CLIENT = 14;
constexpr int CONTROL_BTN_VIDEO_FILTER = 15;
constexpr int CONTROL_BTN_ARTWORK = 16;

/*!
 * \brief The pictures a game has, in the order a person would look at them
 *
 * A game is not a film: what it is recognised by is the front of its box, and
 * what is worth looking at after that is the rest of the package and the game
 * on screen. Anything the library holds that is not listed here is shown
 * after these, under the name the scraper gave it.
 */
constexpr std::array<std::pair<const char*, int>, 10> ART_TYPES = {{
    {"boxfront", 35576},
    {"boxback", 35577},
    {"spine", 35584},
    {"cartridge", 35578},
    {"disc", 35579},
    {"titlescreen", 35580},
    {"screenshot", 35581},
    {"snap", 35581},
    {"clearlogo", 35582},
    {"banner", 35583},
}};

std::string Localize(int id)
{
  return CServiceBroker::GetResourcesComponent().GetLocalizeStrings().Get(id);
}
} // namespace

CGUIDialogGameInfo::CGUIDialogGameInfo()
  : CGUIDialog(WINDOW_DIALOG_GAME_INFO, "DialogGameInfo.xml")
{
  m_loadType = KEEP_IN_MEMORY;
}

CGUIDialogGameInfo::~CGUIDialogGameInfo() = default;

bool CGUIDialogGameInfo::ShowFor(const std::shared_ptr<CFileItem>& item)
{
  if (!item)
    return false;

  auto* dialog =
      CServiceBroker::GetGUI()->GetWindowManager().GetWindow<CGUIDialogGameInfo>(WINDOW_DIALOG_GAME_INFO);
  if (dialog == nullptr)
    return false;

  dialog->SetItem(item);
  dialog->Open();

  return dialog->WantsToPlay();
}

void CGUIDialogGameInfo::SetItem(const std::shared_ptr<CFileItem>& item)
{
  m_item = std::make_shared<CFileItem>(*item);
  m_play = false;
  Reload();
}

void CGUIDialogGameInfo::Reload()
{
  if (!m_item || !m_item->HasProperty("gameid"))
    return;

  CGameDatabase db;
  CGameInfoTag tag;
  const int idGame = static_cast<int>(m_item->GetProperty("gameid").asInteger());
  if (db.Open() && db.GetGameInfo(idGame, tag))
  {
    *m_item->GetGameInfoTag() = tag;
    KODI::ART::Artwork art;
    if (db.GetArtForItem(idGame, MediaTypeGame, art))
    {
      for (const auto& [type, url] : art)
      {
        if (!m_item->HasArt(type))
          m_item->SetArt(type, url);
      }
    }
  }
}

void CGUIDialogGameInfo::OnInitWindow()
{
  CGUIDialog::OnInitWindow();
  UpdateButtons();
}

void CGUIDialogGameInfo::UpdateButtons()
{
  const bool inLibrary = m_item && m_item->HasProperty("gameid");
  CONTROL_ENABLE_ON_CONDITION(CONTROL_BTN_RELEASES, inLibrary);
  CONTROL_ENABLE_ON_CONDITION(CONTROL_BTN_REFRESH, inLibrary);
  CONTROL_ENABLE_ON_CONDITION(CONTROL_BTN_FAVOURITE, inLibrary);
  CONTROL_ENABLE_ON_CONDITION(CONTROL_BTN_PLAY_STATE, inLibrary);
  CONTROL_ENABLE_ON_CONDITION(CONTROL_BTN_USERRATING, inLibrary);

  if (m_item && m_item->HasGameInfoTag())
  {
    const CGameInfoTag* tag = m_item->GetGameInfoTag();
    SET_CONTROL_LABEL(CONTROL_BTN_FAVOURITE, tag->IsFavourite() ? 14077 : 14076);

    // Where a video is watched or not, a game is not started, in progress or
    // finished, and the button says which of the three it is
    static constexpr std::array<int, 3> stateLabels{35572, 35573, 35574};
    static constexpr std::array<const char*, 3> stateNames{"notstarted", "inprogress",
                                                           "completed"};
    const size_t state = static_cast<size_t>(PlayState(*tag));
    SET_CONTROL_LABEL(CONTROL_BTN_PLAY_STATE, stateLabels[state]);
    SetProperty("playstate", stateNames[state]);
  }
}

bool CGUIDialogGameInfo::OnMessage(CGUIMessage& message)
{
  switch (message.GetMessage())
  {
    case GUI_MSG_CLICKED:
    {
      const int control = message.GetSenderId();
      switch (control)
      {
        case CONTROL_BTN_PLAY:
          m_play = true;
          Close();
          return true;
        case CONTROL_BTN_REFRESH:
          OnRefresh();
          return true;
        case CONTROL_BTN_RELEASES:
          OnReleases();
          return true;
        case CONTROL_BTN_FAVOURITE:
          OnFavourite();
          return true;
        case CONTROL_BTN_PLAY_STATE:
          OnPlayState();
          return true;
        case CONTROL_BTN_USERRATING:
          OnUserRating();
          return true;
        case CONTROL_BTN_GAME_CLIENT:
          OnGameClient();
          return true;
        case CONTROL_BTN_VIDEO_FILTER:
          OnVideoFilter();
          return true;
        case CONTROL_BTN_ARTWORK:
          OnArtwork();
          return true;
        default:
          break;
      }
      break;
    }
    case GUI_MSG_NOTIFY_ALL:
    {
      if (message.GetParam1() == GUI_MSG_UPDATE && IsActive())
      {
        Reload();
        UpdateButtons();
        CGUIMessage refresh(GUI_MSG_REFRESH_LIST, GetID(), 0);
        OnMessage(refresh);
      }
      break;
    }
    default:
      break;
  }

  return CGUIDialog::OnMessage(message);
}

void CGUIDialogGameInfo::OnRefresh()
{
  if (!m_item || !m_item->HasProperty("gameid"))
    return;
  CGameLibraryQueue::GetInstance().RefreshGame(
      static_cast<int>(m_item->GetProperty("gameid").asInteger()), true);
}

void CGUIDialogGameInfo::OnArtwork()
{
  if (!m_item)
    return;

  auto* select =
      CServiceBroker::GetGUI()->GetWindowManager().GetWindow<CGUIDialogSelect>(WINDOW_DIALOG_SELECT);
  if (select == nullptr)
    return;

  // The known kinds first, in their own order, then anything else the library
  // was given, so a new kind of picture appears without a change here
  std::vector<std::pair<std::string, std::string>> pictures; // label, url
  std::set<std::string, std::less<>> taken;
  for (const auto& [type, label] : ART_TYPES)
  {
    const std::string url = m_item->GetArt(type);
    if (url.empty() || taken.contains(url))
      continue;
    taken.insert(url);
    pictures.emplace_back(Localize(label), url);
  }
  for (const auto& [type, url] : m_item->GetArt())
  {
    if (url.empty() || taken.contains(url))
      continue;
    taken.insert(url);
    pictures.emplace_back(type, url);
  }

  if (pictures.empty())
    return;

  select->Reset();
  select->SetHeading(CVariant{35575}); // "Artwork"
  select->SetUseDetails(true);
  for (const auto& [label, url] : pictures)
  {
    CFileItem picture(label);
    picture.SetArt("thumb", url);
    picture.SetLabel2(URIUtils::GetFileName(url));
    select->Add(picture);
  }
  select->Open();

  const int chosen = select->GetSelectedItem();
  if (chosen < 0 || chosen >= static_cast<int>(pictures.size()))
    return;

  // The picture viewer shows it whole, and zooming and panning come with it
  CServiceBroker::GetAppMessenger()->PostMsg(TMSG_EXECUTE_BUILT_IN, -1, -1, nullptr,
                                            "ShowPicture(" + pictures[chosen].second + ")");
}

void CGUIDialogGameInfo::OnReleases()
{
  if (!m_item || !m_item->HasProperty("gameid") || !m_item->HasGameInfoTag())
    return;

  const CGameInfoTag* tag = m_item->GetGameInfoTag();
  const std::string path = "gamedb://platforms/" + std::to_string(tag->GetPlatformId()) +
                           "/titles/" + std::to_string(tag->GetDatabaseId()) + "/";
  Close();
  CServiceBroker::GetGUI()->GetWindowManager().ActivateWindow(WINDOW_GAMES, {path, "return"});
}

void CGUIDialogGameInfo::OnFavourite()
{
  if (!m_item || !m_item->HasProperty("gameid") || !m_item->HasGameInfoTag())
    return;

  CGameInfoTag* tag = m_item->GetGameInfoTag();
  CGameDatabase db;
  if (db.Open() && db.SetFavourite(tag->GetDatabaseId(), !tag->IsFavourite()))
  {
    tag->SetFavourite(!tag->IsFavourite());
    UpdateButtons();
  }
}

CGUIDialogGameInfo::PlayStateValue CGUIDialogGameInfo::PlayState(const CGameInfoTag& tag)
{
  if (tag.IsCompleted())
    return PlayStateValue::COMPLETED;
  if (tag.GetPlayCount() > 0)
    return PlayStateValue::IN_PROGRESS;
  return PlayStateValue::NOT_STARTED;
}

void CGUIDialogGameInfo::OnPlayState()
{
  if (!m_item || !m_item->HasProperty("gameid") || !m_item->HasGameInfoTag())
    return;

  CGameInfoTag* tag = m_item->GetGameInfoTag();
  CGameDatabase db;
  if (!db.Open())
    return;

  // The button moves to the next state, the way a watched flag is toggled but
  // with the middle state a game needs
  const int idGame = tag->GetDatabaseId();
  switch (PlayState(*tag))
  {
    case PlayStateValue::NOT_STARTED:
      if (db.SetPlayCount(idGame, 1))
        tag->SetPlayCount(1);
      break;
    case PlayStateValue::IN_PROGRESS:
      if (db.SetCompleted(idGame, true))
        tag->SetCompleted(true);
      break;
    case PlayStateValue::COMPLETED:
      if (db.SetCompleted(idGame, false) && db.SetPlayCount(idGame, 0))
      {
        tag->SetCompleted(false);
        tag->SetPlayCount(0);
      }
      break;
  }

  UpdateButtons();
}

void CGUIDialogGameInfo::OnUserRating()
{
  if (!m_item || !m_item->HasProperty("gameid") || !m_item->HasGameInfoTag())
    return;

  auto* select =
      CServiceBroker::GetGUI()->GetWindowManager().GetWindow<CGUIDialogSelect>(WINDOW_DIALOG_SELECT);
  if (select == nullptr)
    return;

  CGameInfoTag* tag = m_item->GetGameInfoTag();
  select->Reset();
  select->SetHeading(CVariant{38023}); // Set your rating
  select->Add(Localize(38022)); // No rating
  for (int i = 1; i <= 10; ++i)
    select->Add(std::to_string(i));
  select->SetSelected(tag->GetUserRating());
  select->Open();

  if (!select->IsConfirmed() || select->GetSelectedItem() < 0)
    return;

  const int rating = select->GetSelectedItem();
  CGameDatabase db;
  if (db.Open() && db.SetUserRating(tag->GetDatabaseId(), rating))
    tag->SetUserRating(rating);
}

void CGUIDialogGameInfo::OnGameClient()
{
  if (m_item)
    CGameUtils::ChooseAndSetDefaultGameClient(*m_item);
}

void CGUIDialogGameInfo::OnVideoFilter()
{
  if (m_item)
    CGameUtils::ChooseAndSetDefaultVideoFilter(*m_item);
}
