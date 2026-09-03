/*
 *  Copyright (C) 2012-2020 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "GUIWindowGames.h"

#include "FileItem.h"
#include "FileItemList.h"
#include "GUIPassword.h"
#include "ServiceBroker.h"
#include "URL.h"
#include "Util.h"
#include "addons/gui/GUIDialogAddonInfo.h"
#include "application/Application.h"
#include "dialogs/GUIDialogContextMenu.h"
#include "dialogs/GUIDialogMediaSource.h"
#include "dialogs/GUIDialogProgress.h"
#include "filesystem/FileDirectoryFactory.h"
#include "filesystem/GameDatabaseDirectory.h"
#include "games/GameUtils.h"
#include "games/database/GameDatabase.h"
#include "games/dialogs/GUIDialogGameContentSettings.h"
#include "games/library/GameLibraryQueue.h"
#include "guilib/GUIComponent.h"
#include "guilib/GUIWindowManager.h"
#include "guilib/WindowIDs.h"
#include "input/actions/ActionIDs.h"
#include "media/MediaLockState.h"
#include "playlists/PlayListFileItemClassify.h"
#include "playlists/PlayListTypes.h"
#include "settings/MediaSourceSettings.h"
#include "settings/Settings.h"
#include "settings/SettingsComponent.h"
#include "utils/StringUtils.h"
#include "utils/URIUtils.h"

#include <algorithm>

using namespace KODI;
using namespace GAME;

#define CONTROL_BTNVIEWASICONS 2
#define CONTROL_BTNSORTBY 3
#define CONTROL_BTNSORTASC 4

CGUIWindowGames::CGUIWindowGames() : CGUIMediaWindow(WINDOW_GAMES, "MyGames.xml")
{
}

bool CGUIWindowGames::OnMessage(CGUIMessage& message)
{
  switch (message.GetMessage())
  {
    case GUI_MSG_WINDOW_DEINIT:
    {
      if (m_thumbLoader.IsLoading())
        m_thumbLoader.StopThread();
      break;
    }
    case GUI_MSG_WINDOW_INIT:
    {
      m_rootDir.AllowNonLocalSources(true); //! @todo

      // Is this the first time the window is opened?
      const std::string& destination = message.GetStringParam();
      if (StringUtils::EqualsNoCase(destination, "files"))
      {
        message.SetStringParam("sources://games/");
      }
      else if (StringUtils::EqualsNoCase(destination, "library"))
      {
        message.SetStringParam("gamedb://platforms/");
      }
      else if (m_vecItems->GetPath() == "?" && destination.empty())
      {
        // The library, once it has anything in it; the files otherwise
        CGameDatabase db;
        if (db.Open() && db.HasContent())
          message.SetStringParam("gamedb://platforms/");
        else
          message.SetStringParam(CMediaSourceSettings::GetInstance().GetDefaultSource("games"));
      }

      //! @todo
      m_dlgProgress = CServiceBroker::GetGUI()->GetWindowManager().GetWindow<CGUIDialogProgress>(
          WINDOW_DIALOG_PROGRESS);

      break;
    }
    case GUI_MSG_CLICKED:
    {
      if (OnClickMsg(message.GetSenderId(), message.GetParam1()))
        return true;
      break;
    }
    default:
      break;
  }
  return CGUIMediaWindow::OnMessage(message);
}

bool CGUIWindowGames::OnClickMsg(int controlId, int actionId)
{
  if (!m_viewControl.HasControl(controlId)) // list/thumb control
    return false;

  const int iItem = m_viewControl.GetSelectedItem();

  CFileItemPtr pItem = m_vecItems->Get(iItem);
  if (!pItem)
    return false;

  switch (actionId)
  {
    case ACTION_DELETE_ITEM:
    {
      // Is delete allowed?
      if (CServiceBroker::GetSettingsComponent()->GetSettings()->GetBool(
              CSettings::SETTING_FILELISTS_ALLOWFILEDELETION))
      {
        OnDeleteItem(iItem);
        return true;
      }
      break;
    }
    case ACTION_PLAYER_PLAY:
    {
      if (CanPlay(*pItem))
      {
        PlayGame(*pItem);
        return true;
      }
      break;
    }
    case ACTION_SHOW_INFO:
    {
      if (!m_vecItems->IsPlugin())
      {
        if (pItem->HasAddonInfo())
        {
          CGUIDialogAddonInfo::ShowForItem(pItem);
          return true;
        }
      }
      break;
    }
    default:
      break;
  }

  return false;
}

void CGUIWindowGames::SetupShares()
{
  CGUIMediaWindow::SetupShares();

  // Don't convert zip files to directories. Otherwise, the files will be
  // opened and scanned for games with a valid extension. If none are found,
  // the .zip won't be shown.
  //
  // This is a problem for MAME roms, because the files inside the .zip don't
  // have standard extensions.
  //
  m_rootDir.SetFlags(XFILE::DIR_FLAG_NO_FILE_DIRS);
}

bool CGUIWindowGames::OnClick(int iItem, const std::string& player /* = "" */)
{
  CFileItemPtr item = m_vecItems->Get(iItem);
  if (item)
  {
    if (!item->IsFolder())
    {
      PlayGame(*item);
      return true;
    }
  }

  return CGUIMediaWindow::OnClick(iItem, player);
}

