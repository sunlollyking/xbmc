/*
 *  Copyright (C) 2012-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "GameUtils.h"

#include "FileItem.h"
#include "FileItemList.h"
#include "ServiceBroker.h"
#include "URL.h"
#include "addons/Addon.h"
#include "addons/AddonInstaller.h"
#include "addons/AddonManager.h"
#include "addons/BinaryAddonCache.h"
#include "addons/addoninfo/AddonType.h"
#include "cores/RetroPlayer/guibridge/GUIGameRenderManager.h"
#include "cores/RetroPlayer/guibridge/GUIGameSettingsHandle.h"
#include "cores/RetroPlayer/savestates/ISavestate.h"
#include "cores/RetroPlayer/savestates/SavestateDatabase.h"
#include "dialogs/GUIDialogKaiToast.h"
#include "dialogs/GUIDialogOK.h"
#include "dialogs/GUIDialogSelect.h"
#include "filesystem/AddonsDirectory.h"
#include "filesystem/File.h"
#include "filesystem/FileDirectoryFactory.h"
#include "filesystem/IFileDirectory.h"
#include "filesystem/SpecialProtocol.h"
#include "games/VideoFilters.h"
#include "games/addons/GameClient.h"
#include "games/database/GameDatabase.h"
#include "games/dialogs/GUIDialogSelectGameClient.h"
#include "games/dialogs/GUIDialogSelectSavestate.h"
#include "games/library/GameLibraryTypes.h"
#include "games/tags/GameInfoTag.h"
#include "guilib/GUIComponent.h"
#include "guilib/GUIWindowManager.h"
#include "guilib/WindowIDs.h"
#include "messaging/helpers/DialogOKHelper.h"
#include "resources/LocalizeStrings.h"
#include "resources/ResourcesComponent.h"
#include "utils/StringUtils.h"
#include "utils/URIUtils.h"
#include "utils/Variant.h"
#include "utils/log.h"

#include <algorithm>
#include <vector>

using namespace KODI;
using namespace GAME;

namespace
{
/*!
 * \brief Whether a disk is the one a game starts from, going by its name
 *
 * Sets name the boot disk "Disk A", "Disk 1" or "System disk", or give it no
 * tag at all beside its "User disk" and "Data disk", which sort before it.
 */
bool IsStartDisk(const std::string& path)
{
  std::string name = URIUtils::GetFileName(path);
  URIUtils::RemoveExtension(name);
  StringUtils::ToLower(name);

  const size_t open = name.rfind('(');
  if (open == std::string::npos || name.back() != ')')
    return true;

  const std::string tag = name.substr(open + 1, name.size() - open - 2);
  return tag == "disk a" || tag == "disk 1" || StringUtils::StartsWith(tag, "disk a -") ||
         tag == "system disk" || tag == "program disk" || tag == "boot disk" || tag == "game disk";
}
} // namespace

// Initialize static state
ADDON::VECADDONS CGameUtils::m_installableGameAddons;
bool CGameUtils::m_checkInstallable{false};
std::mutex CGameUtils::m_installableMutex;

