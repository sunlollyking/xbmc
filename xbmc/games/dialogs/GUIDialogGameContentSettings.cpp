/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "GUIDialogGameContentSettings.h"

#include "FileItem.h"
#include "FileItemList.h"
#include "ServiceBroker.h"
#include "addons/AddonManager.h"
#include "addons/addoninfo/AddonType.h"
#include "dialogs/GUIDialogSelect.h"
#include "dialogs/GUIDialogYesNo.h"
#include "filesystem/Directory.h"
#include "games/VideoFilters.h"
#include "games/addons/GameClient.h"
#include "games/library/PlatformCatalogue.h"
#include "guilib/GUIComponent.h"
#include "guilib/GUIWindowManager.h"
#include "guilib/WindowIDs.h"
#include "resources/LocalizeStrings.h"
#include "resources/ResourcesComponent.h"
#include "settings/lib/Setting.h"
#include "settings/windows/GUIControlSettings.h"
#include "utils/StringUtils.h"
#include "utils/URIUtils.h"
#include "utils/Variant.h"
#include "utils/log.h"

#include <algorithm>

using namespace KODI;
using namespace GAME;

namespace
{
constexpr const char* SETTING_PLATFORM = "gamecontent.platform";
constexpr const char* SETTING_SCRAPER = "gamecontent.scraper";
constexpr const char* SETTING_SCAN_RECURSIVE = "gamecontent.scanrecursive";
constexpr const char* SETTING_USE_FOLDER_NAMES = "gamecontent.usefoldernames";
constexpr const char* SETTING_NO_UPDATE = "gamecontent.noupdate";
constexpr const char* SETTING_EXCLUDE = "gamecontent.exclude";
constexpr const char* SETTING_GAME_CLIENT = "gamecontent.gameclient";
constexpr const char* SETTING_VIDEO_FILTER = "gamecontent.videofilter";

std::string Localize(int id)
{
  return CServiceBroker::GetResourcesComponent().GetLocalizeStrings().Get(id);
}

std::vector<std::string> FolderExtensions(const std::string& folder)
{
  std::vector<std::string> extensions;
  CFileItemList items;
  if (!XFILE::CDirectory::GetDirectory(folder, items, "", XFILE::DIR_FLAG_NO_FILE_INFO))
    return extensions;
  for (const auto& item : items)
  {
    if (item->IsFolder())
      continue;
    std::string ext = StringUtils::ToLower(URIUtils::GetExtension(item->GetPath()));
    if (!ext.empty() && ext.front() == '.')
      ext.erase(0, 1);
    if (!ext.empty() && std::ranges::find(extensions, ext) == extensions.end())
      extensions.emplace_back(std::move(ext));
  }
  return extensions;
}
} // namespace

CGUIDialogGameContentSettings::CGUIDialogGameContentSettings()
  : CGUIDialogSettingsManualBase(WINDOW_DIALOG_GAME_CONTENT_SETTINGS, "DialogSettings.xml")
{
}

CGUIDialogGameContentSettings::~CGUIDialogGameContentSettings() = default;

bool CGUIDialogGameContentSettings::Show(const std::string& folder, bool& scanNow)
{
  scanNow = false;

  auto* dialog = CServiceBroker::GetGUI()->GetWindowManager().GetWindow<CGUIDialogGameContentSettings>(
      WINDOW_DIALOG_GAME_CONTENT_SETTINGS);
  if (dialog == nullptr)
    return false;

  dialog->SetFolder(folder);
  dialog->Open();

  if (!dialog->IsConfirmed() || !dialog->m_saved)
    return false;

  if (!dialog->m_platformSlug.empty() && !dialog->m_exclude)
    scanNow = CGUIDialogYesNo::ShowAndGetInput(CVariant{35540}, CVariant{35548});

  return true;
}

