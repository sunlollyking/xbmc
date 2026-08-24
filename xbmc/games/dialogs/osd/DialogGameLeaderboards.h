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

namespace KODI
{
namespace GAME
{

/*!
 * \ingroup games
 *
 * \brief The leaderboards of the game being played
 *
 * A leaderboard is a scored challenge - a fastest lap, a highest score on one
 * level - and a game may have dozens. The list shows what the game offers,
 * where the player stands on each, and who holds the top place; choosing one
 * opens its standings.
 *
 * The definitions come from the same RetroAchievements response the
 * achievements do, so opening this costs nothing. The standings do not: each
 * leaderboard is a separate request, so they are fetched in the background
 * and the rows fill in as answers arrive.
 *
 * Browsing only. Submitting a score requires hardcore mode, which
 * RetroAchievements only grants an emulator once it has been approved.
 */
class CDialogGameLeaderboards : public CGUIDialog, protected CJobQueue
{
public:
  CDialogGameLeaderboards();
  ~CDialogGameLeaderboards() override;

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
  //! Build the list from whatever the runtime currently holds
  void PopulateList();

  //! Ask for the standings of every leaderboard, one job at a time
  void FetchStandings();

  //! A one-line summary of what a leaderboard measures and which way it ranks
  static std::string DescribeFormat(const std::string& format, bool lowerIsBetter);

  CGUIViewControl m_viewControl;
  CFileItemList m_items;

  //! Guards the list against the job thread
  CCriticalSection m_section;

  //! Where the player was, so reopening after looking at one leaderboard does
  //! not drop them back at the top of a long list
  int m_lastSelected{-1};
};

} // namespace GAME
} // namespace KODI
