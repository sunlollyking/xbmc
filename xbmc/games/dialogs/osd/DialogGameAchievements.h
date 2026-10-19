/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "games/AchievementRuntime.h"
#include "guilib/GUIDialog.h"
#include "jobs/JobQueue.h"

#include <memory>
#include <optional>

class CFileItemList;
class CGUIMessage;
class CGUIViewControl;

namespace KODI
{
namespace GAME
{
/*!
 * \ingroup games
 *
 * \brief Lists a game's achievements, earned ones first
 *
 * While a game is playing the list comes from the achievement runtime, which
 * the game add-on keeps up to date as achievements unlock.
 *
 * Opened from the library with nothing playing there is no runtime to ask, so
 * the set is fetched from RetroAchievements for the game the list is showing,
 * which is identified by its ``retroachievements`` unique id. That answer is
 * the player's own progress at the moment of asking rather than a stored copy
 * that would drift as they play.
 *
 * Badge images are remote URLs resolved by Kodi's texture cache.
 */
class CDialogGameAchievements : public CGUIDialog, protected CJobQueue
{
public:
  CDialogGameAchievements();
  ~CDialogGameAchievements() override;

  // Implementation of CGUIControl via CGUIDialog
  bool OnMessage(CGUIMessage& message) override;

protected:
  // Implementation of CGUIWindow via CGUIDialog
  void OnWindowLoaded() override;
  void OnWindowUnload() override;
  void OnInitWindow() override;
  void OnDeinitWindow(int nextWindowID) override;

  // Implementation of IJobCallback via CJobQueue
  void OnJobComplete(unsigned int jobID, bool success, CJob* job) override;

private:
  /*!
   * \brief Close the dialog without it ever being drawn
   *
   * Used when OnInitWindow() finds there is nothing to show.
   */
  void Abort();

  /*!
   * \brief Rebuild the list from whichever source is describing this game
   */
  void RefreshList();

  /*!
   * \brief The achievements to show: the fetched set if there is one, else the
   *        playing game's
   */
  AchievementState CurrentState() const;

  /*!
   * \brief Ask RetroAchievements about the game the list is showing
   *
   * \return True if a request was sent and its answer is worth waiting for
   */
  bool FetchForLibraryGame();

  /*!
   * \brief Act on the hardcore toggle
   *
   * Turning hardcore on restarts the game, so it is asked about first. The
   * restart itself comes back from the add-on, which will not let a session
   * begun in casual mode carry on into hardcore.
   */
  void OnHardcoreToggled();

  // Dialog parameters
  std::unique_ptr<CFileItemList> m_items;
  std::unique_ptr<CGUIViewControl> m_viewControl;

  //! Set only when the dialog was opened away from a playing game
  std::optional<AchievementState> m_fetched;
};
} // namespace GAME
} // namespace KODI