bool CGameUtils::FillInGameClient(CFileItem& item, std::string& savestatePath)
{
  using namespace ADDON;

  if (item.GetGameInfoTag()->GetGameClient().empty())
  {
    // If the fileitem is an add-on, fall back to that
    if (item.HasAddonInfo() && item.GetAddonInfo()->Type() == AddonType::GAMEDLL)
    {
      item.GetGameInfoTag()->SetGameClient(item.GetAddonInfo()->ID());
    }
    else
    {
      OpenInsideArchive(item);

      if (!CGUIDialogSelectSavestate::ShowAndGetSavestate(item.GetDynPath(), savestatePath))
        return false;

      if (!savestatePath.empty())
      {
        RETRO::CSavestateDatabase db;
        std::unique_ptr<RETRO::ISavestate> save = RETRO::CSavestateDatabase::AllocateSavestate();
        db.GetSavestate(savestatePath, *save);

        //! @todo Remove this when we can load compressed savestates
        if (save->IsCompressed())
        {
          // "Error"
          // "This savestate is compressed and can't be loaded by this version of Kodi."
          CGUIDialogOK::ShowAndGetInput(257, 35298);
          return false;
        }

        item.GetGameInfoTag()->SetGameClient(save->GameClientID());
      }
      else
      {
        // No game client specified, need to ask the user
        GameClientVector candidates;
        GameClientVector installable;
        bool bHasVfsGameClient;
        GetInstalledGameClients(item, candidates, bHasVfsGameClient);

        // An emulator remembered for this game, or for a folder above it,
        // answers the question without asking
        std::string defaultClient = GetDefaultGameClient(item.GetPath(), candidates);
        if (const std::string arcade =
                GetArcadeGameClient(item.GetPath(), candidates, defaultClient);
            !arcade.empty())
          defaultClient = arcade;
        if (!defaultClient.empty())
        {
          item.GetGameInfoTag()->SetGameClient(defaultClient);
        }
        else if (NeedsExtracting(item))
        {
          // "Failed to play game"
          // "This game can only be played directly from a hard drive or partition. Compressed files must be extracted."
          MESSAGING::HELPERS::ShowOKDialogText(CVariant{35210}, CVariant{35214});
        }
        else
        {
          GetInstallableGameClients(item, installable, bHasVfsGameClient);

          if (candidates.empty() && installable.empty())
          {
            // if: "This game can only be played directly from a hard drive or partition. Compressed files must be extracted."
            // else: "This game isn't compatible with any available emulators."
            int errorTextId = bHasVfsGameClient ? 35214 : 35212;

            // "Failed to play game"
            MESSAGING::HELPERS::ShowOKDialogText(CVariant{35210}, CVariant{errorTextId});
          }
          else if (candidates.size() == 1 && installable.empty())
          {
            // Only 1 option, avoid prompting the user
            item.GetGameInfoTag()->SetGameClient(candidates[0]->ID());
          }
          else
          {
            std::string gameClient = CGUIDialogSelectGameClient::ShowAndGetGameClient(
                item.GetDynPath(), candidates, installable);

            if (!gameClient.empty())
              item.GetGameInfoTag()->SetGameClient(gameClient);
          }
        }
      }
    }
  }

  const std::string gameClient = item.GetGameInfoTag()->GetGameClient();
  if (gameClient.empty())
    return false;

  if (Install(gameClient))
  {
    // If the addon is disabled we need to enable it
    if (!Enable(gameClient))
    {
      CLog::Log(LOGDEBUG, "Failed to enable game client {}", gameClient);
      item.GetGameInfoTag()->SetGameClient("");
    }
  }
  else
  {
    CLog::Log(LOGDEBUG, "Failed to install game client: {}", gameClient);
    item.GetGameInfoTag()->SetGameClient("");
  }

  SetRomset(item);

  return !item.GetGameInfoTag()->GetGameClient().empty();
}

std::string CGameUtils::GetArcadeGameClient(const std::string& path,
                                            const GameClientVector& candidates,
                                            const std::string& remembered)
{
  CGameDatabase db;
  if (path.empty() || !db.Open())
    return "";

  const std::vector<EmulatorRomset> romsets = db.GetRomsetsForFile(path);
  if (!db.GameClients().GetGameClient(path).empty())
    return "";

  if (std::ranges::any_of(romsets, [&remembered](const EmulatorRomset& r)
                          { return r.gameClient == remembered; }))
    return remembered;

  for (const EmulatorRomset& romset : romsets)
  {
    const bool installed = std::ranges::any_of(candidates, [&romset](const GameClientPtr& c)
                                               { return c->ID() == romset.gameClient; });
    if (installed)
    {
      CLog::Log(LOGINFO, "GAME: Opening {} with {}, which holds it exactly as {}",
                CURL::GetRedacted(path), romset.gameClient, romset.romset);
      return romset.gameClient;
    }
  }
  return "";
}

void CGameUtils::SetRomset(CFileItem& item)
{
  CGameDatabase db;
  if (item.GetPath().empty() || !db.Open())
    return;

  const std::string gameClient = item.GetGameInfoTag()->GetGameClient();
  for (const EmulatorRomset& romset : db.GetRomsetsForFile(item.GetPath()))
  {
    if (romset.gameClient != gameClient)
      continue;

    item.SetProperty("game.romset", romset.romset);

    // An emulator looks for a set's parent and BIOS beside it. Any that are
    // not already there are found and handed over with it.
    const std::string folder = URIUtils::GetDirectory(item.GetPath());
    std::vector<std::string> companions;
    for (const std::string& required : romset.required)
    {
      const std::string beside = URIUtils::AddFileToFolder(folder, required + ".zip");
      if (XFILE::CFile::Exists(beside))
        continue;

      std::string found = db.GetFileForRomset(required);
      if (found.empty())
      {
        const std::string bios =
            URIUtils::AddFileToFolder("special://profile/games/bios/system", required + ".zip");
        if (XFILE::CFile::Exists(bios))
          found = bios;
      }
      if (!found.empty())
        companions.emplace_back(required + "=" + found);
      else
        CLog::Log(LOGWARNING, "GAME: {} needs the set {}, which was not found",
                  CURL::GetRedacted(item.GetPath()), required);
    }
    item.SetProperty("game.romset.companions", StringUtils::Join(companions, ";"));
    return;
  }
}

