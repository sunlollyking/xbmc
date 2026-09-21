/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "games/database/GameDatabase.h"
#include "games/library/GameLibraryTypes.h"
#include "settings/dialogs/GUIDialogSettingsManualBase.h"

#include <memory>
#include <string>
#include <vector>

namespace KODI
{
namespace GAME
{
class CPlatformCatalogue;

/*!
 * \ingroup games
 *
 * \brief The "Set content" dialog for a folder of games
 *
 * The games counterpart of the video content dialog: which platform the
 * folder holds, which scraper describes it, how it is scanned, and the two
 * choices the folder's games inherit, the emulator that opens them and the
 * video filter they are drawn with. Everything is stored against the folder,
 * and every folder below inherits it until it says otherwise.
 */
class CGUIDialogGameContentSettings : public CGUIDialogSettingsManualBase
{
public:
  CGUIDialogGameContentSettings();
  ~CGUIDialogGameContentSettings() override;

  bool HasListItems() const override { return true; }

  /*!
   * \brief Show the dialog for a folder and store what was chosen
   *
   * \param folder The folder, which becomes a library root if it was not one
   * \param[out] scanNow Whether the user asked for the folder to be scanned
   *
   * \return True if the settings were saved
   */
  static bool Show(const std::string& folder, bool& scanNow);

protected:
  // Implementation of CGUIDialogSettingsManualBase
  void OnSettingChanged(const std::shared_ptr<const CSetting>& setting) override;
  void OnSettingAction(const std::shared_ptr<const CSetting>& setting) override;
  bool AllowResettingSettings() const override { return false; }
  bool Save() override;
  void SetupView() override;
  void InitializeSettings() override;

private:
  void SetFolder(const std::string& folder);
  void SetLabel2(const std::string& settingId, const std::string& label);
  std::string PlatformLabel(const std::string& slug) const;
  std::string ScraperLabel(const std::string& addonId) const;
  std::string GameClientLabel(const std::string& addonId) const;
  bool ChoosePlatform();
  bool ChooseScraper();
  bool ChooseGameClient();
  bool ChooseVideoFilter();

  std::string m_folder;
  std::unique_ptr<CPlatformCatalogue> m_catalogue;
  std::string m_platformSlug;
  std::string m_scraperId;
  std::string m_gameClient;
  std::string m_videoFilter;
  bool m_scanRecursive{true};
  bool m_useFolderNames{false};
  bool m_exclude{false};
  bool m_noUpdate{false};
  bool m_saved{false};
};
} // namespace GAME
} // namespace KODI
