/*
 *  Copyright (C) 2012-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "settings/lib/ISettingCallback.h"
#include "utils/Observer.h"

#include <string>

class CSetting;
class CSettings;

namespace KODI
{
namespace GAME
{

/*!
 * \ingroup games
 */
class CGameSettings : public ISettingCallback, public Observable
{
public:
  CGameSettings();
  ~CGameSettings() override;

  // General settings
  bool GamesEnabled();
  bool ShowOSDHelp();
  void SetShowOSDHelp(bool bShow);
  void ToggleGames();
  bool AutosaveEnabled();
  bool RewindEnabled();
  unsigned int MaxRewindTimeSec();
  std::string GetRAUsername() const;
  std::string GetRAToken() const;

  bool GetAchievementsLoggedIn() const;

  /*!
   * \brief Whether achievements are earned in hardcore mode
   *
   * Hardcore doubles the points awarded, and requires the player to go without
   * rewind, save state loading, cheats and slow motion. RetroAchievements does
   * not allow a session started in casual mode to continue into hardcore, so
   * turning this on mid-game resets it.
   */
  bool GetAchievementsHardcore() const;

  /*!
   * \brief Whether achievements already earned can be triggered again
   */
  bool GetAchievementsEncore() const;

  /*!
   * \brief Record whether the player is logged in to RetroAchievements
   *
   * Called when the game add-on reports the outcome of a login attempt, so
   * that a rejected token doesn't leave the UI claiming the player is logged
   * in. Saves the settings only when the state actually changes.
   *
   * \param loggedIn True if the player is logged in
   */
  void SetAchievementsLoggedIn(bool loggedIn);

  // Inherited from ISettingCallback
  void OnSettingChanged(const std::shared_ptr<const CSetting>& setting) override;

private:
  std::string LoginToRA(const std::string& username,
                        const std::string& password,
                        std::string token) const;
  bool IsAccountVerified(const std::string& username, const std::string& token) const;

  // Construction parameters
  std::shared_ptr<CSettings> m_settings;
};

} // namespace GAME
} // namespace KODI
