/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "IDirectory.h"

#include <string>

namespace XFILE
{
/*!
 * \ingroup filesystem
 *
 * \brief The gamedb:// tree: the game library as folders
 *
 * See KODI::GAME::CGameDbUrl for the shape of the tree. Overview nodes list
 * the facets and ready-made lists below them; every other node asks the
 * database.
 */
class CGameDatabaseDirectory : public IDirectory
{
public:
  CGameDatabaseDirectory();
  ~CGameDatabaseDirectory() override;

  // Implementation of IDirectory
  bool GetDirectory(const CURL& url, CFileItemList& items) override;
  bool AllowAll() const override { return true; }
  bool Exists(const CURL& url) override;
  CacheType GetCacheType(const CURL& url) const override { return CacheType::ALWAYS; }

  /*!
   * \brief The label a gamedb:// path should be shown with, or empty
   */
  static std::string GetLabel(const std::string& path);
};
} // namespace XFILE
