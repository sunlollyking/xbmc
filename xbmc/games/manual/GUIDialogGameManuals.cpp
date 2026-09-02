/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "GUIDialogGameManuals.h"

#include "FileItem.h"
#include "ServiceBroker.h"
#include "URL.h"
#include "addons/AddonManager.h"
#include "addons/addoninfo/AddonInfo.h"
#include "addons/addoninfo/AddonType.h"
#include "addons/gui/GUIDialogAddonInfo.h"
#include "ManualCache.h"
#include "filesystem/Directory.h"
#include "filesystem/File.h"
#include "games/GameManual.h"
#include "guilib/GUIComponent.h"
#include "guilib/GUIMessage.h"
#include "guilib/GUIWindowManager.h"
#include "guilib/WindowIDs.h"
#include "jobs/Job.h"
#include "resources/LocalizeStrings.h"
#include "resources/ResourcesComponent.h"
#include "utils/StringUtils.h"
#include "utils/URIUtils.h"
#include "utils/log.h"

#include <mutex>

using namespace KODI::GAME;

namespace
{
// 100 is the header the dialog background include owns
constexpr int CONTROL_PROVIDER_LIST = 120;
constexpr int CONTROL_RESULT_LIST = 110;

constexpr const char* PROPERTY_STATUS = "Manuals.Status";
constexpr const char* PROPERTY_GAME = "Manuals.Game";
constexpr const char* PROPERTY_PROVIDER = "Manuals.Provider";

//! What a plugin has to say it provides to be offered here. Unrecognised
//! tokens are kept as written, so this needs no add-on type of its own.
constexpr const char* PROVIDES_MANUALS = "manuals";

/*!
 * \brief Asks a provider what it has, off the GUI thread
 *
 * A provider is a plugin, so asking it is a directory listing - the same
 * mechanism the subtitles dialog uses.
 */
class CManualSearchJob : public CJob
{
public:
  explicit CManualSearchJob(const CURL& url) : m_url(url) {}

  const char* GetType() const override { return "manual-search"; }

  bool DoWork() override
  {
    XFILE::CDirectory::GetDirectory(m_url.Get(), m_results, "", XFILE::DIR_FLAG_DEFAULTS);
    return true;
  }

  const CFileItemList& GetResults() const { return m_results; }

private:
  const CURL m_url;
  CFileItemList m_results;
};

/*!
 * \brief Fetches one manual, off the GUI thread
 */
class CManualDownloadJob : public CJob
{
public:
  CManualDownloadJob(std::string source, std::string target)
    : m_source(std::move(source)),
      m_target(std::move(target))
  {
  }

  const char* GetType() const override { return "manual-download"; }

  bool DoWork() override
  {
    const std::string folder = URIUtils::GetDirectory(m_target);
    if (!folder.empty() && !XFILE::CDirectory::Exists(folder))
      XFILE::CDirectory::Create(folder);

    // Copy() reads through the virtual filesystem, so a provider can answer
    // with anything Kodi can open, not only an http URL
    return XFILE::CFile::Copy(m_source, m_target);
  }

  const std::string& GetTarget() const { return m_target; }

private:
  const std::string m_source;
  const std::string m_target;
};
} // namespace

CGUIDialogGameManuals::CGUIDialogGameManuals()
  : CGUIDialog(WINDOW_DIALOG_GAME_MANUALS, "DialogGameManuals.xml"),
    CJobQueue(false, 1)
{
  m_loadType = LOAD_ON_GUI_INIT;
}

CGUIDialogGameManuals::~CGUIDialogGameManuals() = default;

void CGUIDialogGameManuals::OnInitWindow()
{
  CGUIDialog::OnInitWindow();

  std::string game = URIUtils::GetFileName(m_gamePath);
  const size_t extension = game.find_last_of('.');
  if (extension != std::string::npos)
    game.erase(extension);

  SetProperty(PROPERTY_GAME, game);

  m_results.Clear();
  FillProviders();

  if (!m_provider.empty())
    Search();
}

void CGUIDialogGameManuals::OnDeinitWindow(int nextWindowID)
{
  CancelJobs();

  {
    std::unique_lock lock(m_section);
    m_providers.Clear();
    m_results.Clear();
    m_updateResults = false;
  }

  m_gamePath.clear();
  m_downloadTarget.clear();

  // Only ours: ClearProperties() would take the window's own "xmlfile" with
  // them, and AllocResources reloads the skin file from that property. Wiping
  // it leaves the dialog unable to load after the next skin change - it inits
  // with no XML, draws nothing, and logs no error at all.
  for (const char* property : {PROPERTY_STATUS, PROPERTY_GAME, PROPERTY_PROVIDER})
    SetProperty(property, "");

  CGUIDialog::OnDeinitWindow(nextWindowID);
}