void CGUIDialogGameContentSettings::SetFolder(const std::string& folder)
{
  m_folder = folder;
  URIUtils::AddSlashAtEnd(m_folder);
  m_saved = false;

  if (!m_catalogue)
  {
    m_catalogue = std::make_unique<CPlatformCatalogue>();
    m_catalogue->Load();
  }

  m_platformSlug.clear();
  m_scraperId.clear();
  m_scanRecursive = true;
  m_useFolderNames = false;
  m_exclude = false;
  m_noUpdate = false;

  CGameDatabase db;
  if (db.Open())
  {
    GamePathContent content;
    bool foundDirectly = false;
    if (db.GetPathContent(m_folder, content, foundDirectly) && foundDirectly)
    {
      PlatformInfo platform;
      if (db.GetPlatform(content.idPlatform, platform))
        m_platformSlug = platform.slug;
      m_scraperId = content.scraper;
      m_scanRecursive = content.scanRecursive;
      m_useFolderNames = content.useFolderNames;
      m_noUpdate = content.noUpdate;
    }
    else if (content.exclude)
    {
      m_exclude = true;
    }
    m_gameClient = db.GameClients().GetGameClient(m_folder);
    m_videoFilter = db.VideoFilters().GetVideoFilter(m_folder);

    // What the machine already plays with, where this folder says nothing
    if (content.idPlatform > 0 && (m_gameClient.empty() || m_videoFilter.empty()))
    {
      PlatformInfo known;
      if (db.GetPlatform(content.idPlatform, known))
      {
        if (m_gameClient.empty())
          m_gameClient = known.defaultGameClient;
        if (m_videoFilter.empty())
          m_videoFilter = known.defaultVideoFilter;
      }
    }
  }

  if (m_platformSlug.empty() && m_catalogue->IsLoaded())
  {
    const PlatformInfo* suggested = m_catalogue->Suggest(m_folder, FolderExtensions(m_folder));
    if (suggested != nullptr)
      m_platformSlug = suggested->slug;
  }

  if (m_scraperId.empty())
  {
    ADDON::VECADDONS scrapers;
    if (CServiceBroker::GetAddonMgr().GetAddons(scrapers, ADDON::AddonType::SCRAPER_GAMES) &&
        !scrapers.empty())
      m_scraperId = scrapers.front()->ID();
  }
}

std::string CGUIDialogGameContentSettings::PlatformLabel(const std::string& slug) const
{
  if (slug.empty())
    return Localize(231); // None
  const PlatformInfo* platform = m_catalogue ? m_catalogue->GetPlatform(slug) : nullptr;
  return platform != nullptr ? platform->name : slug;
}

std::string CGUIDialogGameContentSettings::ScraperLabel(const std::string& addonId) const
{
  if (addonId.empty())
    return Localize(231);
  ADDON::AddonPtr addon;
  if (CServiceBroker::GetAddonMgr().GetAddon(addonId, addon, ADDON::AddonType::SCRAPER_GAMES,
                                             ADDON::OnlyEnabled::CHOICE_NO))
    return addon->Name();
  return addonId;
}

std::string CGUIDialogGameContentSettings::GameClientLabel(const std::string& addonId) const
{
  if (addonId.empty())
    return Localize(231);
  ADDON::AddonPtr addon;
  if (CServiceBroker::GetAddonMgr().GetAddon(addonId, addon, ADDON::AddonType::GAMEDLL,
                                             ADDON::OnlyEnabled::CHOICE_NO))
    return addon->Name();
  return addonId;
}

void CGUIDialogGameContentSettings::SetLabel2(const std::string& settingId, const std::string& label)
{
  BaseSettingControlPtr control = GetSettingControl(settingId);
  if (control && control->GetControl() != nullptr)
    SET_CONTROL_LABEL2(control->GetID(), label);
}

void CGUIDialogGameContentSettings::SetupView()
{
  CGUIDialogSettingsManualBase::SetupView();
  SetHeading(20333); // Set content

  SET_CONTROL_HIDDEN(CONTROL_SETTINGS_CUSTOM_BUTTON);
  SET_CONTROL_LABEL(CONTROL_SETTINGS_OKAY_BUTTON, 186);
  SET_CONTROL_LABEL(CONTROL_SETTINGS_CANCEL_BUTTON, 222);

  SetLabel2(SETTING_PLATFORM, PlatformLabel(m_platformSlug));
  SetLabel2(SETTING_SCRAPER, ScraperLabel(m_scraperId));
  SetLabel2(SETTING_GAME_CLIENT, GameClientLabel(m_gameClient));
  SetLabel2(SETTING_VIDEO_FILTER, m_videoFilter.empty() ? Localize(231) : m_videoFilter);
}

