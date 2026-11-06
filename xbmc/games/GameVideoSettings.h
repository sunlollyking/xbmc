/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include <optional>
#include <string>

class CGameSettings;

namespace KODI
{
namespace GAME
{
struct PlatformInfo;

/*!
 * \ingroup games
 * \brief How a game is drawn, as remembered for a game, a folder or a platform
 *
 * A setting left unset is decided further up: a game by its folder, a folder by
 * the folders above it, then the platform, then Kodi's defaults.
 */
struct GameVideoSettings
{
  std::optional<std::string> videoFilter; // Empty means drawn without a filter
  std::optional<std::string> stretchMode; // RetroPlayer's identifier, e.g. "4:3"
  std::optional<unsigned int> rotationDegCCW;
  std::optional<bool> bezelEnabled;

  bool IsEmpty() const;

  /*!
   * \brief Take whatever this leaves unset from a fallback
   */
  void Inherit(const GameVideoSettings& fallback);

  /*!
   * \brief The settings that differ from a baseline, the rest left unset
   */
  GameVideoSettings Difference(const GameVideoSettings& baseline) const;

  /*!
   * \brief Set what this sets on a game's settings
   */
  void ApplyTo(::CGameSettings& settings) const;

  /*!
   * \brief Everything a game's settings say, all of it set
   */
  static GameVideoSettings FromGameSettings(const ::CGameSettings& settings);

  /*!
   * \brief A platform's defaults, unset where the platform has none
   */
  static GameVideoSettings FromPlatform(const PlatformInfo& platform);

  bool operator==(const GameVideoSettings& rhs) const = default;
};
} // namespace GAME
} // namespace KODI
