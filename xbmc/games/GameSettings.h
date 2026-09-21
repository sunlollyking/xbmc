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

// Shared with the GUI info provider, which reads it without game services
constexpr auto SETTING_GAMES_ACHIEVEMENTS_ONSCREEN_INDICATORS =
    "gamesachievements.challengeindicator";

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

  /*!
   * \brief The key that identifies the player to the web API
   *
   * The token above signs requests an emulator makes while playing; this
   * one is for asking the service about a game nobody is playing.
   */
  std::string GetRAApiKey() const;

  /*!
   * \brief The player's RetroAchievements avatar, or empty if not signed in
   *
   * The icon for notifications that speak for RetroAchievements, as the
   * sign-in notification already does. Empty leaves the notification with
   * Kodi's own icon, which is what a signed-out player should see.
   */
  std::string GetRAUserPicUrl() const;

  /*!
   * \brief Whether achievements are earned in hardcore mode
   *
   * Hardcore awards double points, and in exchange the player goes without
   * rewind, save state loading, cheats and slow motion. RetroAchievements does
   * not allow a session begun in casual mode to carry on into hardcore, so
   * turning this on part-way through restarts the game.
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
   * \brief Whether to show the achievement being attempted over the game
   *
   * Some players want to know an attempt is live; others would rather nothing
   * covered the picture. It is on by default, because an indicator nobody asked
   * for is easier to turn off than one nobody knew existed.
   */
  bool GetChallengeIndicator() const;

  bool GetAchievementsLoggedIn() const;

  bool GetAchievementsOnScreenIndicators() const;

  /*!
   * \brief Record whether the player is logged in to RetroAchievements
   *
   * Reported by the add-on, so a rejected token doesn't leave the UI claiming
   * the player is logged in.
   *
   * \param loggedIn True if the player is logged in
   */
  void SetAchievementsLoggedIn(bool loggedIn);

  // Inherited from ISettingCallback
  void OnSettingAction(const std::shared_ptr<const CSetting>& setting) override;
  void OnSettingChanged(const std::shared_ptr<const CSetting>& setting) override;

private:
  std::string LoginToRA(const std::string& username,
                        const std::string& password,
                        std::string token) const;
  bool IsAccountVerified(const std::string& username, const std::string& token) const;

  /*!
   * \brief Tell the game scrapers who the person is
   *
   * The scrapers read achievements with the same account, so a person says it
   * once here rather than again in every scraper's own settings. A field left
   * empty here leaves the scraper's own alone, so anyone driving a scraper
   * directly still can.
   */
  void ShareAchievementCredentials() const;

  // Construction parameters
  std::shared_ptr<CSettings> m_settings;
};

} // namespace GAME
} // namespace KODI
