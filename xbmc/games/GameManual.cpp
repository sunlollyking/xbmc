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
#include "filesystem/Directory.h"
#include "utils/StringUtils.h"
#include "utils/URIUtils.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <vector>

using namespace KODI::GAME;

namespace
{
//! Preferred first: a PDF is a single file with real page structure, where a
//! comic archive is a bag of images that has to be ordered by filename
constexpr std::array<const char*, 3> MANUAL_EXTENSIONS = {".pdf", ".cbz", ".cbr"};

//! Where a manual is kept if it is not beside the game
//!
//! Both spellings are looked in. The collections people already have use the
//! capitalised one, and a case-sensitive filesystem makes that a different
//! folder that would otherwise never be read.
constexpr std::array<const char*, 2> MANUAL_SUBFOLDERS = {"manuals", "Manuals"};

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
} // namespace

bool CManualIndex::HasManual(const std::string& gamePath)
{
  if (!CanHaveManual(gamePath))
    return false;

  const std::string normalised = CGameManual::NormaliseName(GetGameStem(gamePath));
  if (normalised.empty())
    return false;

  return Names(URIUtils::GetDirectory(gamePath)).contains(normalised);
}

const std::set<std::string>& CManualIndex::Names(const std::string& folder)
{
  const auto seen = m_folders.find(folder);
  if (seen != m_folders.end())
    return seen->second;

  // Inserted before the folders are read, so that one that cannot be read is
  // remembered as holding nothing rather than being tried again per game
  std::set<std::string>& names = m_folders[folder];

  // Filtered by the directory layer rather than here: a platform folder can
  // hold thousands of games, and without a mask every one of them becomes an
  // item just to be discarded
  std::string mask;
  for (const char* extension : MANUAL_EXTENSIONS)
    mask += std::string(extension) + "|";
  mask.pop_back();

  // The mask leaves folders in, so the listing also says which manual
  // subfolders exist. Asking for one that doesn't logs an error, and most game
  // folders have neither.
  std::vector<std::string> directories{folder};
  for (size_t i = 0; i < directories.size(); ++i)
  {
    CFileItemList items;
    if (!XFILE::CDirectory::GetDirectory(directories[i], items, mask,
                                         XFILE::DIR_FLAG_NO_FILE_DIRS))
      continue;

    for (int j = 0; j < items.Size(); ++j)
    {
      const CFileItemPtr& item = items[j];
      if (!item->IsFolder())
      {
        names.insert(CGameManual::NormaliseName(GetGameStem(item->GetPath())));
      }
      else if (i == 0)
      {
        std::string path = item->GetPath();
        URIUtils::RemoveSlashAtEnd(path);
        const std::string name = URIUtils::GetFileName(path);
        if (std::ranges::find(MANUAL_SUBFOLDERS, name) != MANUAL_SUBFOLDERS.end())
          directories.emplace_back(item->GetPath());
      }
    }
  }

  return names;
}

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
