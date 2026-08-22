/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "PathDefaults.h"

#include "utils/URIUtils.h"

using namespace KODI;
using namespace GAME;

namespace
{
/*!
 * \brief How many folders above a game are asked for a setting
 *
 * GetParentPath() stops handing back new paths at the root, so the walk ends
 * on its own for every path this has been given. The limit is there so that a
 * path form which never shrinks cannot spin forever.
 */
constexpr unsigned int MAX_FOLDER_DEPTH = 64;
} // namespace

std::string GAME::FindPathDefault(const std::string& path,
                                  const std::function<std::string(const std::string&)>& lookup)
{
  if (path.empty() || !lookup)
    return "";

  // The game's own setting wins, which is what makes a per-game choice an
  // override of the folder it sits in
  std::string value = lookup(path);
  if (!value.empty())
    return value;

  // Otherwise the nearest folder above it that has one
  std::string currentPath = path;
  for (unsigned int i = 0; i < MAX_FOLDER_DEPTH; ++i)
  {
    std::string parentPath;
    if (!URIUtils::GetParentPath(currentPath, parentPath) || parentPath.empty() ||
        parentPath == currentPath)
      break;

    value = lookup(parentPath);
    if (!value.empty())
      return value;

    currentPath = std::move(parentPath);
  }

  return "";
}