std::string CGameUtils::GetRememberedGameClient(const std::string& path)
{
  if (path.empty())
    return "";

  CGameDatabase db;
  if (!db.Open())
    return "";

  std::string gameClient = db.GameClients().GetGameClientForGame(path);
  if (gameClient.empty())
  {
    // A library game plays with what its platform plays with: a collection is
    // arranged by machine, and the emulator belongs to the machine
    const int idPlatform = db.GetPlatformIdForGame(path);
    PlatformInfo platform;
    if (idPlatform > 0 && db.GetPlatform(idPlatform, platform))
      gameClient = platform.defaultGameClient;
  }

  return gameClient;
}

std::string CGameUtils::GetDefaultGameClient(const std::string& path,
                                             const GameClientVector& candidates)
{
  const std::string gameClient = GetRememberedGameClient(path);
  if (gameClient.empty())
    return "";

  // A remembered emulator is a preference, not an instruction. It is only used
  // if it can still open this game: one set on a folder has no idea what else
  // was put in that folder later, and one set before the emulator was
  // uninstalled would otherwise stop the game loading at all. Where it does not
  // fit, say so and let the user be asked, which is what would have happened
  // had nothing been remembered.
  const bool bCanOpen = std::any_of(candidates.begin(), candidates.end(),
                                    [&gameClient](const GameClientPtr& candidate)
                                    { return candidate->ID() == gameClient; });
  if (!bCanOpen)
  {
    CLog::Log(LOGDEBUG, "GAME: Ignoring remembered emulator {} for {}: it can't open this game",
              gameClient, CURL::GetRedacted(path));
    return "";
  }

  CLog::Log(LOGDEBUG, "GAME: Opening {} with remembered emulator {}", CURL::GetRedacted(path),
            gameClient);

  return gameClient;
}

bool CGameUtils::ChooseAndSetDefaultGameClient(const CFileItem& item)
{
  using namespace ADDON;

  const std::string path = item.GetPath();
  if (path.empty())
    return false;

  // A platform in the library is not a folder on a disk, so what is chosen for
  // it is stored against the machine
  const int idPlatform = item.HasProperty("platformid")
                             ? static_cast<int>(item.GetProperty("platformid").asInteger())
                             : -1;

  // A folder can be given anything later, so it offers every emulator that is
  // installed. A game only offers the ones that can open it.
  GameClientVector emulators;
  if (item.IsFolder())
  {
    VECADDONS addons;
    CServiceBroker::GetBinaryAddonCache().GetAddons(addons, AddonType::GAMEDLL);
    for (const auto& addon : addons)
      emulators.emplace_back(std::static_pointer_cast<CGameClient>(addon));
  }
  else
  {
    bool bHasVfsGameClient = false;
    GetInstalledGameClients(item, emulators, bHasVfsGameClient);
  }

  CGUIDialogSelect* dialog =
      CServiceBroker::GetGUI()->GetWindowManager().GetWindow<CGUIDialogSelect>(
          WINDOW_DIALOG_SELECT);
  if (dialog == nullptr)
    return false;

  CGameDatabase db;
  if (!db.Open())
    return false;

  PlatformInfo platform;
  const bool forPlatform = idPlatform > 0 && db.GetPlatform(idPlatform, platform);
  const std::string currentGameClient =
      forPlatform ? platform.defaultGameClient : db.GameClients().GetGameClient(path);

  dialog->Reset();
  dialog->SetHeading(CVariant{35510}); // "Default emulator"
  dialog->SetUseDetails(true);

  CFileItemList items;

  // First, so that clearing is as easy to reach as setting
  {
    CFileItemPtr noneItem = std::make_shared<CFileItem>(
        CServiceBroker::GetResourcesComponent().GetLocalizeStrings().Get(231)); // "None"
    noneItem->SetPath("");
    items.Add(std::move(noneItem));
  }

  for (const auto& emulator : emulators)
  {
    CFileItemPtr emulatorItem(XFILE::CAddonsDirectory::FileItemFromAddon(emulator, emulator->ID()));
    if (emulator->ID() == currentGameClient)
    {
      emulatorItem->SetLabel2(
          CServiceBroker::GetResourcesComponent().GetLocalizeStrings().Get(35511)); // "Current"
      emulatorItem->Select(true);
    }
    items.Add(std::move(emulatorItem));
  }

  dialog->SetItems(items);
  dialog->Open();

  if (!dialog->IsConfirmed())
    return false;

  const int selectedIndex = dialog->GetSelectedItem();
  if (selectedIndex < 0 || selectedIndex >= items.Size())
    return false;

  // An empty path is the "None" entry, which forgets rather than stores
  const std::string gameClient = items[selectedIndex]->GetPath();

  const bool stored = forPlatform
                          ? db.SetPlatformDefaults(idPlatform, gameClient, platform.defaultVideoFilter)
                          : db.GameClients().SetGameClient(path, gameClient);
  if (!stored)
    return false;

  if (gameClient.empty())
    CLog::Log(LOGDEBUG, "GAME: Forgot the emulator for {}", CURL::GetRedacted(path));
  else
    CLog::Log(LOGDEBUG, "GAME: Remembered emulator {} for {}", gameClient, CURL::GetRedacted(path));

  return true;
}

