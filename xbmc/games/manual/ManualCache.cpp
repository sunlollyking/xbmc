/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "ManualCache.h"

#include "ServiceBroker.h"
#include "URL.h"
#include "filesystem/File.h"
#include "profiles/ProfileManager.h"
#include "settings/Settings.h"
#include "settings/SettingsComponent.h"
#include "utils/XBMCTinyXML2.h"
#include "utils/log.h"

#include <algorithm>
#include <ctime>
#include <vector>

using namespace KODI::GAME;

namespace
{
constexpr const char* CACHE_FILE = "gamemanualcache.xml";
constexpr const char* ROOT_ELEMENT = "gamemanualcache";
constexpr const char* MANUAL_ELEMENT = "manual";
constexpr const char* PATH_ATTRIBUTE = "path";
constexpr const char* BYTES_ATTRIBUTE = "bytes";
constexpr const char* OPENED_ATTRIBUTE = "lastopened";

const std::string SETTING_GAMES_MANUALCACHESIZE = "gamesgeneral.manualcachesize";

//! What the setting means if it cannot be read
constexpr int DEFAULT_BUDGET_MB = 250;

constexpr uint64_t BYTES_PER_MB = 1024 * 1024;

int64_t Now()
{
  return static_cast<int64_t>(std::time(nullptr));
}
} // namespace

CManualCache& CManualCache::GetInstance()
{
  static CManualCache instance;
  return instance;
}

std::string CManualCache::GetPath()
{
  const auto settings = CServiceBroker::GetSettingsComponent();
  if (!settings)
    return {};

  const auto profileManager = settings->GetProfileManager();
  if (!profileManager)
    return {};

  return profileManager->GetUserDataItem(CACHE_FILE);
}

uint64_t CManualCache::GetBudget()
{
  const auto settingsComponent = CServiceBroker::GetSettingsComponent();
  if (!settingsComponent)
    return static_cast<uint64_t>(DEFAULT_BUDGET_MB) * BYTES_PER_MB;

  const auto settings = settingsComponent->GetSettings();
  if (!settings)
    return static_cast<uint64_t>(DEFAULT_BUDGET_MB) * BYTES_PER_MB;

  const int megabytes = settings->GetInt(SETTING_GAMES_MANUALCACHESIZE);
  if (megabytes <= 0)
    return static_cast<uint64_t>(DEFAULT_BUDGET_MB) * BYTES_PER_MB;

  return static_cast<uint64_t>(megabytes) * BYTES_PER_MB;
}

void CManualCache::Load()
{
  if (m_loaded)
    return;

  // Marked loaded whatever happens, so a missing or broken file is read once
  m_loaded = true;

  const std::string path = GetPath();
  if (path.empty() || !XFILE::CFile::Exists(path))
    return;

  CXBMCTinyXML2 doc;
  if (!doc.LoadFile(path))
  {
    CLog::Log(LOGERROR, "CManualCache: unable to load {} (line {})", path, doc.ErrorLineNum());
    return;
  }

  const auto* root = doc.RootElement();
  if (root == nullptr || std::string(root->Value()) != ROOT_ELEMENT)
  {
    CLog::Log(LOGERROR, "CManualCache: {} has no <{}> root element", path, ROOT_ELEMENT);
    return;
  }

  for (const auto* manual = root->FirstChildElement(MANUAL_ELEMENT); manual != nullptr;
       manual = manual->NextSiblingElement(MANUAL_ELEMENT))
  {
    const char* manualPath = manual->Attribute(PATH_ATTRIBUTE);
    if (manualPath == nullptr || *manualPath == '\0')
      continue;

    CacheEntry entry;

    int64_t bytes = 0;
    if (manual->QueryInt64Attribute(BYTES_ATTRIBUTE, &bytes) != tinyxml2::XML_SUCCESS || bytes < 0)
      continue;
    entry.bytes = static_cast<uint64_t>(bytes);

    // An entry written before this attribute existed sorts as oldest, which is
    // the right answer for something never seen being opened
    manual->QueryInt64Attribute(OPENED_ATTRIBUTE, &entry.lastOpened);

    m_entries[manualPath] = entry;
  }

  CLog::Log(LOGDEBUG, "CManualCache: tracking {} downloaded manual(s)", m_entries.size());
}

