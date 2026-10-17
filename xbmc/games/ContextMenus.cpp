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

bool SetCompleted(const std::shared_ptr<CFileItem>& item, bool completed)
{
  GAME::CGameInfoTag* tag = item->GetGameInfoTag();

  GAME::CGameDatabase db;
  if (!db.Open() || !db.SetCompleted(tag->GetDatabaseId(), completed))
    return false;

  tag->SetCompleted(completed);
  item->SetProperty("completed", completed);
  item->SetOverlayImage(completed ? CGUIListItem::ICON_OVERLAY_WATCHED
                                  : CGUIListItem::ICON_OVERLAY_NONE);

  CGUIMessage msg(GUI_MSG_NOTIFY_ALL, 0, 0, GUI_MSG_REFRESH_LIST);
  CServiceBroker::GetGUI()->GetWindowManager().SendThreadMessage(msg);
  return true;
}
} // namespace

namespace CONTEXTMENU
{

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
