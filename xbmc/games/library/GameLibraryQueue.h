/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "threads/CriticalSection.h"
#include "jobs/JobQueue.h"

#include <atomic>
#include <memory>
#include <set>
#include <vector>
#include <string>

namespace KODI
{
namespace GAME
{
class CGameInfoScanner;

/*!
 * \ingroup games
 *
 * \brief The jobs that change the game library, run one at a time
 *
 * Scans and cleans queue here so that two never run against the database at
 * once, and so the rest of Kodi can ask whether one is running.
 */
class CGameLibraryQueue : protected CJobQueue
{
public:
  ~CGameLibraryQueue() override;

  static CGameLibraryQueue& GetInstance();

  /*!
   * \brief Scan a folder, or every folder with content when none is given
   *
   * \param showProgress Whether to show the progress bar
   */
  void ScanLibrary(const std::string& directory, bool showProgress = true);

  bool IsScanningLibrary() const { return m_scanning; }
  void StopLibraryScanning();

  /*!
   * \brief Identify and describe one game again, in the background
   *
   * \param interactive Whether the user is watching and may be asked which
   *        game the file is when several catalogue entries share its title
   */
  void RefreshGame(int idGame, bool interactive = false);

  /*!
   * \brief Describe a list of games again
   *
   * For the games a catalogue could not name when they were scanned: asking
   * again later is how they are picked up once a catalogue has learnt them.
   */
  void RefreshGames(std::vector<int> games);

  /*!
   * \brief Remove what no longer exists on disk
   *
   * \param paths Limit the clean to these path IDs, or empty for everything
   * \param modal Block with a progress dialog rather than queue
   */
  bool CleanLibrary(const std::set<int>& paths = {}, bool modal = false);

  bool IsRunning() const;

protected:
  void OnJobComplete(unsigned int jobID, bool success, CJob* job) override;

private:
  CGameLibraryQueue();

  mutable CCriticalSection m_critical;
  std::atomic<bool> m_scanning{false};
  std::atomic<bool> m_cleaning{false};
  CGameInfoScanner* m_scanner{nullptr};
};
} // namespace GAME
} // namespace KODI
