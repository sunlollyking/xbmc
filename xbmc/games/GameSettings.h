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
//! Read directly by the info provider, which is handed its dependencies rather
//! than reaching for the game services, and so cannot go through CGameSettings
constexpr const char* SETTING_ACHIEVEMENTS_CHALLENGE_INDICATOR =
    "gamesachievements.challengeindicator";

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

  /*!
   * \brief Whether to run the emulator ahead of itself to hide input latency
   *
   * Costs a whole extra run of the emulator per hidden frame, so it is only
   * usable on clients that already have the headroom to spare.
   */
  bool RunaheadEnabled();

  /*!
   * \brief How many frames ahead of itself to run the emulator
   *
   * Each frame removes about one frame's worth of input latency and adds
   * another full run of the emulator per displayed frame.
   */
  unsigned int RunaheadFrames();
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
   * \brief Turn hardcore mode on or off
   *
   * Used to drop back to casual when a session resumes from a save state,
   * which RetroAchievements requires.
   */
  void SetAchievementsHardcore(bool hardcore);

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

  /*!
   * \brief Whether to show the achievement being attempted over the game
   *
   * Some players want to know an attempt is live; others would rather nothing
   * covered the picture. It is on by default, because an indicator nobody asked
   * for is easier to turn off than one nobody knew existed.
   */
  bool GetChallengeIndicator() const;

  // Inherited from ISettingCallback
  void OnSettingChanged(const std::shared_ptr<const CSetting>& setting) override;
  void OnSettingAction(const std::shared_ptr<const CSetting>& setting) override;

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