void CGUIDialogGameManuals::FillProviders()
{
  std::unique_lock lock(m_section);

  m_providers.Clear();
  m_provider.clear();

  std::vector<ADDON::AddonInfoPtr> plugins;
  CServiceBroker::GetAddonMgr().GetAddonInfos(plugins, true, ADDON::AddonType::PLUGIN);

  for (const auto& plugin : plugins)
  {
    const ADDON::CAddonType* type = plugin->Type(ADDON::AddonType::PLUGIN);
    if (type == nullptr)
      continue;

    // "provides" is a space separated list. Kodi maps the values it knows to
    // content types and keeps the rest verbatim, which is what lets a plugin
    // mark itself as a manual provider without a new add-on type.
    const std::vector<std::string> provides =
        StringUtils::Split(type->GetValue("provides").asString(), ' ');

    if (std::find(provides.begin(), provides.end(), PROVIDES_MANUALS) == provides.end())
      continue;

    CFileItemPtr item = std::make_shared<CFileItem>(plugin->Name());
    item->SetPath("plugin://" + plugin->ID() + "/");
    item->SetProperty("Addon.ID", plugin->ID());
    item->SetArt("icon", plugin->Icon());

    m_providers.Add(item);
  }

  if (m_providers.IsEmpty())
  {
    lock.unlock();
    SetStatus(Status::NO_PROVIDERS);
    return;
  }

  // The first is used until there is somewhere to remember a preference. With
  // one provider installed, which is the normal case, there is no choice to
  // make anyway.
  m_provider = m_providers[0]->GetProperty("Addon.ID").asString();

  lock.unlock();

  UpdateProviderList();
}

void CGUIDialogGameManuals::UpdateProviderList()
{
  std::unique_lock lock(m_section);

  CGUIMessage message(GUI_MSG_LABEL_BIND, GetID(), CONTROL_PROVIDER_LIST, 0, 0, &m_providers);
  OnMessage(message);

  for (int i = 0; i < m_providers.Size(); ++i)
  {
    if (m_providers[i]->GetProperty("Addon.ID").asString() == m_provider)
    {
      SetProperty(PROPERTY_PROVIDER, m_providers[i]->GetLabel());
      break;
    }
  }
}

void CGUIDialogGameManuals::Search()
{
  if (m_provider.empty() || m_gamePath.empty())
    return;

  SetStatus(Status::SEARCHING);

  {
    std::unique_lock lock(m_section);
    m_results.Clear();
  }

  CURL url("plugin://" + m_provider + "/");
  url.SetOption("action", "search");

  // The path is what is handed over, not a hash or a title. How a provider
  // decides what matches is its own business: one may hash the ROM, another
  // may only be able to compare titles.
  url.SetOption("path", m_gamePath);

  std::string title = URIUtils::GetFileName(m_gamePath);
  const size_t extension = title.find_last_of('.');
  if (extension != std::string::npos)
    title.erase(extension);
  url.SetOption("title", title);

  AddJob(new CManualSearchJob(url));
}

void CGUIDialogGameManuals::OnSearchComplete(const CFileItemList& results)
{
  {
    std::unique_lock lock(m_section);

    m_results.Clear();
    m_results.Copy(results);

    // This runs on the job thread. Binding a control from here does not work,
    // so the list is marked for the next frame instead.
    m_updateResults = true;
  }

  SetStatus(results.IsEmpty() ? Status::NO_RESULTS : Status::RESULTS);

  MarkDirtyRegion();
}

void CGUIDialogGameManuals::Process(unsigned int currentTime, CDirtyRegionList& dirtyregions)
{
  if (m_updateResults)
  {
    std::unique_lock lock(m_section);
    m_updateResults = false;

    CGUIMessage message(GUI_MSG_LABEL_BIND, GetID(), CONTROL_RESULT_LIST, 0, 0, &m_results);
    OnMessage(message);

    const bool haveResults = !m_results.IsEmpty();
    lock.unlock();

    if (haveResults)
    {
      CGUIMessage focus(GUI_MSG_SETFOCUS, GetID(), CONTROL_RESULT_LIST);
      OnMessage(focus);
    }
  }

  CGUIDialog::Process(currentTime, dirtyregions);
}

void CGUIDialogGameManuals::Download(const CFileItem& manual)
{
  bool writable = false;
  std::string target = CGameManual::GetDownloadPath(m_gamePath, writable);

  if (target.empty())
  {
    SetStatus(Status::FAILED);
    return;
  }

  if (!writable)
  {
    // A games folder is often somewhere that cannot be written to. Keeping
    // the manual in the profile means it is still there next time, which a
    // temporary folder would not guarantee.
    CLog::Log(LOGINFO,
              "CGUIDialogGameManuals: \"{}\" is not writable, keeping the manual in the "
              "profile instead",
              URIUtils::GetDirectory(target));

    target =
        URIUtils::AddFileToFolder("special://profile/gamemanuals/", URIUtils::GetFileName(target));
  }

  m_downloadTarget = target;

  SetStatus(Status::DOWNLOADING);

  AddJob(new CManualDownloadJob(manual.GetPath(), target));
}

