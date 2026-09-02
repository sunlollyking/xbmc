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

using namespace KODI;
using namespace GAME;

bool CGameThumbLoader::LoadItemCached(CFileItem* item)
{
  if (item == nullptr || item->IsParentFolder())
    return false;

  const bool artLoaded = CProgramThumbLoader::LoadItemCached(item);

  // Asked before loading rather than after, because reading the tag creates one
  // on the item and anything carrying a game tag answers IsGame(). Most of a
  // listing is not games, and they must not start claiming to be.
  if (!CGameInfoTagLoader::HasNFO(*item))
    return artLoaded;

  return item->LoadGameTag() || artLoaded;
}
