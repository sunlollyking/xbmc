/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "ContextMenus.h"

#include "FileItem.h"
#include "GUIUserMessages.h"
#include "ServiceBroker.h"
#include "games/database/GameDatabase.h"
#include "games/tags/GameInfoTag.h"
#include "guilib/GUIComponent.h"
#include "guilib/GUIListItem.h"
#include "guilib/GUIMessage.h"
#include "guilib/GUIWindowManager.h"

using namespace KODI;

namespace
{
bool IsLibraryGame(const CFileItem& item)
{
  return item.HasGameInfoTag() && item.GetGameInfoTag()->HasDatabaseId();
}

void ShowProgress(const std::shared_ptr<CFileItem>& item)
{
  const GAME::CGameInfoTag* tag = item->GetGameInfoTag();
  item->SetProperty("completed", tag->IsCompleted());
  item->SetOverlayImage(tag->IsCompleted() ? CGUIListItem::ICON_OVERLAY_WATCHED
                                           : CGUIListItem::ICON_OVERLAY_NONE);
  GAME::CGameDatabase::SetCompletionProperties(*item, *tag);

  CGUIMessage msg(GUI_MSG_NOTIFY_ALL, 0, 0, GUI_MSG_REFRESH_LIST);
  CServiceBroker::GetGUI()->GetWindowManager().SendThreadMessage(msg);
}

bool SetCompleted(const std::shared_ptr<CFileItem>& item, bool completed)
{
  GAME::CGameInfoTag* tag = item->GetGameInfoTag();

  GAME::CGameDatabase db;
  if (!db.Open() || !db.SetCompleted(tag->GetDatabaseId(), completed))
    return false;

  tag->SetCompleted(completed);
  if (completed)
    tag->SetBeaten(true);
  ShowProgress(item);
  return true;
}

bool SetBeaten(const std::shared_ptr<CFileItem>& item, bool beaten)
{
  GAME::CGameInfoTag* tag = item->GetGameInfoTag();

  GAME::CGameDatabase db;
  if (!db.Open() || !db.SetBeaten(tag->GetDatabaseId(), beaten))
    return false;

  tag->SetBeaten(beaten);
  if (!beaten)
    tag->SetCompleted(false);
  ShowProgress(item);
  return true;
}
} // namespace

namespace CONTEXTMENU
{

bool CGameMarkBeaten::IsVisible(const CFileItem& item) const
{
  return IsLibraryGame(item) && !item.GetGameInfoTag()->IsBeaten();
}

bool CGameMarkBeaten::Execute(const std::shared_ptr<CFileItem>& item) const
{
  return SetBeaten(item, true);
}

bool CGameMarkNotBeaten::IsVisible(const CFileItem& item) const
{
  // A completed game is marked not completed first
  return IsLibraryGame(item) && item.GetGameInfoTag()->IsBeaten() &&
         !item.GetGameInfoTag()->IsCompleted();
}

bool CGameMarkNotBeaten::Execute(const std::shared_ptr<CFileItem>& item) const
{
  return SetBeaten(item, false);
}

bool CGameMarkCompleted::IsVisible(const CFileItem& item) const
{
  return IsLibraryGame(item) && !item.GetGameInfoTag()->IsCompleted();
}

bool CGameMarkCompleted::Execute(const std::shared_ptr<CFileItem>& item) const
{
  return SetCompleted(item, true);
}

bool CGameMarkNotCompleted::IsVisible(const CFileItem& item) const
{
  return IsLibraryGame(item) && item.GetGameInfoTag()->IsCompleted();
}

bool CGameMarkNotCompleted::Execute(const std::shared_ptr<CFileItem>& item) const
{
  return SetCompleted(item, false);
}

} // namespace CONTEXTMENU
