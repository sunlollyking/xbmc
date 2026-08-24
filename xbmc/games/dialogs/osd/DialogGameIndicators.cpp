/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "DialogGameIndicators.h"

#include "ServiceBroker.h"
#include "games/AchievementRuntime.h"
#include "games/GameServices.h"
#include "guilib/GUIComponent.h"
#include "guilib/GUIWindowManager.h"
#include "guilib/WindowIDs.h"
#include "messaging/ApplicationMessenger.h"

using namespace KODI::GAME;

CDialogGameIndicators::CDialogGameIndicators()
  : CGUIDialog(
        WINDOW_DIALOG_GAME_INDICATORS, "DialogGameIndicators.xml", DialogModalityType::MODELESS)
{
  m_loadType = KEEP_IN_MEMORY;
}

void CDialogGameIndicators::Process(unsigned int currentTime, CDirtyRegionList& dirtyregions)
{
  // Where the game has its own DRM plane the GUI layer is only composited when
  // something dirties it, and a label quietly changing its text is not reliably
  // enough. This is only reached while an indicator is up, so the rest of the
  // session still gets the saving that optimisation exists for.
  MarkDirtyRegion();

  CGUIDialog::Process(currentTime, dirtyregions);
}

void CDialogGameIndicators::Update()
{
  auto& runtime = CServiceBroker::GetGameServices().AchievementRuntime();

  const bool wanted = !runtime.GetChallenges().empty() || runtime.GetProgressIndicator().id != 0 ||
                      !runtime.GetLeaderboardTrackers().empty();

  auto& windowManager = CServiceBroker::GetGUI()->GetWindowManager();

  // GetWindow is safe from any thread; opening one is not, so the change is
  // posted rather than made here - this is called from the game thread
  const CGUIWindow* dialog = windowManager.GetWindow(WINDOW_DIALOG_GAME_INDICATORS);
  if (dialog == nullptr)
    return;

  const bool showing = dialog->IsActive();
  if (wanted == showing)
    return;

  if (wanted)
  {
    CServiceBroker::GetAppMessenger()->PostMsg(TMSG_GUI_ACTIVATE_WINDOW,
                                               WINDOW_DIALOG_GAME_INDICATORS, 0);
  }
  else
  {
    CServiceBroker::GetAppMessenger()->PostMsg(TMSG_GUI_WINDOW_CLOSE, WINDOW_DIALOG_GAME_INDICATORS,
                                               0);
  }
}