void CGUIDialogGameContentSettings::InitializeSettings()
{
  CGUIDialogSettingsManualBase::InitializeSettings();

  const std::shared_ptr<CSettingCategory> category = AddCategory("gamecontentsettings", -1);
  const std::shared_ptr<CSettingGroup> group = category ? AddGroup(category) : nullptr;
  if (!group)
  {
    CLog::Log(LOGERROR, "GAME: Unable to set up the content settings dialog");
    return;
  }

  AddButton(group, SETTING_PLATFORM, 35549, SettingLevel::Basic);
  AddButton(group, SETTING_SCRAPER, 38025, SettingLevel::Basic);

  const std::shared_ptr<CSettingGroup> scanGroup = AddGroup(category, 20322);
  if (scanGroup)
  {
    AddToggle(scanGroup, SETTING_SCAN_RECURSIVE, 20346, SettingLevel::Basic, m_scanRecursive);
    AddToggle(scanGroup, SETTING_USE_FOLDER_NAMES, 20330, SettingLevel::Basic, m_useFolderNames);
    AddToggle(scanGroup, SETTING_NO_UPDATE, 20432, SettingLevel::Basic, m_noUpdate);
    AddToggle(scanGroup, SETTING_EXCLUDE, 20380, SettingLevel::Basic, m_exclude);
  }

  const std::shared_ptr<CSettingGroup> playGroup = AddGroup(category, 35201);
  if (playGroup)
  {
    AddButton(playGroup, SETTING_GAME_CLIENT, 35510, SettingLevel::Basic);
    AddButton(playGroup, SETTING_VIDEO_FILTER, 35326, SettingLevel::Basic);
  }
}

void CGUIDialogGameContentSettings::OnSettingChanged(const std::shared_ptr<const CSetting>& setting)
{
  if (!setting)
    return;

  CGUIDialogSettingsManualBase::OnSettingChanged(setting);

  const std::string& id = setting->GetId();
  const bool value = std::static_pointer_cast<const CSettingBool>(setting)->GetValue();
  if (id == SETTING_SCAN_RECURSIVE)
    m_scanRecursive = value;
  else if (id == SETTING_USE_FOLDER_NAMES)
    m_useFolderNames = value;
  else if (id == SETTING_NO_UPDATE)
    m_noUpdate = value;
  else if (id == SETTING_EXCLUDE)
    m_exclude = value;
}

void CGUIDialogGameContentSettings::OnSettingAction(const std::shared_ptr<const CSetting>& setting)
{
  if (!setting)
    return;

  CGUIDialogSettingsManualBase::OnSettingAction(setting);

  const std::string& id = setting->GetId();
  if (id == SETTING_PLATFORM)
  {
    if (ChoosePlatform())
      SetLabel2(SETTING_PLATFORM, PlatformLabel(m_platformSlug));
  }
  else if (id == SETTING_SCRAPER)
  {
    if (ChooseScraper())
      SetLabel2(SETTING_SCRAPER, ScraperLabel(m_scraperId));
  }
  else if (id == SETTING_GAME_CLIENT)
  {
    if (ChooseGameClient())
      SetLabel2(SETTING_GAME_CLIENT, GameClientLabel(m_gameClient));
  }
  else if (id == SETTING_VIDEO_FILTER)
  {
    if (ChooseVideoFilter())
      SetLabel2(SETTING_VIDEO_FILTER, m_videoFilter.empty() ? Localize(231) : m_videoFilter);
  }
}

bool CGUIDialogGameContentSettings::ChoosePlatform()
{
  auto* select =
      CServiceBroker::GetGUI()->GetWindowManager().GetWindow<CGUIDialogSelect>(WINDOW_DIALOG_SELECT);
  if (select == nullptr || !m_catalogue)
    return false;

  select->Reset();
  select->SetHeading(CVariant{35549});
  select->SetUseDetails(true);

  CFileItemList items;
  const auto none = std::make_shared<CFileItem>(Localize(231));
  none->SetPath("");
  items.Add(none);

  std::vector<const PlatformInfo*> platforms;
  for (const PlatformInfo& platform : m_catalogue->GetPlatforms())
    platforms.emplace_back(&platform);
  std::ranges::sort(platforms, [](const PlatformInfo* a, const PlatformInfo* b)
                    { return a->sortName < b->sortName; });

  int selected = 0;
  for (const PlatformInfo* platform : platforms)
  {
    const auto item = std::make_shared<CFileItem>(platform->name);
    item->SetPath(platform->slug);
    item->SetLabel2(platform->manufacturer +
                    (platform->released > 0 ? " · " + std::to_string(platform->released) : ""));
    if (platform->slug == m_platformSlug)
      selected = items.Size();
    items.Add(item);
  }

  select->SetItems(items);
  select->SetSelected(selected);
  select->Open();

  if (!select->IsConfirmed() || select->GetSelectedItem() < 0)
    return false;

  m_platformSlug = select->GetSelectedFileItem()->GetPath();
  return true;
}