void CGUIWindowGames::GetContextButtons(int itemNumber, CContextButtons& buttons)
{
  CFileItemPtr item = m_vecItems->Get(itemNumber);

  if (item && !item->GetProperty("pluginreplacecontextitems").asBoolean())
  {
    if (m_vecItems->IsVirtualDirectoryRoot() || m_vecItems->IsSourcesPath())
    {
      // Context buttons for a sources path, like "Add Source", "Remove Source", etc.
      CGUIDialogContextMenu::GetContextButtons("games", item, buttons);
    }
    else
    {
      if (CanPlay(*item))
      {
        buttons.Add(CONTEXT_BUTTON_PLAY_ITEM, 208); // Play
      }

      // Offered on folders as well as games: setting one on a folder is the
      // point, and a game only overrides the folder it sits in
      // A game may override the emulator and video filter its folder chose
      if (!item->IsFolder() && CanPlay(*item))
      {
        buttons.Add(CONTEXT_BUTTON_SET_DEFAULT_EMULATOR, 35510); // "Default emulator"
        buttons.Add(CONTEXT_BUTTON_SET_DEFAULT_VIDEO_FILTER, 35326); // "Default video filter"
      }

      // A library game can be identified and described again
      if (item->HasProperty("gameid") && !item->HasProperty("releaseid"))
        buttons.Add(CONTEXT_BUTTON_REFRESH_THUMBS, 184); // "Refresh"

      // A release of a library game can be made the one that plays
      if (item->HasProperty("releaseid") && !item->GetProperty("isdefaultrelease").asBoolean())
        buttons.Add(CONTEXT_BUTTON_SET_DEFAULT, 35551); // "Set as default release"

      // A folder of games is given its platform, scraper, emulator and filter in one place
      if (item->IsFolder() && !m_vecItems->IsPlugin() && !URIUtils::IsProtocol(item->GetPath(), "gamedb"))
      {
        buttons.Add(CONTEXT_BUTTON_SET_CONTENT, 20333); // "Set content"
        CGameDatabase db;
        if (db.Open() && db.GetPlatformIdForPath(item->GetPath()) > 0)
          buttons.Add(CONTEXT_BUTTON_SCAN, 35540); // "Scan to library"
      }

      if (CServiceBroker::GetSettingsComponent()->GetSettings()->GetBool(
              CSettings::SETTING_FILELISTS_ALLOWFILEDELETION) &&
          !item->IsReadOnly())
      {
        buttons.Add(CONTEXT_BUTTON_DELETE, 117);
        buttons.Add(CONTEXT_BUTTON_RENAME, 118);
      }
    }
  }

  CGUIMediaWindow::GetContextButtons(itemNumber, buttons);
}

