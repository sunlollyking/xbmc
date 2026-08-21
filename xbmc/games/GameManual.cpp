/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "GameManual.h"

#include "FileItem.h"
#include "URL.h"
#include "filesystem/File.h"
#include "utils/URIUtils.h"

using namespace KODI::GAME;

namespace
{
constexpr const char* MANUAL_EXTENSION = ".pdf";
} // namespace

std::string CGameManual::BuildManualPath(const std::string& gamePath)
{
  if (gamePath.empty())
    return {};

  // A game inside an archive, or reached through a protocol that addresses
  // something other than a file, has no directory to sit a manual beside
  if (URIUtils::IsInArchive(gamePath) || URIUtils::IsInternetStream(gamePath))
    return {};

  // A game with no extension is left alone: appending would invent a basename
  // the player never chose
  if (URIUtils::GetExtension(gamePath).empty())
    return {};

  // ReplaceExtension() rather than RemoveExtension(): the latter only strips
  // extensions Kodi recognises, which it looks up from the installed add-ons.
  // A manual sitting beside a game must be found the same way whatever add-ons
  // happen to be installed, so the last extension is replaced literally.
  return URIUtils::ReplaceExtension(gamePath, MANUAL_EXTENSION);
}

std::string CGameManual::GetManualPath(const std::string& gamePath)
{
  const std::string manualPath = BuildManualPath(gamePath);
  if (manualPath.empty())
    return {};

  if (!XFILE::CFile::Exists(manualPath))
    return {};

  return manualPath;
}

std::string CGameManual::GetManualPath(const CFileItem& item)
{
  return GetManualPath(item.GetDynPath());
}
