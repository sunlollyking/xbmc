/*
 *  Copyright (C) 2026 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#include "GameThumbLoader.h"

#include "FileItem.h"
#include "games/tags/GameInfoTagLoader.h"
#include "utils/FileUtils.h"
#include "utils/URIUtils.h"

#include <array>
#include <string>

using namespace KODI;
using namespace GAME;

namespace
{
//! The extensions art is looked for in, in order
constexpr std::array<const char*, 2> ART_EXTENSIONS = {".jpg", ".png"};

/*!
 * \brief The artwork a game can have
 *
 * Named for what the images are, not for the film equivalents: a game is sold
 * in a box, and its front is what a player recognises it by.
 */
constexpr std::array<const char*, 6> ART_TYPES = {"boxfront", "boxback", "fanart",
                                                  "clearlogo", "banner", "snap"};

/*!
 * \brief Where a collection may keep its artwork instead of beside the games
 *
 * A platform folder holding a few thousand games doubles in length once every
 * one of them has a picture next to it, so the art may be kept together in a
 * folder of its own. Spellings vary because people make these by hand.
 */
constexpr std::array<const char*, 3> ART_SUBFOLDERS = {"Box Art", "boxart", "box art"};

/*!
 * \brief The game's filename with its extension removed
 *
 * Dropped literally rather than with RemoveExtension(), which only strips
 * extensions an installed add-on has registered - so it would depend on which
 * game add-ons happen to be present.
 */
std::string GameStem(const std::string& path)
{
  std::string name = URIUtils::GetFileName(path);

  const size_t dot = name.rfind('.');
  if (dot != std::string::npos)
    name.erase(dot);

  return name;
}

//! \brief Art kept in a folder of its own, rather than beside the game
std::string FindArtInSubfolder(const CFileItem& item, const std::string& suffix)
{
  const std::string parent = URIUtils::GetDirectory(item.GetPath());
  const std::string stem = GameStem(item.GetPath());
  if (parent.empty() || stem.empty())
    return {};

  for (const char* subfolder : ART_SUBFOLDERS)
  {
    const std::string folder = URIUtils::AddFileToFolder(parent, subfolder);

    for (const char* extension : ART_EXTENSIONS)
    {
      const std::string art = URIUtils::AddFileToFolder(folder, stem + suffix + extension);
      if (CFileUtils::Exists(art))
        return art;
    }
  }

  return {};
}

//! \brief Find "<game>-<type>.jpg", then the same as a .png, beside or apart
std::string FindTypedArt(const CFileItem& item, const std::string& type)
{
  for (const char* extension : ART_EXTENSIONS)
  {
    const std::string art = item.FindLocalArt(type + extension, false);
    if (!art.empty())
      return art;
  }

  return FindArtInSubfolder(item, "-" + type);
}
} // namespace

bool CGameThumbLoader::LoadItemCached(CFileItem* item)
{
  if (item == nullptr || item->IsParentFolder())
    return false;

  // Handles the folder art a platform carries, and the .tbn a game may have
  bool artLoaded = CProgramThumbLoader::LoadItemCached(item);

  if (!item->IsFolder())
    artLoaded |= LoadLocalArt(*item);

  // Asked before loading rather than after, because reading the tag creates one
  // on the item and anything carrying a game tag answers IsGame(). Most of a
  // listing is not games, and they must not start claiming to be.
  if (!CGameInfoTagLoader::HasNFO(*item))
    return artLoaded;

  return item->LoadGameTag() || artLoaded;
}

bool CGameThumbLoader::LoadLocalArt(CFileItem& item)
{
  bool found = false;

  for (const char* type : ART_TYPES)
  {
    if (item.HasArt(type))
      continue;

    const std::string art = FindTypedArt(item, type);
    if (art.empty())
      continue;

    item.SetArt(type, art);
    found = true;
  }

  // The art folder is named for what it holds, so an image in it that carries
  // no type is the box front
  if (!item.HasArt("boxfront"))
  {
    const std::string art = FindArtInSubfolder(item, "");
    if (!art.empty())
    {
      item.SetArt("boxfront", art);
      found = true;
    }
  }

  // Skins built for films ask for a poster, and the box front is the picture
  // that slot wants. Filled rather than looked for: nothing writes a poster
  // for a game, so leaving it empty only hides artwork that is right there.
  if (item.HasArt("boxfront") && !item.HasArt("poster"))
    item.SetArt("poster", item.GetArt("boxfront"));

  if (item.HasArt("thumb"))
    return found;

  // A skin that asks for nothing in particular gets the box front, which is
  // what CProgramThumbLoader's .tbn would have been
  if (item.HasArt("boxfront"))
  {
    item.SetArt("thumb", item.GetArt("boxfront"));
    return true;
  }

  // A picture simply left beside the game, which is how a collection that has
  // never been through a scraper tends to look
  for (const char* extension : ART_EXTENSIONS)
  {
    const std::string beside = URIUtils::ReplaceExtension(item.GetPath(), extension);
    if (CFileUtils::Exists(beside))
    {
      item.SetArt("thumb", beside);
      return true;
    }
  }

  return found;
}
