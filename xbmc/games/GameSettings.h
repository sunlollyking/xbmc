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
    "gamesachievements.onscreenindicators";

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

  bool GetAchievementsLoggedIn() const;

  bool GetAchievementsOnScreenIndicators() const;

  //! \brief Whether achievements already earned should be earnable again
  bool GetAchievementsEncore() const;

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
  void OnSettingChanged(const std::shared_ptr<const CSetting>& setting) override;
  void OnSettingAction(const std::shared_ptr<const CSetting>& setting) override;

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
