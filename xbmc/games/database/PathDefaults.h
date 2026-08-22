/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include <functional>
#include <string>

namespace KODI
{
namespace GAME
{

/*!
 * \ingroup games
 *
 * \brief Find the setting that applies to a game
 *
 * Several things are remembered against a path rather than against a game:
 * which emulator opens it, which video filter it is drawn with. They all
 * resolve the same way, so they all ask this.
 *
 * The game's own setting wins, which is what makes a per-game choice an
 * override of the folder it sits in. Failing that, the nearest folder above it
 * that has one is used, then the folder above that.
 *
 * \param path The game
 * \param lookup Answers with the setting stored for exactly one path, or an
 *        empty string when that path has none
 *
 * \return The setting that applies, or empty if neither the game nor any
 *         folder above it has one
 */
std::string FindPathDefault(const std::string& path,
                            const std::function<std::string(const std::string&)>& lookup);

} // namespace GAME
} // namespace KODI
