/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "GameLibraryTypes.h"

#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace KODI
{
namespace GAME
{
/*!
 * \ingroup games
 *
 * \brief The platforms Kodi knows about before any have been added to the
 *        library
 *
 * Loaded from the bundled platforms.xml, which gathers every name each
 * frontend and metadata provider uses for a platform, so that a folder
 * called "Sega Mega Drive", "genesis" or "Sega - Mega Drive - Genesis" can be
 * offered the right platform without the user typing anything.
 */
class CPlatformCatalogue
{
public:
  CPlatformCatalogue() = default;

  /*!
   * \brief Load the bundled catalogue
   *
   * \return True if at least one platform was read
   */
  bool Load();

  /*!
   * \brief Load a catalogue from a file, for testing or a user override
   */
  bool Load(const std::string& path);

  bool IsLoaded() const { return !m_platforms.empty(); }

  const std::vector<PlatformInfo>& GetPlatforms() const { return m_platforms; }

  /*!
   * \brief The platform with this slug, or nullptr
   */
  const PlatformInfo* GetPlatform(std::string_view slug) const;

  /*!
   * \brief The platform a name refers to, or nullptr
   *
   * Compares the name against every platform's name, slug and aliases, with
   * case, punctuation and whitespace ignored.
   */
  const PlatformInfo* FindByName(std::string_view name) const;

  /*!
   * \brief The platforms whose games use one of these file extensions
   *
   * \param extensions Lower case, without a leading dot
   *
   * \return Platforms ordered by how many of the extensions they claim
   */
  std::vector<const PlatformInfo*> FindByExtensions(
      const std::vector<std::string>& extensions) const;

  /*!
   * \brief The platform a folder most likely holds
   *
   * Its name is tried first, then its parent folders' names, then the
   * extensions of the files it holds.
   *
   * \return The platform, or nullptr when nothing fits well enough
   */
  const PlatformInfo* Suggest(const std::string& folderPath,
                              const std::vector<std::string>& extensions) const;

  /*!
   * \brief The key names are compared on
   */
  static std::string NameKey(std::string_view name);

private:
  std::vector<PlatformInfo> m_platforms;
  std::map<std::string, size_t> m_byKey; // NameKey -> index into m_platforms
  std::map<std::string, size_t> m_bySlug;
};
} // namespace GAME
} // namespace KODI