bool CGameUtils::ChooseAndSetDefaultVideoFilter(const CFileItem& item)
{
  const std::string path = item.GetPath();
  if (path.empty())
    return false;

  CGUIDialogSelect* dialog =
      CServiceBroker::GetGUI()->GetWindowManager().GetWindow<CGUIDialogSelect>(
          WINDOW_DIALOG_SELECT);
  if (dialog == nullptr)
    return false;

  CGameDatabase db;
  if (!db.Open())
    return false;

  PlatformInfo platform;
  const int idPlatform = item.HasProperty("platformid")
                             ? static_cast<int>(item.GetProperty("platformid").asInteger())
                             : -1;
  const bool forPlatform = idPlatform > 0 && db.GetPlatform(idPlatform, platform);
  const std::string currentVideoFilter =
      forPlatform ? platform.defaultVideoFilter : db.VideoFilters().GetVideoFilter(path);

  dialog->Reset();
  dialog->SetHeading(CVariant{35726}); // "Default video filter"
  dialog->SetUseDetails(true);

  CFileItemList items;

  // First, so that clearing is as easy to reach as setting
  {
    CFileItemPtr noneItem = std::make_shared<CFileItem>(
        CServiceBroker::GetResourcesComponent().GetLocalizeStrings().Get(231)); // "None"
    noneItem->SetProperty("game.videofilter", CVariant{""});
    items.Add(std::move(noneItem));
  }

  // No game is running, so nothing can say which scaling methods it supports
  GetVideoFilters(items);

  for (int i = 0; i < items.Size(); ++i)
  {
    if (items[i]->GetProperty("game.videofilter").asString() == currentVideoFilter &&
        !currentVideoFilter.empty())
    {
      items[i]->SetLabel2(
          CServiceBroker::GetResourcesComponent().GetLocalizeStrings().Get(35511)); // "Current"
      items[i]->Select(true);
    }
  }

  dialog->SetItems(items);
  dialog->Open();

  if (!dialog->IsConfirmed())
    return false;

  const int selectedIndex = dialog->GetSelectedItem();
  if (selectedIndex < 0 || selectedIndex >= items.Size())
    return false;

  // An empty filter is the "None" entry, which forgets rather than stores
  const std::string videoFilter = items[selectedIndex]->GetProperty("game.videofilter").asString();

  const bool stored =
      forPlatform ? db.SetPlatformDefaults(idPlatform, platform.defaultGameClient, videoFilter)
                  : db.VideoFilters().SetVideoFilter(path, videoFilter);
  if (!stored)
    return false;

  if (videoFilter.empty())
    CLog::Log(LOGDEBUG, "GAME: Forgot the video filter for {}", CURL::GetRedacted(path));
  else
    CLog::Log(LOGDEBUG, "GAME: Remembered video filter {} for {}", videoFilter,
              CURL::GetRedacted(path));

  return true;
}

