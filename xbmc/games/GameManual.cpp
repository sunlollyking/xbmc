/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "GameManual.h"

#include "FileItem.h"
#include "FileItemList.h"
#include "URL.h"
#include "filesystem/Directory.h"
#include "filesystem/File.h"
#include "utils/StringUtils.h"
#include "utils/URIUtils.h"

#include <algorithm>
#include <array>
#include <cctype>

using namespace KODI::GAME;

namespace
{
//! Preferred first: a PDF is a single file with real page structure, where a
//! comic archive is a bag of images that has to be ordered by filename
constexpr std::array<const char*, 3> MANUAL_EXTENSIONS = {".pdf", ".cbz", ".cbr"};

//! Where a manual is kept if it is not beside the game
constexpr const char* MANUAL_SUBFOLDER = "manuals";

/*!
 * \brief Whether a path could have a manual sitting beside it
 */
bool CanHaveManual(const std::string& gamePath)
{
  if (gamePath.empty())
    return false;

  // A game inside an archive, or reached through a protocol that addresses
  // something other than a file, has no directory to sit a manual beside
  if (URIUtils::IsInArchive(gamePath) || URIUtils::IsInternetStream(gamePath))
    return false;

  // A game with no extension is left alone: replacing nothing would invent a
  // basename the player never chose
  if (URIUtils::GetExtension(gamePath).empty())
    return false;

  return true;
}

/*!
 * \brief The game's filename with its extension removed
 *
 * The extension is dropped literally rather than with RemoveExtension(), which
 * only strips extensions an installed add-on has registered - so it would
 * depend on which game add-ons happen to be present.
 */
std::string GetGameStem(const std::string& gamePath)
{
  std::string stem = URIUtils::GetFileName(gamePath);

  const size_t extension = stem.find_last_of('.');
  if (extension != std::string::npos)
    stem.erase(extension);

  return stem;
}

/*!
 * \brief Look through a folder for a manual whose name reduces to the game's
 *
 * \return The manual's path, or empty if the folder holds no match
 */
std::string FindByNormalisedName(const std::string& folder, const std::string& normalisedGame)
{
  if (normalisedGame.empty())
    return {};

  CFileItemList items;
  if (!XFILE::CDirectory::GetDirectory(folder, items, "", XFILE::DIR_FLAG_NO_FILE_DIRS))
    return {};

  // The extensions are tried in order of preference, so that a folder holding
  // both a PDF and a comic archive resolves the same way an exact match would
  for (const char* extension : MANUAL_EXTENSIONS)
  {
    for (int i = 0; i < items.Size(); ++i)
    {
      const CFileItemPtr& item = items[i];
      if (item->IsFolder())
        continue;

      const std::string candidate = item->GetPath();

      std::string candidateExtension = URIUtils::GetExtension(candidate);
      StringUtils::ToLower(candidateExtension);
      if (candidateExtension != extension)
        continue;

      if (CGameManual::NormaliseName(GetGameStem(candidate)) == normalisedGame)
        return candidate;
    }
  }

  return {};
}
} // namespace

std::string CGameManual::NormaliseName(const std::string& name)
{
  std::string result;
  result.reserve(name.size());

  // Depth rather than a flag, so that a nested tag closes correctly instead of
  // the first closing bracket ending the skip
  int depth = 0;

  for (const char c : name)
  {
    if (c == '(' || c == '[')
    {
      ++depth;
      continue;
    }

    if (c == ')' || c == ']')
    {
      if (depth > 0)
        --depth;
      continue;
    }

    if (depth > 0)
      continue;

    // Everything that is not a letter or a digit becomes a single space, so
    // that punctuation and spacing differences between two spellings of the
    // same title stop mattering
    if (std::isalnum(static_cast<unsigned char>(c)))
      result += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    else if (!result.empty() && result.back() != ' ')
      result += ' ';
  }

  StringUtils::Trim(result);

  return result;
}

std::vector<std::string> CGameManual::BuildManualPaths(const std::string& gamePath)
{
  if (!CanHaveManual(gamePath))
    return {};

  std::vector<std::string> paths;

  const std::string folder = URIUtils::GetDirectory(gamePath);
  const std::string subfolder = URIUtils::AddFileToFolder(folder, MANUAL_SUBFOLDER);
  const std::string stem = GetGameStem(gamePath);

  if (stem.empty())
    return {};

  // Beside the game first, since that is where a manual kept with one game
  // lives, then the shared folder
  for (const std::string& directory : {folder, subfolder})
  {
    for (const char* extension : MANUAL_EXTENSIONS)
      paths.emplace_back(URIUtils::AddFileToFolder(directory, stem + extension));
  }

  return paths;
}

std::string CGameManual::GetManualPath(const std::string& gamePath)
{
  // An exact name match costs only a handful of existence checks, so it is
  // tried in full before any directory is listed
  for (const std::string& candidate : BuildManualPaths(gamePath))
  {
    if (XFILE::CFile::Exists(candidate))
      return candidate;
  }

  if (!CanHaveManual(gamePath))
    return {};

  // Nothing matched exactly, so the folders are listed and a manual whose name
  // differs only by its region and revision tags is accepted
  const std::string normalised = NormaliseName(GetGameStem(gamePath));
  const std::string folder = URIUtils::GetDirectory(gamePath);

  for (const std::string& directory : {folder, URIUtils::AddFileToFolder(folder, MANUAL_SUBFOLDER)})
  {
    const std::string found = FindByNormalisedName(directory, normalised);
    if (!found.empty())
      return found;
  }

  return {};
}

std::string CGameManual::GetManualPath(const CFileItem& item)
{
  return GetManualPath(item.GetDynPath());
}