bool CGUIWindowGames::OnContextButton(int itemNumber, CONTEXT_BUTTON button)
{
  CFileItemPtr item = m_vecItems->Get(itemNumber);
  if (item)
  {
    if (m_vecItems->IsVirtualDirectoryRoot() || m_vecItems->IsSourcesPath())
    {
      if (CGUIDialogContextMenu::OnContextButton("games", item, button))
      {
        Update(m_vecItems->GetPath());
        return true;
      }
    }
    switch (button)
    {
      case CONTEXT_BUTTON_PLAY_ITEM:
        PlayGame(*item);
        return true;
      case CONTEXT_BUTTON_SET_DEFAULT_EMULATOR:
        CGameUtils::ChooseAndSetDefaultGameClient(*item);
        return true;
      case CONTEXT_BUTTON_SET_DEFAULT_VIDEO_FILTER:
        CGameUtils::ChooseAndSetDefaultVideoFilter(*item);
        return true;
      case CONTEXT_BUTTON_SET_CONTENT:
      {
        bool scanNow = false;
        if (CGUIDialogGameContentSettings::Show(item->GetPath(), scanNow) && scanNow)
          CGameLibraryQueue::GetInstance().ScanLibrary(item->GetPath());
        return true;
      }
      case CONTEXT_BUTTON_SCAN:
        CGameLibraryQueue::GetInstance().ScanLibrary(item->GetPath());
        return true;
      case CONTEXT_BUTTON_REFRESH_THUMBS:
        CGameLibraryQueue::GetInstance().RefreshGame(
            static_cast<int>(item->GetProperty("gameid").asInteger()));
        return true;
      case CONTEXT_BUTTON_SET_DEFAULT:
      {
        CGameDatabase db;
        if (db.Open() &&
            db.SetDefaultRelease(static_cast<int>(item->GetProperty("gameid").asInteger()),
                                 static_cast<int>(item->GetProperty("releaseid").asInteger())))
          Refresh(true);
        return true;
      }
      case CONTEXT_BUTTON_INFO:
        CGUIDialogAddonInfo::ShowForItem(item);
        return true;
      case CONTEXT_BUTTON_DELETE:
        OnDeleteItem(itemNumber);
        return true;
      case CONTEXT_BUTTON_RENAME:
        OnRenameItem(itemNumber);
        return true;
      default:
        break;
    }
  }
  return CGUIMediaWindow::OnContextButton(itemNumber, button);
}

bool CGUIWindowGames::OnAddMediaSource()
{
  return CGUIDialogMediaSource::ShowAndAddMediaSource("games");
}

bool CGUIWindowGames::Update(const std::string& strDirectory, bool updateFilterPath /* = true */)
{
  if (m_thumbLoader.IsLoading())
    m_thumbLoader.StopThread();

  if (!CGUIMediaWindow::Update(strDirectory, updateFilterPath))
    return false;

  // Games carry no artwork of their own: nothing scrapes them and the add-ons
  // that run them describe the emulator rather than the game. What a collection
  // does have is images sitting beside the files, so a platform folder shows
  // the system it holds and a game shows its own cover.
  m_thumbLoader.Load(*m_vecItems);

  return true;
}

bool CGUIWindowGames::GetDirectory(const std::string& strDirectory, CFileItemList& items)
{
  if (!CGUIMediaWindow::GetDirectory(strDirectory, items))
    return false;

  // Now we must account for file folders not handled by DIR_FLAG_NO_FILE_DIRS
  for (int i = 0; i < items.Size(); ++i)
  {
    CFileItemPtr item = items[i];
    if (item->IsFolder() || !item->IsFileFolder(FileFolderType::ALWAYS))
      continue;

    const std::string originalPath = item->GetPath();

    // This will turn the item into a file folder as a side effect
    std::unique_ptr<XFILE::IFileDirectory> pDirectory{
        XFILE::CFileDirectoryFactory::Create(CURL{originalPath}, item.get())};
    if (pDirectory)
    {
      // Check for empty file folders
      CFileItemList fileFolderItems;
      if (!pDirectory->GetDirectory(item->GetURL(), fileFolderItems) || fileFolderItems.IsEmpty())
      {
        items.Remove(i);
        --i;
        continue;
      }

      // Check if file folder contains games or subfolders
      if (std::ranges::any_of(fileFolderItems,
                              [](const CFileItemPtr& fileFolderItem)
                              {
                                return fileFolderItem->IsFolder() ||
                                       CGameUtils::HasGameExtension(fileFolderItem->GetPath());
                              }))
      {
        continue;
      }

      // If the file folder contains no games, turn it back into a regular file
      item->SetFolder(false);
      item->SetPath(originalPath);
    }
    else
    {
      // File folder contains a single file and was collapsed down. Remove it
      // if not a game.
      if (!CGameUtils::HasGameExtension(item->GetPath()))
      {
        items.Remove(i);
        --i;
      }
    }
  }

  // Set label
  std::string label;
  if (items.GetLabel().empty())
  {
    if (URIUtils::IsProtocol(items.GetPath(), "gamedb"))
    {
      label = XFILE::CGameDatabaseDirectory::GetLabel(items.GetPath());
    }
    else
    {
      std::string source;
      if (m_rootDir.IsSource(items.GetPath(),
                             CMediaSourceSettings::GetInstance().GetSources("games"), &source))
        label = std::move(source);
    }
  }

  if (!label.empty())
    items.SetLabel(label);

  // Set content
  std::string content;
  if (items.GetContent().empty())
  {
    if (!items.IsVirtualDirectoryRoot() && // Don't set content for root directory
        !items.IsPlugin()) // Don't set content for plugins
    {
      content = "games";
    }
  }

  if (!content.empty())
    items.SetContent(content);

  // Ensure a game info tag is created so that files are recognized as games
  for (const CFileItemPtr& item : items)
  {
    if (!item->IsFolder())
      item->GetGameInfoTag();
  }

  return true;
}