void CGameUtils::OpenInsideArchive(CFileItem& item)
{
  const std::string archivePath = item.GetDynPath();
  CFileItem contents(archivePath, false);
  if (URIUtils::IsInArchive(archivePath) || !contents.IsFileFolder(FileFolderType::ALWAYS))
    return;

  // Arcade sets are archives by design, and an emulator that asks for the
  // archive is given it whole
  const std::string gameClient = GetRememberedGameClient(item.GetPath());
  if (!gameClient.empty())
  {
    GameClientVector candidates;
    bool bHasVfsGameClient;
    GetInstalledGameClients(item, candidates, bHasVfsGameClient);
    if (std::ranges::any_of(candidates, [&gameClient](const GameClientPtr& candidate)
                            { return candidate->ID() == gameClient; }))
      return;
  }

  // An archive holding a single game is that game, as it is when browsing
  std::string gamePath;
  const std::unique_ptr<XFILE::IFileDirectory> directory{
      XFILE::CFileDirectoryFactory::Create(CURL{archivePath}, &contents)};
  if (directory)
  {
    CFileItemList files;
    if (!directory->GetDirectory(contents.GetURL(), files))
      return;

    std::vector<std::string> games;
    for (const auto& file : files)
    {
      if (!file->IsFolder() && HasGameExtension(file->GetPath()))
        games.push_back(file->GetPath());
    }

    if (games.size() > 1)
    {
      // Several disks of one game, for the emulator the platform remembers:
      // start at the first, and the rest are found beside it
      const std::string extension = URIUtils::GetExtension(games.front());
      if (gameClient.empty() ||
          !std::ranges::all_of(games, [&extension](const std::string& game)
                               { return URIUtils::GetExtension(game) == extension; }))
        return;
      std::ranges::stable_sort(games,
                               [](const std::string& lhs, const std::string& rhs)
                               {
                                 const bool lhsStarts = IsStartDisk(lhs);
                                 const bool rhsStarts = IsStartDisk(rhs);
                                 return lhsStarts != rhsStarts ? lhsStarts : lhs < rhs;
                               });
    }

    if (!games.empty())
      gamePath = games.front();
  }
  else if (!contents.IsFolder() && contents.GetPath() != archivePath &&
           HasGameExtension(contents.GetPath()))
  {
    gamePath = contents.GetPath();
  }

  if (gamePath.empty())
    return;

  CLog::Log(LOGDEBUG, "GAME: Opening {} from inside {}", CURL::GetRedacted(gamePath),
            CURL::GetRedacted(archivePath));
  item.SetDynPath(gamePath);
}

bool CGameUtils::NeedsExtracting(const CFileItem& item)
{
  const std::string gameClientId = GetRememberedGameClient(item.GetPath());
  if (gameClientId.empty())
    return false;

  ADDON::AddonPtr addon;
  if (!CServiceBroker::GetAddonMgr().GetAddon(gameClientId, addon, ADDON::AddonType::GAMEDLL,
                                              ADDON::OnlyEnabled::CHOICE_NO))
    return false;

  const auto gameClient = std::static_pointer_cast<CGameClient>(addon);
  const CURL translatedUrl(CSpecialProtocol::TranslatePath(item.GetDynPath()));
  const bool bIsLocalFile =
      (translatedUrl.GetProtocol() == "file" || translatedUrl.GetProtocol().empty());

  return !bIsLocalFile && !URIUtils::IsInArchive(translatedUrl.Get()) &&
         !gameClient->SupportsVFS() &&
         gameClient->IsExtensionValid(URIUtils::GetExtension(translatedUrl.Get()));
}

void CGameUtils::GetInstalledGameClients(const CFileItem& file,
                                         GameClientVector& candidates,
                                         bool& bHasVfsGameClient)
{
  using namespace ADDON;

  bHasVfsGameClient = false;

  // Try to resolve path to a local file, as not all game clients support VFS
  CURL translatedUrl(CSpecialProtocol::TranslatePath(file.GetDynPath()));

  VECADDONS localAddons;
  CBinaryAddonCache& addonCache = CServiceBroker::GetBinaryAddonCache();
  addonCache.GetAddons(localAddons, AddonType::GAMEDLL);

  GetGameClients(localAddons, translatedUrl, candidates, bHasVfsGameClient);

  // Sort by name
  //! @todo Move to presentation code
  std::sort(candidates.begin(), candidates.end(),
            [](const GameClientPtr& lhs, const GameClientPtr& rhs)
            {
              std::string lhsName = lhs->Name();
              std::string rhsName = rhs->Name();

              StringUtils::ToLower(lhsName);
              StringUtils::ToLower(rhsName);

              return lhsName < rhsName;
            });
}