void CManualCache::Save()
{
  const std::string path = GetPath();
  if (path.empty())
    return;

  CXBMCTinyXML2 doc;
  auto* element = doc.NewElement(ROOT_ELEMENT);
  auto* root = doc.InsertEndChild(element);
  if (root == nullptr)
    return;

  for (const auto& [manualPath, entry] : m_entries)
  {
    auto* manual = doc.NewElement(MANUAL_ELEMENT);
    manual->SetAttribute(PATH_ATTRIBUTE, manualPath.c_str());
    manual->SetAttribute(BYTES_ATTRIBUTE, static_cast<int64_t>(entry.bytes));
    manual->SetAttribute(OPENED_ATTRIBUTE, entry.lastOpened);
    root->InsertEndChild(manual);
  }

  if (!doc.SaveFile(path))
    CLog::Log(LOGERROR, "CManualCache: unable to save {}", path);
}

void CManualCache::Forget(const std::string& path)
{
  m_entries.erase(path);
}

void CManualCache::Add(const std::string& path, uint64_t bytes)
{
  if (path.empty())
    return;

  std::unique_lock<std::mutex> lock(m_mutex);

  Load();

  CacheEntry entry;
  entry.bytes = bytes;

  // Counted as opened now: it was just fetched to be read, and starting its
  // life as the least recently used would make it the first thing evicted
  entry.lastOpened = Now();

  m_entries[path] = entry;

  Save();
}

void CManualCache::Touch(const std::string& path)
{
  if (path.empty())
    return;

  std::unique_lock<std::mutex> lock(m_mutex);

  Load();

  const auto it = m_entries.find(path);

  // Not tracked means the player supplied it, and it is none of our business
  if (it == m_entries.end())
    return;

  it->second.lastOpened = Now();

  Save();
}

uint64_t CManualCache::GetSize()
{
  std::unique_lock<std::mutex> lock(m_mutex);

  Load();

  uint64_t total = 0;
  for (const auto& [manualPath, entry] : m_entries)
    total += entry.bytes;

  return total;
}

void CManualCache::Enforce(const std::string& keep)
{
  std::unique_lock<std::mutex> lock(m_mutex);

  Load();

  const uint64_t budget = GetBudget();

  // Oldest first, so the front of the list is what goes
  std::vector<std::pair<std::string, CacheEntry>> ordered(m_entries.begin(), m_entries.end());
  std::sort(ordered.begin(), ordered.end(),
            [](const auto& lhs, const auto& rhs) { return lhs.second.lastOpened < rhs.second.lastOpened; });

  uint64_t total = 0;
  for (const auto& [manualPath, entry] : ordered)
    total += entry.bytes;

  if (total <= budget)
    return;

  bool changed = false;

  for (const auto& [manualPath, entry] : ordered)
  {
    if (total <= budget)
      break;

    if (manualPath == keep)
      continue;

    // A manual deleted by hand still counts against the budget until its entry
    // goes, so a missing file is dropped from the accounting rather than
    // treated as a failure
    if (XFILE::CFile::Exists(manualPath) && !XFILE::CFile::Delete(manualPath))
    {
      CLog::Log(LOGWARNING, "CManualCache: could not delete \"{}\"",
                CURL::GetRedacted(manualPath));
      continue;
    }

    CLog::Log(LOGINFO, "CManualCache: freed {} bytes by removing \"{}\"", entry.bytes,
              CURL::GetRedacted(manualPath));

    total -= std::min(total, entry.bytes);
    Forget(manualPath);
    changed = true;
  }

  if (changed)
    Save();
}

void CManualCache::Clear()
{
  std::unique_lock<std::mutex> lock(m_mutex);

  Load();

  for (const auto& [manualPath, entry] : m_entries)
  {
    if (XFILE::CFile::Exists(manualPath) && !XFILE::CFile::Delete(manualPath))
      CLog::Log(LOGWARNING, "CManualCache: could not delete \"{}\"", CURL::GetRedacted(manualPath));
  }

  m_entries.clear();

  Save();
}
