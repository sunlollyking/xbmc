/*
 *  Copyright (C) 2012-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "GUIViewStateWindowGames.h"

#include "FileItem.h"
#include "FileItemList.h"
#include "games/GameUtils.h"
#include "guilib/WindowIDs.h"
#include "settings/MediaSourceSettings.h"
#include "utils/StringUtils.h"
#include "utils/URIUtils.h"
#include "view/ViewState.h"
#include "view/ViewStateSettings.h"

#include <assert.h>
#include <set>

using namespace KODI;
using namespace GAME;

CGUIViewStateWindowGames::CGUIViewStateWindowGames(const CFileItemList& items)
  : CGUIViewState(items)
{
  if (items.IsVirtualDirectoryRoot())
  {
    AddSortMethod(SortBy::LABEL, 551, LABEL_MASKS());
    AddSortMethod(SortBy::DRIVE_TYPE, 564, LABEL_MASKS());
    SetSortMethod(SortBy::LABEL);
    SetSortOrder(SortOrder::ASCENDING);
    SetViewAsControl(DEFAULT_VIEW_LIST);
  }
  else if (URIUtils::IsProtocol(items.GetPath(), "gamedb") || items.GetContent() == "games" ||
           items.GetContent() == "releases" || items.GetContent() == "platforms")
  {
    // A smart playlist lists the library too, so it sorts and looks the same
    if (items.GetContent() == "games" || items.GetContent() == "releases")
    {
      AddSortMethod(SortBy::LABEL, 551, LABEL_MASKS("%T", "%Y", "%T", "%Y")); // Title, Year
      AddSortMethod(SortBy::YEAR, 562, LABEL_MASKS("%T", "%Y", "%T", "%Y"));
      AddSortMethod(SortBy::RATING, 563, LABEL_MASKS("%T", "%R", "%T", "%R"));
      AddSortMethod(SortBy::USER_RATING, 38018, LABEL_MASKS("%T", "%r", "%T", "%r"));
      AddSortMethod(SortBy::PLAYCOUNT, 567, LABEL_MASKS("%T", "%V", "%T", "%V"));
      AddSortMethod(SortBy::LAST_PLAYED, 568, LABEL_MASKS("%T", "%p", "%T", "%p"));
      AddSortMethod(SortBy::DATE_ADDED, 570, LABEL_MASKS("%T", "%a", "%T", "%a"));
      AddSortMethod(SortBy::STUDIO, 35549, LABEL_MASKS("%T", "%U", "%T", "%U")); // Platform
      SetSortMethod(SortBy::LABEL);
    }
    else
    {
      AddSortMethod(SortBy::LABEL, 551, LABEL_MASKS("%L", "%V", "%L", "%V")); // Label, count
      SetSortMethod(SortBy::LABEL);
    }
    SetSortOrder(SortOrder::ASCENDING);

    const CViewState* viewState = CViewStateSettings::GetInstance().Get("gameslibrary");
    if (viewState)
    {
      SetSortMethod(viewState->m_sortDescription);
      SetViewAsControl(viewState->m_viewMode);
      SetSortOrder(viewState->m_sortDescription.sortOrder);
    }
  }
  else
  {
    AddSortMethod(SortBy::FILE, 561,
                  LABEL_MASKS("%F", "%I", "%L", "")); // Filename, Size | Label, empty
    AddSortMethod(SortBy::SIZE, 553,
                  LABEL_MASKS("%L", "%I", "%L", "%I")); // Filename, Size | Label, Size

    const CViewState* viewState = CViewStateSettings::GetInstance().Get("games");
    if (viewState)
    {
      SetSortMethod(viewState->m_sortDescription);
      SetViewAsControl(viewState->m_viewMode);
      SetSortOrder(viewState->m_sortDescription.sortOrder);
    }
  }

  LoadViewState(items.GetPath(), WINDOW_GAMES);
}

std::string CGUIViewStateWindowGames::GetLockType()
{
  return "games";
}

std::string CGUIViewStateWindowGames::GetExtensions()
{
  std::set<std::string> exts = CGameUtils::GetGameExtensions();

  // Ensure .zip appears
  exts.insert(".zip");

  // A smart playlist is listed alongside games, as it is for video and music
  exts.insert(".xsp");

  return StringUtils::Join(exts, "|");
}

std::vector<CMediaSource>& CGUIViewStateWindowGames::GetSources()
{
  std::vector<CMediaSource>* pGameSources = CMediaSourceSettings::GetInstance().GetSources("games");

  // Guard against source type not existing
  if (pGameSources == nullptr)
  {
    static std::vector<CMediaSource> empty;
    return empty;
  }

  return *pGameSources;
}

void CGUIViewStateWindowGames::SaveViewState()
{
  const char* viewState = URIUtils::IsProtocol(m_items.GetPath(), "gamedb") ? "gameslibrary" : "games";
  SaveViewToDb(m_items.GetPath(), WINDOW_GAMES, CViewStateSettings::GetInstance().Get(viewState));
}