void CGameUtils::GetInstallableGameClients(const CFileItem& file,
                                           GameClientVector& installable,
                                           bool& bHasVfsGameClient)
{
  using namespace ADDON;

  // Try to resolve path to a local file, as not all game clients support VFS
  CURL translatedUrl(CSpecialProtocol::TranslatePath(file.GetDynPath()));

  VECADDONS remoteAddons;
  if (!CServiceBroker::GetAddonMgr().GetInstallableAddons(remoteAddons, AddonType::GAMEDLL))
    return;

  bool bVfs = false;
  GetGameClients(remoteAddons, translatedUrl, installable, bVfs);
  bHasVfsGameClient |= bVfs;

  // Sort by name
  //! @todo Move to presentation code
  std::sort(installable.begin(), installable.end(),
            [](const GameClientPtr& lhs, const GameClientPtr& rhs)
            {
              std::string lhsName = lhs->Name();
              std::string rhsName = rhs->Name();

              StringUtils::ToLower(lhsName);
              StringUtils::ToLower(rhsName);

              return lhsName < rhsName;
            });
}

void CGameUtils::GetGameClients(const ADDON::VECADDONS& addons,
                                const CURL& translatedUrl,
                                GameClientVector& candidates,
                                bool& bHasVfsGameClient)
{
  bHasVfsGameClient = false;

  const std::string extension = URIUtils::GetExtension(translatedUrl.Get());

  // A game inside an archive is copied out for a client that reads only local
  // files, so it can be offered one
  const bool bIsLocalFile = translatedUrl.GetProtocol() == "file" ||
                            translatedUrl.GetProtocol().empty() ||
                            URIUtils::IsInArchive(translatedUrl.Get());

  // An emulator that boots a folder can take an archive unpacked into one
  const bool bIsArchive =
      bIsLocalFile && CFileItem(translatedUrl.Get(), false).IsFileFolder(FileFolderType::ALWAYS);

  for (auto& addon : addons)
  {
    GameClientPtr gameClient = std::static_pointer_cast<CGameClient>(addon);

    // Filter by extension
    if (!gameClient->IsExtensionValid(extension) && !(bIsArchive && gameClient->SupportsFolders()))
      continue;

    // Filter by VFS
    if (!bIsLocalFile && !gameClient->SupportsVFS())
    {
      bHasVfsGameClient = true;
      continue;
    }

    candidates.push_back(gameClient);
  }
}

bool CGameUtils::HasGameExtension(const std::string& path)
{
  using namespace ADDON;

  // Get filename from CURL so that top-level zip directories will become
  // normal paths:
  //
  //   zip://%2Fpath_to_zip_file.zip/  ->  /path_to_zip_file.zip
  //
  std::string filename = CURL(path).GetFileNameWithoutPath();

  // Get the file extension
  std::string extension = URIUtils::GetExtension(filename);
  if (extension.empty())
    return false;

  StringUtils::ToLower(extension);

  // Look for a game client that supports this extension
  VECADDONS gameClients;
  CBinaryAddonCache& addonCache = CServiceBroker::GetBinaryAddonCache();
  addonCache.GetInstalledAddons(gameClients, AddonType::GAMEDLL);
  for (auto& gameClient : gameClients)
  {
    GameClientPtr gc(std::static_pointer_cast<CGameClient>(gameClient));
    if (gc->IsExtensionValid(extension))
      return true;
  }

  // Check remote add-ons
  std::lock_guard<std::mutex> installableLock(m_installableMutex);
  LoadInstallableAddons();
  if (!m_installableGameAddons.empty())
  {
    for (auto& gameClient : m_installableGameAddons)
    {
      GameClientPtr gc(std::static_pointer_cast<CGameClient>(gameClient));
      if (gc->IsExtensionValid(extension))
        return true;
    }
  }

  return false;
}

