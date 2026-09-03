/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "GameLibraryQueue.h"

#include "GameInfoScanner.h"
#include "GUIUserMessages.h"
#include "jobs/Job.h"
#include "ServiceBroker.h"
#include "dialogs/GUIDialogProgress.h"
#include "games/database/GameDatabase.h"
#include "guilib/GUIComponent.h"
#include "guilib/GUIMessage.h"
#include "guilib/GUIWindowManager.h"
#include "guilib/WindowIDs.h"
#include "interfaces/AnnouncementManager.h"
#include "messaging/helpers/DialogHelper.h"
#include "resources/LocalizeStrings.h"
#include "resources/ResourcesComponent.h"
#include "threads/SingleLock.h"
#include "utils/Variant.h"
#include "utils/log.h"

#include <mutex>

using namespace KODI;
using namespace GAME;

namespace
{
class CGameLibraryScanningJob : public CJob
{
public:
  CGameLibraryScanningJob(std::string directory, bool showProgress, CGameInfoScanner*& slot)
    : m_directory(std::move(directory)), m_showProgress(showProgress), m_slot(slot)
  {
  }

  const char* GetType() const override { return "GameLibraryScanningJob"; }
  bool CanBeCancelled() const { return true; }

  bool DoWork() override
  {
    m_scanner.ShowDialog(m_showProgress);
    m_slot = &m_scanner;
    m_scanner.Start(m_directory);
    m_slot = nullptr;
    return true;
  }

  bool Equals(const CJob* job) const override
  {
    const auto* other = dynamic_cast<const CGameLibraryScanningJob*>(job);
    return other != nullptr && other->m_directory == m_directory;
  }

private:
  CGameInfoScanner m_scanner;
  std::string m_directory;
  bool m_showProgress;
  CGameInfoScanner*& m_slot;
};

class CGameLibraryCleaningJob : public CJob
{
public:
  explicit CGameLibraryCleaningJob(std::set<int> paths) : m_paths(std::move(paths)) {}

  const char* GetType() const override { return "GameLibraryCleaningJob"; }

  bool DoWork() override
  {
    CGameDatabase db;
    if (!db.Open())
      return false;

    auto& announcer = *CServiceBroker::GetAnnouncementManager();
    announcer.Announce(ANNOUNCEMENT::GameLibrary, "OnCleanStarted");
    const int removed = db.CleanDatabase(m_paths);
    announcer.Announce(ANNOUNCEMENT::GameLibrary, "OnCleanFinished");

    CGUIMessage msg(GUI_MSG_NOTIFY_ALL, 0, 0, GUI_MSG_UPDATE);
    CServiceBroker::GetGUI()->GetWindowManager().SendThreadMessage(msg);
    return removed >= 0;
  }

  bool Equals(const CJob* job) const override
  {
    return dynamic_cast<const CGameLibraryCleaningJob*>(job) != nullptr;
  }

private:
  std::set<int> m_paths;
};
} // namespace

CGameLibraryQueue::CGameLibraryQueue() : CJobQueue(false, 1, CJob::PRIORITY_LOW)
{
}

CGameLibraryQueue::~CGameLibraryQueue()
{
  CancelJobs();
}

CGameLibraryQueue& CGameLibraryQueue::GetInstance()
{
  static CGameLibraryQueue instance;
  return instance;
}

void CGameLibraryQueue::ScanLibrary(const std::string& directory, bool showProgress)
{
  std::unique_lock lock(m_critical);
  if (m_scanning)
    return;
  m_scanning = true;
  AddJob(new CGameLibraryScanningJob(directory, showProgress, m_scanner));
}

void CGameLibraryQueue::StopLibraryScanning()
{
  std::unique_lock lock(m_critical);
  if (m_scanner != nullptr)
    m_scanner->Stop();
}

bool CGameLibraryQueue::CleanLibrary(const std::set<int>& paths, bool modal)
{
  if (m_scanning)
  {
    CLog::Log(LOGWARNING, "GAME: Cannot clean the library while it is being scanned");
    return false;
  }

  if (!modal)
  {
    std::unique_lock lock(m_critical);
    m_cleaning = true;
    AddJob(new CGameLibraryCleaningJob(paths));
    return true;
  }

  auto* dialog = CServiceBroker::GetGUI()->GetWindowManager().GetWindow<CGUIDialogProgress>(
      WINDOW_DIALOG_PROGRESS);
  if (dialog != nullptr)
  {
    dialog->SetHeading(CVariant{35547});
    dialog->SetLine(0, CVariant{""});
    dialog->SetLine(1, CVariant{""});
    dialog->SetLine(2, CVariant{""});
    dialog->ShowProgressBar(false);
    dialog->Open();
  }

  m_cleaning = true;
  CGameLibraryCleaningJob job(paths);
  const bool ok = job.DoWork();
  m_cleaning = false;

  if (dialog != nullptr)
    dialog->Close();
  return ok;
}

bool CGameLibraryQueue::IsRunning() const
{
  return m_scanning || m_cleaning;
}

void CGameLibraryQueue::OnJobComplete(unsigned int jobID, bool success, CJob* job)
{
  {
    std::unique_lock lock(m_critical);
    if (dynamic_cast<CGameLibraryScanningJob*>(job) != nullptr)
    {
      m_scanning = false;
      m_scanner = nullptr;
    }
    else if (dynamic_cast<CGameLibraryCleaningJob*>(job) != nullptr)
    {
      m_cleaning = false;
    }
  }
  CJobQueue::OnJobComplete(jobID, success, job);
}
