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
#include "utils/Variant.h"

using namespace KODI;
using namespace GAME;

namespace
{
constexpr int CONTROL_BTN_RELEASES = 5;
constexpr int CONTROL_BTN_REFRESH = 6;
constexpr int CONTROL_BTN_USERRATING = 7;
constexpr int CONTROL_BTN_PLAY = 8;
constexpr int CONTROL_BTN_FAVOURITE = 12;
constexpr int CONTROL_BTN_COMPLETED = 13;
constexpr int CONTROL_BTN_GAME_CLIENT = 14;
constexpr int CONTROL_BTN_VIDEO_FILTER = 15;

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
  CONTROL_ENABLE_ON_CONDITION(CONTROL_BTN_COMPLETED, inLibrary);
  CONTROL_ENABLE_ON_CONDITION(CONTROL_BTN_USERRATING, inLibrary);

  if (m_item && m_item->HasGameInfoTag())
  {
    const CGameInfoTag* tag = m_item->GetGameInfoTag();
    SET_CONTROL_LABEL(CONTROL_BTN_FAVOURITE, tag->IsFavourite() ? 14077 : 14076);
    SET_CONTROL_LABEL(CONTROL_BTN_COMPLETED, tag->IsCompleted() ? 35559 : 35558);
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
        case CONTROL_BTN_COMPLETED:
          OnCompleted();
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

void CGUIDialogGameInfo::OnCompleted()
{
  if (!m_item || !m_item->HasProperty("gameid") || !m_item->HasGameInfoTag())
    return;

  CGameInfoTag* tag = m_item->GetGameInfoTag();
  CGameDatabase db;
  if (db.Open() && db.SetCompleted(tag->GetDatabaseId(), !tag->IsCompleted()))
  {
    tag->SetCompleted(!tag->IsCompleted());
    UpdateButtons();
  }
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