std::string CGUIWindowGames::GetStartFolder(const std::string& dir)
{
  // From CGUIWindowPictures::GetStartFolder()

  if (StringUtils::EqualsNoCase(dir, "plugins") || StringUtils::EqualsNoCase(dir, "addons"))
  {
    return "addons://sources/game/";
  }

  SetupShares();
  std::vector<CMediaSource> shares;
  m_rootDir.GetSources(shares);
  bool bIsSourceName = false;
  int iIndex = CUtil::GetMatchingSource(dir, shares, bIsSourceName);
  if (iIndex >= 0)
  {
    if (iIndex < static_cast<int>(shares.size()) && shares[iIndex].GetLockInfo().IsLocked())
    {
      CFileItem item(shares[iIndex]);
      if (!g_passwordManager.IsItemUnlocked(&item, "games"))
        return "";
    }
    if (bIsSourceName)
      return shares[iIndex].strPath;
    return dir;
  }
  return CGUIMediaWindow::GetStartFolder(dir);
}

void CGUIWindowGames::OnItemInfo(int itemNumber)
{
  CFileItemPtr item = m_vecItems->Get(itemNumber);
  if (!item)
    return;

  if (!m_vecItems->IsPlugin())
  {
    if (item->IsPlugin() || item->IsScript())
      CGUIDialogAddonInfo::ShowForItem(item);
  }

  //! @todo
  /*
  CGUIDialogGameInfo* gameInfo =
  CServiceBroker::GetGUI()->GetWindowManager().GetWindow<CGUIDialogGameInfo>(WINDOW_DIALOG_PICTURE_INFO);
  if (gameInfo)
  {
    gameInfo->SetGame(item);
    gameInfo->Open();
  }
  */
}

bool CGUIWindowGames::PlayGame(const CFileItem& item)
{
  CFileItem itemCopy(item);

  // Dereference file folders. The check here assumes all "is folder" items
  // passed CanPlay(), e.g. are definitely file folders.
  if (itemCopy.IsFolder())
  {
    itemCopy.SetFolder(false);
    itemCopy.SetPath(itemCopy.GetURL().GetHostName());
    itemCopy.GetGameInfoTag();
  }

  PLAYLIST::Id playlistId = PLAYLIST::Id::TYPE_NONE;
  if (PLAYLIST::IsPlayList(item))
    playlistId = PLAYLIST::Id::TYPE_GAME;

  return g_application.PlayMedia(itemCopy, "", playlistId);
}

bool CGUIWindowGames::CanPlay(const CFileItem& item) const
{
  if (item.IsGame())
    return true;

  if (item.IsFolder())
  {
    // Check for file folders
    CURL url{item.GetPath()};
    if (url.GetFileName().empty() && URIUtils::IsZIP(url.GetHostName()))
      return true;
  }

  return false;
}