bool CGUIDialogGameContentSettings::ChooseScraper()
{
  auto* select =
      CServiceBroker::GetGUI()->GetWindowManager().GetWindow<CGUIDialogSelect>(WINDOW_DIALOG_SELECT);
  if (select == nullptr)
    return false;

  ADDON::VECADDONS scrapers;
  CServiceBroker::GetAddonMgr().GetAddons(scrapers, ADDON::AddonType::SCRAPER_GAMES);

  select->Reset();
  select->SetHeading(CVariant{38025});
  select->SetUseDetails(true);

  CFileItemList items;
  const auto none = std::make_shared<CFileItem>(Localize(231));
  none->SetPath("");
  items.Add(none);

  int selected = 0;
  for (const ADDON::AddonPtr& scraper : scrapers)
  {
    const auto item = std::make_shared<CFileItem>(scraper->Name());
    item->SetPath(scraper->ID());
    item->SetLabel2(scraper->Summary());
    item->SetArt("icon", scraper->Icon());
    if (scraper->ID() == m_scraperId)
      selected = items.Size();
    items.Add(item);
  }

  select->SetItems(items);
  select->SetSelected(selected);
  select->Open();

  if (!select->IsConfirmed() || select->GetSelectedItem() < 0)
    return false;

  m_scraperId = select->GetSelectedFileItem()->GetPath();
  return true;
}

bool CGUIDialogGameContentSettings::ChooseGameClient()
{
  auto* select =
      CServiceBroker::GetGUI()->GetWindowManager().GetWindow<CGUIDialogSelect>(WINDOW_DIALOG_SELECT);
  if (select == nullptr)
    return false;

  ADDON::VECADDONS addons;
  CServiceBroker::GetAddonMgr().GetAddons(addons, ADDON::AddonType::GAMEDLL);

  select->Reset();
  select->SetHeading(CVariant{35510});
  select->SetUseDetails(true);

  CFileItemList items;
  const auto none = std::make_shared<CFileItem>(Localize(231));
  none->SetPath("");
  items.Add(none);

  int selected = 0;
  for (const ADDON::AddonPtr& addon : addons)
  {
    const auto item = std::make_shared<CFileItem>(addon->Name());
    item->SetPath(addon->ID());
    item->SetLabel2(addon->Summary());
    item->SetArt("icon", addon->Icon());
    if (addon->ID() == m_gameClient)
      selected = items.Size();
    items.Add(item);
  }

  select->SetItems(items);
  select->SetSelected(selected);
  select->Open();

  if (!select->IsConfirmed() || select->GetSelectedItem() < 0)
    return false;

  m_gameClient = select->GetSelectedFileItem()->GetPath();
  return true;
}

bool CGUIDialogGameContentSettings::ChooseVideoFilter()
{
  auto* select =
      CServiceBroker::GetGUI()->GetWindowManager().GetWindow<CGUIDialogSelect>(WINDOW_DIALOG_SELECT);
  if (select == nullptr)
    return false;

  select->Reset();
  select->SetHeading(CVariant{35326});

  CFileItemList items;
  const auto none = std::make_shared<CFileItem>(Localize(231));
  none->SetPath("");
  items.Add(none);

  CFileItemList filters;
  GetVideoFilters(filters);
  int selected = 0;
  for (const auto& filter : filters)
  {
    const auto item = std::make_shared<CFileItem>(*filter);
    if (filter->GetPath() == m_videoFilter)
      selected = items.Size();
    items.Add(item);
  }

  select->SetItems(items);
  select->SetSelected(selected);
  select->Open();

  if (!select->IsConfirmed() || select->GetSelectedItem() < 0)
    return false;

  m_videoFilter = select->GetSelectedFileItem()->GetPath();
  return true;
}

bool CGUIDialogGameContentSettings::Save()
{
  CGameDatabase db;
  if (!db.Open())
    return false;

  GamePathContent content;
  content.scraper = m_scraperId;
  content.scanRecursive = m_scanRecursive;
  content.useFolderNames = m_useFolderNames;
  content.noUpdate = m_noUpdate;
  content.exclude = m_exclude;

  if (!m_platformSlug.empty() && m_catalogue)
  {
    if (const PlatformInfo* platform = m_catalogue->GetPlatform(m_platformSlug); platform != nullptr)
      content.idPlatform = db.AddPlatform(*platform);
  }

  if (!db.SetPathContent(m_folder, content))
    return false;

  // The emulator and the picture belong to the machine, so a second folder of
  // the same platform inherits what was chosen here. The folder keeps them too,
  // for a collection browsed as files rather than as a library.
  if (content.idPlatform > 0)
    db.SetPlatformDefaults(content.idPlatform, m_gameClient, m_videoFilter);

  db.GameClients().SetGameClient(m_folder, m_gameClient);
  db.VideoFilters().SetVideoFilter(m_folder, m_videoFilter);

  m_saved = true;
  return true;
}
