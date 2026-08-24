/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "FileItemList.h"
#include "guilib/GUIDialog.h"
#include "jobs/JobQueue.h"
#include "threads/CriticalSection.h"
#include "view/GUIViewControl.h"

#include <string>

namespace KODI
{
namespace GAME
{

/*!
 * \ingroup games
 *
 * \brief The standings of one leaderboard
 *
 * Reached from the leaderboards list, which records which one was chosen on
 * the achievement runtime - the skin opens this window and has no way to pass
 * anything with it.
 *
 * The player's own row is marked so a skin can pick it out, and is fetched
 * along with the top of the table even when they are far enough down it not to
 * appear: seeing where you stand is the point of looking.
 */
class CDialogGameLeaderboardEntries : public CGUIDialog, protected CJobQueue
{
public:
  CDialogGameLeaderboardEntries();
  ~CDialogGameLeaderboardEntries() override;

  // Implementation of CGUIControl via CGUIDialog
  bool OnMessage(CGUIMessage& message) override;

  // Implementation of CGUIWindow via CGUIDialog
  void OnWindowLoaded() override;
  void OnWindowUnload() override;

protected:
  // Implementation of CGUIWindow via CGUIDialog
  void OnInitWindow() override;
  void OnDeinitWindow(int nextWindowID) override;

  // Implementation of IJobCallback via CJobQueue
  void OnJobComplete(unsigned int jobID, bool success, CJob* job) override;

private:
  //! Build the list from whatever the runtime holds for this leaderboard
  void PopulateList();

  //! Publish the heading and status for the skin
  void SetStatus(const std::string& status);

  CGUIViewControl m_viewControl;
  CFileItemList m_items;

  //! Guards the list against the job thread
  CCriticalSection m_section;

  //! The leaderboard being shown
  unsigned int m_leaderboardId{0};
};

} // namespace GAME
} // namespace KODI
