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

#include <atomic>
#include <memory>
#include <string>

class CFileItem;

namespace KODI
{
namespace GAME
{

/*!
 * \brief Finds a manual for a game that has none locally
 *
 * Modelled on the subtitles dialog, and works the same way: a provider is an
 * ordinary plugin, asked for results with a directory call and answering with
 * a list. Kodi never talks to any particular service.
 *
 * A provider is recognised by carrying "manuals" among the things its plugin
 * extension point says it provides. That is an existing mechanism - unknown
 * tokens are kept as they are - so no add-on type had to be invented for
 * something far less common than subtitles.
 *
 * Opened with the game's path as the first window parameter.
 */
class CGUIDialogGameManuals : public CGUIDialog, public CJobQueue
{
public:
  CGUIDialogGameManuals();
  ~CGUIDialogGameManuals() override;

  // Implementation of CGUIControl via CGUIDialog
  bool OnMessage(CGUIMessage& message) override;

protected:
  // Implementation of CGUIWindow via CGUIDialog
  void OnInitWindow() override;
  void OnDeinitWindow(int nextWindowID) override;

  // Implementation of CGUIControl via CGUIDialog
  void Process(unsigned int currentTime, CDirtyRegionList& dirtyregions) override;

  // Implementation of CJobQueue
  void OnJobComplete(unsigned int jobID, bool success, CJob* job) override;
  void OnJobProgress(unsigned int jobID,
                     unsigned int progress,
                     unsigned int total,
                     const CJob* job) override;

private:
  enum class Status
  {
    NO_PROVIDERS,
    SEARCHING,
    NO_RESULTS,
    RESULTS,
    DOWNLOADING,
    FAILED,
  };

  /*!
   * \brief List the plugins that say they can provide manuals
   */
  void FillProviders();

  /*!
   * \brief Ask the chosen provider what it has for this game
   *
   * The game's path is handed over rather than a hash or a title, because
   * what a provider matches on is its own business - one may hash the ROM,
   * another may only know titles.
   */
  void Search();

  void OnSearchComplete(const CFileItemList& results);

  /*!
   * \brief Fetch a manual the provider offered and show it
   */
  void Download(const CFileItem& manual);
  void OnDownloadComplete(bool success, const std::string& path);

  void SetStatus(Status status, const std::string& detail = "");

  //! Brings up, updates and closes Kodi's standard progress dialog. Driven
  //! from Process() so that everything touching the GUI happens on the GUI
  //! thread, while the percentage arrives from the job thread.
  void UpdateDownloadProgress();

  //! Publish the lists and status for the skin
  void UpdateProviderList();

  std::string m_gamePath;

  //! The provider being asked, and the one to ask again next time
  std::string m_provider;

  CFileItemList m_providers;
  CFileItemList m_results;

  //! Guards the lists, which a finished job writes to
  CCriticalSection m_section;

  //! Where a download in flight will be written
  std::string m_downloadTarget;

  //! Results arrive on a job thread, but a control can only be bound on the
  //! GUI thread, so the binding waits for the next frame
  bool m_updateResults{false};

  //! Set once there are results to focus, and cleared only once the focus has
  //! actually landed. The list is hidden while a status is showing, and
  //! whether it is visible is worked out during rendering - so on the frame
  //! the results are bound it can still be invisible, and a control that
  //! cannot be seen cannot be focused. One attempt was silently losing that
  //! race and leaving a dialog nothing could be pressed in.
  bool m_focusResults{false};

  //! A manual is fetched in the background, so without this a large one over
  //! a slow link is indistinguishable from Kodi having hung
  std::atomic<bool> m_downloadActive{false};
  std::atomic<int> m_downloadPercent{0};

  //! Whether the progress dialog on screen is the one we opened
  bool m_progressShown{false};
};

} // namespace GAME
} // namespace KODI
