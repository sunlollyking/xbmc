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
 * \brief The artwork a game can have beside it
 *
 * Named for what the images are, not for the film equivalents: a game is sold
 * in a box, and its front is what a player recognises it by.
 */
constexpr std::array<const char*, 6> ART_TYPES = {"boxfront", "boxback",  "fanart",
                                                  "clearlogo", "banner", "snap"};

//! \brief Find "<game>-<type>.jpg", then the same as a .png
std::string FindTypedArt(const CFileItem& item, const std::string& type)
{
  for (const char* extension : ART_EXTENSIONS)
  {
    const std::string art = item.FindLocalArt(type + extension, false);
    if (!art.empty())
      return art;
  }

  return {};
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
