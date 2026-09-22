/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "ManualPosition.h"

#include "ServiceBroker.h"
#include "filesystem/File.h"
#include "profiles/ProfileManager.h"
#include "settings/SettingsComponent.h"
#include "utils/XBMCTinyXML2.h"
#include "utils/log.h"

#include <vector>

using namespace KODI::GAME;

namespace
{
constexpr const char* POSITIONS_FILE = "gamemanuals.xml";
constexpr const char* ROOT_ELEMENT = "gamemanuals";
constexpr const char* MANUAL_ELEMENT = "manual";
constexpr const char* PATH_ATTRIBUTE = "path";
constexpr const char* PAGE_ATTRIBUTE = "page";

//! Positions are worth keeping but not worth growing without bound. When the
//! file is full the oldest entries are not tracked, so the newest are dropped
//! instead - it is a convenience, not a record.
constexpr size_t MAX_POSITIONS = 500;
} // namespace

CManualPosition& CManualPosition::GetInstance()
{
  static CManualPosition instance;
  return instance;
}

std::string CManualPosition::GetPath()
{
  const auto settings = CServiceBroker::GetSettingsComponent();
  if (!settings)
    return {};

  const auto profileManager = settings->GetProfileManager();
  if (!profileManager)
    return {};

  return profileManager->GetUserDataItem(POSITIONS_FILE);
}

void CManualPosition::Load()
{
  if (m_loaded)
    return;

  // Marked loaded whatever happens, so that a missing or broken file is read
  // once rather than on every page turn
  m_loaded = true;

  const std::string path = GetPath();
  if (path.empty() || !XFILE::CFile::Exists(path))
    return;

  CXBMCTinyXML2 doc;
  if (!doc.LoadFile(path))
  {
    CLog::Log(LOGERROR, "CManualPosition: unable to load {} (line {})", path, doc.ErrorLineNum());
    return;
  }

  const auto* root = doc.RootElement();
  if (root == nullptr || std::string(root->Value()) != ROOT_ELEMENT)
  {
    CLog::Log(LOGERROR, "CManualPosition: {} has no <{}> root element", path, ROOT_ELEMENT);
    return;
  }

  for (const auto* manual = root->FirstChildElement(MANUAL_ELEMENT); manual != nullptr;
       manual = manual->NextSiblingElement(MANUAL_ELEMENT))
  {
    const char* manualPath = manual->Attribute(PATH_ATTRIBUTE);
    if (manualPath == nullptr || *manualPath == '\0')
      continue;

    unsigned int page = 0;
    if (manual->QueryUnsignedAttribute(PAGE_ATTRIBUTE, &page) != tinyxml2::XML_SUCCESS)
      continue;

    if (page > 0)
      m_positions[manualPath] = page;
  }

  CLog::Log(LOGDEBUG, "CManualPosition: read {} position(s)", m_positions.size());
}

void CManualPosition::Save()
{
  const std::string path = GetPath();
  if (path.empty())
    return;

  CXBMCTinyXML2 doc;
  auto* element = doc.NewElement(ROOT_ELEMENT);
  auto* root = doc.InsertEndChild(element);
  if (root == nullptr)
    return;

  for (const auto& [manualPath, page] : m_positions)
  {
    auto* manual = doc.NewElement(MANUAL_ELEMENT);
    manual->SetAttribute(PATH_ATTRIBUTE, manualPath.c_str());
    manual->SetAttribute(PAGE_ATTRIBUTE, page);
    root->InsertEndChild(manual);
  }

  if (!doc.SaveFile(path))
    CLog::Log(LOGERROR, "CManualPosition: unable to save {}", path);
}

unsigned int CManualPosition::GetPage(const std::string& path)
{
  if (path.empty())
    return 0;

  std::unique_lock<std::mutex> lock(m_mutex);

  Load();

  const auto it = m_positions.find(path);
  if (it == m_positions.end())
    return 0;

  return it->second;
}

void CManualPosition::SetPage(const std::string& path, unsigned int page)
{
  if (path.empty())
    return;

  std::unique_lock<std::mutex> lock(m_mutex);

  Load();

  const auto it = m_positions.find(path);

  if (page == 0)
  {
    // Back at the cover is the same as never having read it
    if (it == m_positions.end())
      return;

    m_positions.erase(it);
  }
  else
  {
    if (it != m_positions.end())
    {
      if (it->second == page)
        return;

      it->second = page;
    }
    else
    {
      if (m_positions.size() >= MAX_POSITIONS)
      {
        CLog::Log(LOGDEBUG, "CManualPosition: {} positions stored, not recording \"{}\"",
                  m_positions.size(), path);
        return;
      }

      m_positions[path] = page;
    }
  }

  Save();
}