std::set<std::string> CGameUtils::GetGameExtensions()
{
  using namespace ADDON;

  std::set<std::string> extensions;

  VECADDONS gameClients;
  CBinaryAddonCache& addonCache = CServiceBroker::GetBinaryAddonCache();
  addonCache.GetAddons(gameClients, AddonType::GAMEDLL);
  for (auto& gameClient : gameClients)
  {
    GameClientPtr gc(std::static_pointer_cast<CGameClient>(gameClient));
    extensions.insert(gc->GetExtensions().begin(), gc->GetExtensions().end());
  }

  // Check remote add-ons
  std::lock_guard<std::mutex> installableLock(m_installableMutex);
  LoadInstallableAddons();
  if (!m_installableGameAddons.empty())
  {
    for (auto& gameClient : m_installableGameAddons)
    {
      GameClientPtr gc(std::static_pointer_cast<CGameClient>(gameClient));
      extensions.insert(gc->GetExtensions().begin(), gc->GetExtensions().end());
    }
  }

  // Remove special libretro extensions
  extensions.erase("*");
  extensions.erase(".");
  extensions.erase("./");
  extensions.erase("/");

  return extensions;
}

bool CGameUtils::IsStandaloneGame(const ADDON::AddonPtr& addon)
{
  using namespace ADDON;

  switch (addon->Type())
  {
    case AddonType::GAMEDLL:
    {
      return std::static_pointer_cast<GAME::CGameClient>(addon)->SupportsStandalone();
    }
    case AddonType::SCRIPT:
    {
      return addon->HasType(AddonType::GAME);
    }
    default:
      break;
  }

  return false;
}

void CGameUtils::UpdateInstallableAddons()
{
  std::lock_guard<std::mutex> installableLock(m_installableMutex);
  m_checkInstallable = true;
}

bool CGameUtils::Install(const std::string& gameClient)
{
  // If the addon isn't installed we need to install it
  bool installed = CServiceBroker::GetAddonMgr().IsAddonInstalled(gameClient);
  if (!installed)
  {
    ADDON::AddonPtr installedAddon;
    installed = ADDON::CAddonInstaller::GetInstance().InstallModal(
        gameClient, installedAddon, ADDON::InstallModalPrompt::CHOICE_NO);
    if (!installed)
    {
      CLog::Log(LOGERROR, "Game utils: Failed to install {}", gameClient);
      // "Error"
      // "Failed to install add-on."
      MESSAGING::HELPERS::ShowOKDialogText(CVariant{257}, CVariant{35256});
    }
  }

  return installed;
}

bool CGameUtils::Enable(const std::string& gameClient)
{
  bool bSuccess = true;

  if (CServiceBroker::GetAddonMgr().IsAddonDisabled(gameClient))
    bSuccess = CServiceBroker::GetAddonMgr().EnableAddon(gameClient);

  return bSuccess;
}

void CGameUtils::LoadInstallableAddons()
{
  using namespace ADDON;

  if (m_checkInstallable)
  {
    m_checkInstallable = false;
    m_installableGameAddons.clear();
    CServiceBroker::GetAddonMgr().GetInstallableAddons(m_installableGameAddons, AddonType::GAMEDLL);
  }
}

GameClientPtr CGameUtils::GetPlayingGameClient()
{
  auto gameSettingsHandle = CServiceBroker::GetGameRenderManager().RegisterGameSettingsDialog();
  if (!gameSettingsHandle)
    return {};

  // A handle is given out whether or not a game is playing, and says so with an
  // empty id rather than by being null
  const std::string gameClientId = gameSettingsHandle->GameClientID();
  if (gameClientId.empty())
    return {};

  ADDON::AddonPtr addon;
  if (!CServiceBroker::GetAddonMgr().GetAddon(gameClientId, addon, ADDON::AddonType::GAMEDLL,
                                              ADDON::OnlyEnabled::CHOICE_YES))
    return {};

  return std::static_pointer_cast<CGameClient>(addon);
}

void CGameUtils::NotifyBlockedByHardcore(uint32_t featureStringId)
{
  constexpr unsigned int TOAST_DISPLAY_TIME_MS = 5000;

  const auto& strings = CServiceBroker::GetResourcesComponent().GetLocalizeStrings();

  // "Hardcore mode", "{0:s} is not available". The mode heads the toast so the
  // longest feature name still fits the notification's fixed width.
  CGUIDialogKaiToast::QueueNotification(
      CGUIDialogKaiToast::Info, strings.Get(35700),
      StringUtils::Format(strings.Get(35305), strings.Get(featureStringId)), TOAST_DISPLAY_TIME_MS);
}