void CGUIDialogGameManuals::OnDownloadComplete(bool success, const std::string& path)
{
  if (!success)
  {
    CLog::Log(LOGERROR, "CGUIDialogGameManuals: could not fetch a manual to \"{}\"", path);
    SetStatus(Status::FAILED);
    return;
  }

  CLog::Log(LOGINFO, "CGUIDialogGameManuals: fetched \"{}\"", path);

  // Tracked so that it counts against the player's budget, and so that it is
  // one of the files eviction is allowed to take back. Only what Kodi
  // downloaded is ever tracked, and only what is tracked is ever deleted.
  struct __stat64 statBuffer = {};
  if (XFILE::CFile::Stat(path, &statBuffer) == 0 && statBuffer.st_size > 0)
    CManualCache::GetInstance().Add(path, static_cast<uint64_t>(statBuffer.st_size));

  // Spare the manual just fetched: the player is about to read it, and it
  // would be absurd to download something and immediately delete it
  CManualCache::GetInstance().Enforce(path);

  Close();

  CServiceBroker::GetGUI()->GetWindowManager().ActivateWindow(WINDOW_GAME_MANUAL, path);
}

void CGUIDialogGameManuals::SetStatus(Status status, const std::string& detail)
{
  const auto& strings = CServiceBroker::GetResourcesComponent().GetLocalizeStrings();

  std::string text;

  switch (status)
  {
    case Status::NO_PROVIDERS:
      // "No add-on is installed that can find manuals"
      text = strings.Get(35320);
      break;
    case Status::SEARCHING:
      // "Searching…"
      text = strings.Get(35321);
      break;
    case Status::NO_RESULTS:
      // "No manual was found for this game"
      text = strings.Get(35322);
      break;
    case Status::RESULTS:
      text.clear();
      break;
    case Status::DOWNLOADING:
      // "Downloading…"
      text = strings.Get(35323);
      break;
    case Status::FAILED:
      // "The manual could not be downloaded"
      text = strings.Get(35324);
      break;
  }

  if (!detail.empty())
    text = detail;

  SetProperty(PROPERTY_STATUS, text);
}

void CGUIDialogGameManuals::OnJobComplete(unsigned int jobID, bool success, CJob* job)
{
  if (StringUtils::EqualsNoCase(job->GetType(), "manual-search"))
  {
    // Copied out here because the job is destroyed as soon as this returns
    const CFileItemList& results = static_cast<CManualSearchJob*>(job)->GetResults();

    CFileItemList copy;
    copy.Copy(results);

    OnSearchComplete(copy);
  }
  else if (StringUtils::EqualsNoCase(job->GetType(), "manual-download"))
  {
    OnDownloadComplete(success, static_cast<CManualDownloadJob*>(job)->GetTarget());
  }

  CJobQueue::OnJobComplete(jobID, success, job);
}

bool CGUIDialogGameManuals::OnMessage(CGUIMessage& message)
{
  switch (message.GetMessage())
  {
    case GUI_MSG_WINDOW_INIT:
    {
      const std::string path = message.GetStringParam();
      if (!path.empty())
        m_gamePath = path;

      break;
    }
    case GUI_MSG_CLICKED:
    {
      const int control = message.GetSenderId();

      if (control == CONTROL_RESULT_LIST)
      {
        CGUIMessage selected(GUI_MSG_ITEM_SELECTED, GetID(), CONTROL_RESULT_LIST);
        OnMessage(selected);

        const int index = selected.GetParam1();

        std::unique_lock lock(m_section);
        if (index >= 0 && index < m_results.Size())
        {
          const CFileItemPtr item = m_results[index];
          lock.unlock();

          Download(*item);
        }

        return true;
      }

      if (control == CONTROL_PROVIDER_LIST)
      {
        CGUIMessage selected(GUI_MSG_ITEM_SELECTED, GetID(), CONTROL_PROVIDER_LIST);
        OnMessage(selected);

        const int index = selected.GetParam1();

        std::unique_lock lock(m_section);
        if (index >= 0 && index < m_providers.Size())
        {
          const std::string provider = m_providers[index]->GetProperty("Addon.ID").asString();
          lock.unlock();

          if (provider != m_provider)
          {
            m_provider = provider;
            UpdateProviderList();
            Search();
          }
        }

        return true;
      }

      break;
    }
    default:
      break;
  }

  return CGUIDialog::OnMessage(message);
}
